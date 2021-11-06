#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>

#define MAX_PROC 10
#define TICK_TIME 1000000		//micro sec 1000000 = 1sec
#define QUANTUM_TIME 3

typedef struct node {
	struct node* next;
	int proc_num;
} node;

typedef struct list {
	node* head;
	node* tail;
	int list_num;
} list;

struct data {
	int pid;// 자식 프로세스의 id.
	int cpuTime;
	int ioTime;// 자식의 io time.
};// 메세지 큐에 넣을  자식 프로세스의  데이터.

struct msgbuf {
	long mtype;// 무조건 있어야 하는 mtype.
	struct data mdata;
};// 메세지 큐에 넣을 데이터.

//Function Define
void signal_bustend(int signo);
void signal_decrease(int signo);
void addnode(list* list, int proc_num);
int delnode(list* list, node* return_node);
void inilist(list* list);
void signal_handler(int signo);

//Global Parameter
int child_proc_num = 0;		//편의를 위해 자식 프로세스 번호지정
//child_proc_num 을 통해 각 자식 프로세스가 bust_time[child_proc_num]으로 남은 버스트 타임 계산

int bust_time[MAX_PROC];
int iobust_time[MAX_PROC];
int pids[MAX_PROC];
int parents_pid;

list* WaitingQ;
list* ReadyQ;
node* RunningQ;

//합칠 코드
void pmsgSnd(int curProc);
void pmsgRcv(int curProc);
void cmsgSnd(int* ckey);
void cmsgRcv(int* ckey);
void signalHandler(int signo);

int curProc = 1;// 현재 실행 중인 자식 프로세스.
int tickCount = 0; //타임 카운터
int quantumCount = 0;//틱 카운터.
int cpid[3];// parent holds child process pid array.
int cqid[3];// parent holds message queue id array.
int* ckey[3];// child holds child process key array.
//합칠 코드

int main(int argc, char* arg[]) {
	int ret;
	int temp;

	struct sigaction new_sa;
	memset(&new_sa, 0, sizeof(new_sa));
	new_sa.sa_handler = &signal_handler;
	sigaction(SIGALRM, &new_sa, NULL);

	struct itimerval new_itimer, old_itimer;
	new_itimer.it_interval.tv_sec = 1;
	//new_itimer.it_interval.tv_usec = TICK_TIME;
	new_itimer.it_value.tv_sec = 1;
	new_itimer.it_value.tv_usec = 0;

	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = &signal_decrease;
	sigaction(SIGUSR1, &act, NULL);

	struct sigaction act2;
	memset(&act2, 0, sizeof(act2));
	act2.sa_handler = &signal_bustend;
	sigaction(SIGUSR2, &act2, NULL);

	ReadyQ = malloc(sizeof(list));
	WaitingQ = malloc(sizeof(list));
	RunningQ = malloc(sizeof(node));

	inilist(ReadyQ);
	inilist(WaitingQ);

	//CPU bust 타임 임의 세팅
	//추가 할 것 : setting.txt 파일등으로 미리 세팅된 파일을 불러와서 저장 or 세팅파일을 프로그램 시작시 지정하지 않으면 랜덤으로 생성(고려)
	bust_time[0] = 100;
	bust_time[1] = 500;
	bust_time[2] = 100;
	bust_time[3] = 200;
	bust_time[4] = 100;
	bust_time[5] = 300;
	bust_time[6] = 700;
	bust_time[7] = 400;
	bust_time[8] = 100;
	bust_time[9] = 600;

	//IO bust 타임 임의 세팅
	iobust_time[0] = 100;
	iobust_time[1] = 500;
	iobust_time[2] = 100;
	iobust_time[3] = 200;
	iobust_time[4] = 100;
	iobust_time[5] = 300;
	iobust_time[6] = 700;
	iobust_time[7] = 400;
	iobust_time[8] = 100;
	iobust_time[9] = 600;

	parents_pid = getpid();

	//초기 자식 프로세스 생성 구간 - 부모 프로세스(처음 1번만 실행)
	for (int i = 0; i < 10; i++) {
		//sleep(1);
		ret = fork();
		if (ret > 0) {								//부모 프로세스
			child_proc_num++;
			pids[i] = ret;
			addnode(ReadyQ, i);

		}
		//초기 자식 프로세스 생성 구간

			//자식 프로세스 코드 구간 - 부모 프로세스가 kill 시그널 혹은 일정 자식 프로세스의 일정 조건까지 반복 후 종료
		else if (ret == 0) {						//자식 프로세스
			//printf("pid[%d] = proc num [%d]\n",getpid(),child_proc_num);		//디버그 용
			printf("pid[%d] : stop\n", getpid());
			//raise(SIGSTOP);
			kill(getpid(), SIGSTOP);

			while (1) {								//루프가 없으면 한번 실행 후 자식 프로세스가 다른 자식 프로세스 무한 생성
				printf("pid[%d] : work\n", getpid());

				temp = bust_time[child_proc_num];

				if (bust_time[child_proc_num] - 100 == 0) {
					bust_time[child_proc_num] = bust_time[child_proc_num] - 100;
					printf("pids[%d] = cpu_bust decrease %d - 100 = %d\n", getpid(), temp, bust_time[child_proc_num]);
					kill(parents_pid, SIGUSR2);
				}
				else {
					bust_time[child_proc_num] = bust_time[child_proc_num] - 100;
					printf("pids[%d] = cpu_bust decrease %d - 100 = %d\n", getpid(), temp, bust_time[child_proc_num]);
					kill(parents_pid, SIGUSR1);
				}
				printf("pid[%d] : work done\n", getpid());
				kill(getpid(), SIGSTOP);
			}
		}
		//자식 프로세스 코드 구간
	}

	//부모 프로세스 코드 구간 - 시그널 통해 자식 프로세스 통제 및 종료

	delnode(ReadyQ, RunningQ);
	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
	while(1) {
		//addnode(ReadyQ, RunningQ->proc_num);

		if (ReadyQ->head == NULL && ReadyQ->tail == NULL) {
			break;
		}
	}

	//추가할 것 : 프로그램의 종료 조건(모든 자식 프로세스의 CPU bust가 0)
	for (int i = 0; i < 10; i++) {
		kill(pids[i], SIGKILL);
		printf("sigkill\n");
	}
	//부모 프로세스 코드 구간

	return 0;
}

