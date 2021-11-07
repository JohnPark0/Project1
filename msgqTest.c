#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <sys/time.h>

void Parent(void);
void Child(void);

// 데이터 중 자식 프로세스 정보
struct cdata {
    int cpid;
    int iotime;
};

// 메세지 큐에 넣을 데이터 
struct msgbuf {
    long mtype;
    struct cdata data;
};

int main(int argc, char** argv) {
    int pid = fork();
 
    if(pid > 0) {// 부모 프로세스
        printf("Parents Process: %d, %d\n", getpid(), pid);
        Parent();
	exit(0);
    }
    else if(pid == 0) {// 자식 프로세스
        printf("Child Process: %d, %d\n", getpid(), pid);
        Child();
	exit(0);
    }
    else if(pid == -1) {
        perror("fork error");
        exit(0);
    }
    return 0;
}

void Parent(void) {
    int msgq;
    int ret;
    int pkey = 0x1000;// 큐 키값.
    struct msgbuf msg;

    msgq = msgget(pkey, IPC_CREAT | 0666);
    // 큐 키값이 0x1000인 메시지 큐 생성, 단 이미 생성되어있으므로
    // 메시지 큐 id만 반환한다.

    printf("msgq id: %d\n", msgq);	
    
    memset(&msg, 0, sizeof(msg));
    if (ret = msgrcv(msgq, (void*)&msg, sizeof(msg), 0, 0) == -1) {
	perror("msgrcv error");
	exit(1);
    }// 메세지 큐에 담긴 데이터를 꺼낸다.

    printf("msgrcv: %d\n", ret);
    printf("msg.mtype: %ld\n", msg.mtype);
    printf("msg.data.cpid: %d\n", msg.data.cpid);
    printf("msg.data.iotime: %d\n", msg.data.iotime);
    return;
}

void Child(void) {
    int msgq;
    int ret;
    int ckey = 0x1000;// 큐 키값
    struct msgbuf msg;

    msgq = msgget(ckey, IPC_CREAT | 0666);// 큐 키값이  0x1000인 메세지 큐 생성
    printf("msgq id: %d\n", msgq);

    memset(&msg, 0, sizeof(msg));
    msg.mtype = 1;// mtype은 반드시 long에 0보다 커야한다.
    msg.data.cpid = getpid();// 자식 프로세스의 pid
    msg.data.iotime = 100;// 자식 프로세스의 io burst time

    // msgq에 msg 구조체를  데이터로서  보낸다.
    if (ret = msgsnd(msgq, (void*)&msg, sizeof(struct cdata), 0) == -1) {
	perror("msgsnd error");
	exit(1);
    }
    printf("msgsnd ret: %d\n", ret);
    return;
}
