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
#include <time.h>

void readFile(void);
void parent(void);
void child(int* ckey);
void pmsgSnd(int curProc);
void pmsgRcv(int curProc);
void cmsgSnd(int* ckey);
void cmsgRcv(int* ckey);
void signalHandler(int signo);

int curProc = 1;// 현재 실행 중인 자식 프로세스.
int tickCount, quantumCount = 0;// 타임 틱 카운터.
int cpid[3];// parent holds child process pid array.
int cqid[3];// parent holds message queue id array.
int* ckey[3];// child holds child process key array.

struct data {
    int pid;// 자식 프로세스의 id.
    int cpuTime;
    int ioTime;// 자식의 io time.
    int timeQuantum;
};// 메세지 큐에 넣을  자식 프로세스의  데이터.

struct msgbuf {
    long mtype;// 무조건 있어야 하는 mtype.
    struct data mdata;
};// 메세지 큐에 넣을 데이터.

int main(int argc, char** argv) {
    printf("PROCESS\tNUMBER\tPID\tKEY\n");
    for (int i = 0; i < 3; i++) {
        int ret = fork();

        if (ret > 0) {
            if (i == 0)
                printf("parent\t%d\t%d\n", i + 1, getpid());
            cpid[i] = ret;

            /* 여기에 메세지 큐를 생성하는 코드를 작성하세요. */
        }
        else if (ret == 0) {
            srand((unsigned int)time(NULL));

            /* 여기에 파일에서 cpu, io를 읽거나 랜덤으로 생성하는 코드를 작성하세요. */
            int pid = getpid();
            int cpuTime = rand() % 100;
            int ioTime = rand() % 100;
            int key = 0x1000 * (i + 1);// 자식 프로세스마다 고유한 키를 갖는다.
            ckey[i] = &key;// 0x1000, 0x2000, 0x3000, ...

            printf("child\t%d\t%d\t%d\n", i + 1, pid, key);
            child(ckey[i]);
            exit(0);
        }
        else if (ret == -1) {
            perror("fork error");
            exit(0);
        }
    }
    parent();
    exit(0);
    return 0;
}

void readFile(void) {
    return;
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
    new_itimer.it_value.tv_sec = 2;// 타이머는 3초 후에 시작한다.
    new_itimer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &new_itimer, &old_itimer);

    sleep(1);
    printf("TICK\tPROCESS\tTYPE\tPID\tCPU TIME\tIO TIME\n");
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
        printf("%d\t%d\t", tickCount, curProc);
        pmsgSnd(curProc);
        pmsgRcv(curProc);// 자식에게 메세지를 보내고 받는다.
        tickCount++;
        break;

    case 2:// 세 번째 타임 틱
        printf("%d\t%d\t", tickCount, curProc);
        pmsgSnd(curProc);
        pmsgRcv(curProc);
        curProc++;// 다음 자식 프로세스로 타겟을 변경한다.
        tickCount = 0;// 틱을 초기화
        break;
    }

    if (curProc >= 4) {// 자식 프로세스를 종료한다.
        for (int i = 0; i < 3; i++) {
            kill(cpid[i], SIGKILL);
            msgctl(cqid[i], IPC_RMID, NULL);// 메시지 큐를 삭제한다.
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
    cqid[curProc-1] = qid;
    memset(&msg, 0, sizeof(msg));

    msg.mtype = 1;
    msg.mdata.pid = 0;
    msg.mdata.cpuTime = 0;
    msg.mdata.ioTime = 0;

    if (ret = msgsnd(qid, (void*)&msg, sizeof(struct data), 0) == -1) {
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

// 자식이 부모에게 자신의 데이터가 담긴 메시지를 보낸다.
void cmsgSnd(int* ckey) {
    int qid;
    int ret;
    int key = *ckey;// 자식 프로세스 고유의 키 값.
    struct msgbuf msg;

    qid = msgget(key, IPC_CREAT | 0666);
    memset(&msg, 0, sizeof(msg));

    msg.mtype = 1;
    msg.mdata.pid = getpid();
    msg.mdata.cpuTime = 100;
    msg.mdata.ioTime = 100;// io time을 부모에게 보낸다.

    if (ret = msgsnd(qid, (void*)&msg, sizeof(struct data), 0) == -1) {
        perror("msgsnd error");
        exit(1);
    }
    return;
}

// 자식이 보낸 메시지를 받은 부모는 그 데이터를 출력한다.
void pmsgRcv(int curProc) {
    int qid;
    int ret;
    int key = 0x1000 * curProc;
    struct msgbuf msg;

    qid = msgget(key, IPC_CREAT | 0666);
    memset(&msg, 0, sizeof(msg));

    if (ret = msgrcv(qid, (void*)&msg, sizeof(msg), 0, 0) == -1) {
        perror("msgrcv error");
        exit(1);
    }
    printf("%ld\t%d\t%d\t\t%d\n", msg.mtype, msg.mdata.pid, msg.mdata.cpuTime, msg.mdata.ioTime);
    return;
}