void signal_handler(int signo)				//부모 프로세스에서 작동 SIGALRM
{
	int child_proc;
	child_proc = pids[RunningQ->proc_num];		//Running Queue의 자식 프로세스 pid 지정
	kill(pids[RunningQ->proc_num], SIGCONT);

}

void signal_decrease(int signo) {			//실행중인 자식 프로세스에서 작동 SIGUSR1
	if (tickCount < QUANTUM_TIME-1) {
		tickCount++;
	}
	else {
		addnode(ReadyQ, RunningQ->proc_num);
		delnode(ReadyQ, RunningQ);
		tickCount = 0;
	}
}

void signal_bustend(int signo) {			//자식 프로세스 -> 부모 프로세스 시그널 전송 테스트 SIGUSR2
	printf("SIGUSR2 called1\n");
	addnode(WaitingQ, RunningQ->proc_num);
	delnode(ReadyQ, RunningQ);

	tickCount = 0;
	printf("SIGUSR2 called2\n");
}

//합칠 코드
void pmsgSnd(int curProc) {
	int qid;
	int ret;
	int key = 0x1000 * curProc;
	struct msgbuf msg;

	qid = msgget(key, IPC_CREAT | 0666);
	cqid[curProc - 1] = qid;
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

// 자식이 부모에게 자신의 데이터가 담긴  메시지를 보낸다.
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




//헤더로 분리할 함수들 - 1	(가명 list.h)

void inilist(list* list) {
	list->head = NULL;
	list->tail = NULL;
	list->list_num = 0;
}

void addnode(list* list, int proc_num) {
	node* addnode = (node*)malloc(sizeof(node));
	addnode->next = NULL;
	addnode->proc_num = proc_num;
	if (list->head == NULL) {				//첫번째 노드일때 list->head = list->tail
		list->head = addnode;
		list->tail = addnode;
		printf("Add first node\n");
	}
	else {									//첫번째 노드가 아니면 마지막 노드 업데이트
		list->tail->next = addnode;
		list->tail = addnode;
		printf("Add node\n");
	}
}

int delnode(list* list, node* return_node) {
	int proc_num;
	node* delnode;

	if (list->head == NULL) {				//비어있는 리스트 삭제시 예외처리
		printf("There is no node to delete\n");
		return 0;							//실패
	}
	delnode = list->head;
	proc_num = list->head->proc_num;
	if (list->head->next == NULL) {			//마지막 노드를 지우면 list를 NULL로 초기화
		list->head = NULL;
		list->tail = NULL;
		printf("Delete last node\n");
	}
	else {
		list->head = delnode->next;
		printf("Delete node\n");
	}
	*return_node = *delnode;
	free(delnode);

	return 1;								//성공
}
//헤더로 분리할 함수들 - 1
