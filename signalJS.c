#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <sched.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

void parent(void);
void child(int* ckey);
void pmsgSnd(int curProc);
void pmsgRcv(int curProc);
void cmsgSnd(int* ckey);
void cmsgRcv(int* ckey);
void signalHandler(int signo);

int curProc = 1;// 현재 실행 중인 자식 프로세스.
int tickCount = 0;// 타임 틱 카운터.
int cpid[3];// child process pid array.
int* ckey[3];// child process key array.

struct cdata {
    bool isZero;// cpu time이 0인가?
    int cpid;// 자식 프로세스의 id.
    int iotime;// 자식의 io time.
};// 메세지 큐에 넣을  자식 프로세스의  데이터.

struct msgbuf {
    long mtype;// 무조건 있어야 하는 mtype.
    struct cdata data;
};// 메세지 큐에 넣을 데이터.

int main(int argc, char** argv) {

    for (int i = 0; i < 3; i++) {
        int pid = fork();

        if (pid > 0) {
            printf("Parents Process: %d\n", getpid());
            cpid[i] = pid;
        }
        else if (pid == 0) {
            int key = 0x1000 * (i + 1);// 자식 프로세스마다 고유한 키를 갖는다.
            ckey[i] = &key;// 0x1000, 0x2000, 0x3000, ...

            printf("Child Process: %d\n", getpid());
            printf("ckey is %d\n", *ckey[i]);
            child(ckey[i]);
            exit(0);
        }
        else if (pid == -1) {
            perror("fork error");
            exit(0);
        }
    }
    parent();
    exit(0);
    return 0;
}

void parent(void) {
    struct sigaction old_sa;
    struct sigaction new_sa;
    struct itimerval new_itimer;
    struct itimerval old_itimer;

    memset(&new_sa, 0, sizeof(new_sa));
    new_sa.sa_handler = &signalHandler;
    sigaction(SIGALRM, &new_sa, &old_sa);

    new_itimer.it_interval.tv_sec = 1;// 타이머 간격은 1초이다.
    new_itimer.it_interval.tv_usec = 0;
    new_itimer.it_value.tv_sec = 3;// 타이머는 3초 후에 시작한다.
    new_itimer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &new_itimer, &old_itimer);

    while (1);
    return;
}

void child(int* ckey) {
    while (1) {
        cmsgRcv(ckey);// 부모가 메시지를 보낼 때까지 기다린다.
        cmsgSnd(ckey);
    }
}

void signalHandler(int signo) {// 타임 틱이 발생했을 때 실행되는 함수이다.
    switch (tickCount) {
    case 0:// 첫 번째 타임 틱
    case 1:// 두 번째 타임 틱
        printf("[%d tick, no.%d proc] \n", tickCount, curProc);
        pmsgSnd(curProc);
        pmsgRcv(curProc);// 자식에게 메세지를 보내고 받는다.
        tickCount++;
        break;

    case 2:// 세 번째 타임 틱
        printf("[%d tick, no.%d proc] \n", tickCount, curProc);
        pmsgSnd(curProc);
        pmsgRcv(curProc);
        curProc++;// 다음 자식 프로세스로 타겟을 변경한다.
        tickCount = 0;// 틱을 초기화
        break;
    }

    if (curProc >= 4) {// 자식 프로세스를 종료한다.
        for (int i = 0; i < 3; i++) {
            kill(cpid[i], SIGKILL);
        }
        exit(0);
    }
    return;
}

// 부모가 자식에게 메시지를 보낸다.
void pmsgSnd(int curProc) {
    int qid;
    int ret;
    int key = 0x1000 * curProc;
    struct msgbuf msg;

    qid = msgget(key, IPC_CREAT | 0666);
    memset(&msg, 0, sizeof(msg));

    msg.mtype = 1;
    msg.data.isZero = false;
    msg.data.cpid = 0;
    msg.data.iotime = 0;

    if (ret = msgsnd(qid, (void*)&msg, sizeof(struct cdata), 0) == -1) {
        perror("pmsgsnd error");
        exit(1);
    }
    return;
}

// 부모가 보낸 메시지를 자식이 받는다.
void cmsgRcv(int* ckey) {
    int qid;
    int ret;
    int key = *ckey;// 자식 프로세스 고유의 키 값.
    struct msgbuf msg;

    qid = msgget(key, IPC_CREAT | 0666);
    memset(&msg, 0, sizeof(msg));

    if (ret = msgrcv(qid, (void*)&msg, sizeof(msg), 0, 0) == -1) {
        perror("cmsgrcv error");
        exit(1);
    }
    return;
}

// 자식이 부모에게 자신의 데이터가 담긴  메시지를 보낸다.
void cmsgSnd(int* ckey) {
    int msgq;
    int ret;
    int key = *ckey;// 자식 프로세스 고유의 키 값.
    struct msgbuf msg;

    msgq = msgget(key, IPC_CREAT | 0666);
    memset(&msg, 0, sizeof(msg));

    msg.mtype = 1;
    msg.data.isZero = false;
    msg.data.cpid = getpid();
    msg.data.iotime = 100;// io time을 부모에게 보낸다.

    if (ret = msgsnd(msgq, (void*)&msg, sizeof(struct cdata), 0) == -1) {
        perror("msgsnd error");
        exit(1);
    }
    return;
}

// 자식이 보낸 메시지를 받은 부모는 그 데이터를 출력한다.
void pmsgRcv(int curProc) {
    int msgq;
    int ret;
    int key = 0x1000 * curProc;
    struct msgbuf msg;

    msgq = msgget(key, IPC_CREAT | 0666);
    memset(&msg, 0, sizeof(msg));

    if (ret = msgrcv(msgq, (void*)&msg, sizeof(msg), 0, 0) == -1) {
        perror("msgrcv error");
        exit(1);
    }
    printf("msg.mtype: %ld\n", msg.mtype);// 자식으로부터  받은 데이터.
    printf("msg.data.isZero: %d\n", msg.data.isZero);
    printf("msg.data.cpid: %d\n", msg.data.cpid);
    printf("msg.data.iotime: %d\n", msg.data.iotime);
    return;
}
