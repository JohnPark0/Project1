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
void child(void);
void pmsgSnd(void);
void pmsgRcv(void);
void cmsgSnd(void);
void cmsgRcv(void);
void signalHandler(int signo);

int tickCount = 0;
int cpid[1];// child process pid array.

struct cdata {
    bool isZero;
    int cpid;
    int iotime;
};

struct msgbuf {
    long mtype;
    struct cdata data;
};

int main(int argc, char** argv) {
    for (int i = 0; i < 1; i++) {
        int pid = fork();

        if (pid > 0) {
            printf("Parents Process: %d\n", getpid());
        }
        else if (pid == 0) {
            printf("Child Process: %d\n", getpid());
            sleep(1);
            cpid[i] = pid;
            child();
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

    new_itimer.it_interval.tv_sec = 1;
    new_itimer.it_interval.tv_usec = 0;
    new_itimer.it_value.tv_sec = 3;
    new_itimer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &new_itimer, &old_itimer);

    while (1);
    return;
}

void child(void) {
    while (1) {
        cmsgRcv();
        cmsgSnd();
    }
}

void signalHandler(int signo) {
    switch (tickCount) {
    case 0:
    case 1:
        printf("[%d tick]\n", tickCount);
        pmsgSnd();
        pmsgRcv();
        tickCount++;
        break;

    case 2:
        printf("[%d tick]\n", tickCount);
        pmsgSnd();
        pmsgRcv();
        tickCount = 0;
        kill(cpid[0], SIGKILL);
        // exit(0);
        break;
    }
    return;
}

// 어떻게 자식 프로세스마다 다른 key 값을 보유할 수 있을까?

void pmsgSnd(void) {
    int qid;
    int ret;
    int ckey = 0x1000;
    struct msgbuf msg;

    qid = msgget(ckey, IPC_CREAT | 0666);
    memset(&msg, 0, sizeof(msg));

    msg.mtype = 1;
    msg.data.isZero = false;
    msg.data.cpid = 0;
    msg.data.iotime = 0;

    if (ret = msgsnd(qid, (void*)&msg, sizeof(struct cdata), 0) == -1) {
        perror("msgsnd error");
        exit(1);
    }
    return;
}

void cmsgRcv(void) {
    int qid;
    int ret;
    int ckey = 0x1000;
    struct msgbuf msg;

    qid = msgget(ckey, IPC_CREAT | 0666);
    memset(&msg, 0, sizeof(msg));

    if (ret = msgrcv(qid, (void*)&msg, sizeof(msg), 0, 0) == -1) {
        perror("msgrcv error");
        exit(1);
    }
    return;
}

void cmsgSnd(void) {
    int msgq;
    int ret;
    int ckey = 0x1000;
    struct msgbuf msg;

    msgq = msgget(ckey, IPC_CREAT | 0666);
    memset(&msg, 0, sizeof(msg));

    msg.mtype = 1;
    msg.data.isZero = false;
    msg.data.cpid = getpid();
    msg.data.iotime = 100;

    if (ret = msgsnd(msgq, (void*)&msg, sizeof(struct cdata), 0) == -1) {
        perror("msgsnd error");
        exit(1);
    }
    return;
}

void pmsgRcv(void) {
    int msgq;
    int ret;
    int ckey = 0x1000;
    struct msgbuf msg;

    msgq = msgget(ckey, IPC_CREAT | 0666);
    memset(&msg, 0, sizeof(msg));

    if (ret = msgrcv(msgq, (void*)&msg, sizeof(msg), 0, 0) == -1) {
        perror("msgrcv error");
        exit(1);
    }
    printf("msg.mtype: %ld\n", msg.mtype);
    printf("msg.data.isZero: %d\n", msg.data.isZero);
    printf("msg.data.cpid: %d\n", msg.data.cpid);
    printf("msg.data.iotime: %d\n", msg.data.iotime);
    return;
}
