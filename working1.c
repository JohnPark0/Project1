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
#define TICK_TIME 10000		//micro sec 1000000 = 1sec
#define QUANTUM_TIME 2

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
void writenode(list* list, FILE* fp, char* listname);
void writeallnode(list* ready, list* waiting, node* running, FILE* fp);

void inilist(list* list);
void signal_handler(int signo);

//Global Parameter
int child_proc_num = 0;		//편의를 위해 자식 프로세스 번호지정
//child_proc_num 을 통해 각 자식 프로세스가 bust_time[child_proc_num]으로 남은 버스트 타임 계산

int bust_time[MAX_PROC];
int iobust_time[MAX_PROC];
int pids[MAX_PROC];
int parents_pid;
int WaitingQ_num;

list* WaitingQ;
list* ReadyQ;
node* RunningQ;
node* IORunningQ;

FILE* fp;

int running_ticks = 0;
int tickCount = 0; //타임 카운터
//int messagechk[MAX_PROC];

//합칠 코드
void pmsgSnd(int curProc);
//void pmsgRcv(int curProc);
//void cmsgSnd(int* ckey);
void cmsgRcv(int* ckey);

void cmsgSnd(int ckey, int iobust_time);
void pmsgRcv(int curProc, int* iobust_time);

void signalHandler(int signo);

int curProc = 1;// 현재 실행 중인 자식 프로세스.
int quantumCount = 0;//틱 카운터.
int cpid[MAX_PROC];// parent holds child process pid array.
int cqid[MAX_PROC];// parent holds message queue id array.
int ckey[MAX_PROC];// child holds child process key array.
//합칠 코드

int main(int argc, char* arg[]) {
	int ret;
	int temp;

	fp = fopen("schedule_dump.txt", "w");

	//memset(&messagechk, 0, sizeof(messagechk));

	struct sigaction new_sa;
	memset(&new_sa, 0, sizeof(new_sa));
	new_sa.sa_handler = &signal_handler;
	sigaction(SIGALRM, &new_sa, NULL);

	struct itimerval new_itimer, old_itimer;
	if (TICK_TIME > 999999) {
		new_itimer.it_interval.tv_sec = TICK_TIME/1000000;
	}
	//new_itimer.it_interval.tv_sec = 0;
	new_itimer.it_interval.tv_usec = TICK_TIME % 1000000;
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
	IORunningQ = malloc(sizeof(node));

	inilist(ReadyQ);
	inilist(WaitingQ);

	for (int i = 0; i < MAX_PROC; i++) {						//이전에 사용했던 메시지 큐를 비워 에러 방지(무한대로 돌기때문에 프로그램 끝날때 초기화 불가능)
		ckey[i] = 0x1000 * (i + 1);
		cqid[i] = msgget(ckey[i], IPC_CREAT | 0666);
		msgctl(cqid[i], IPC_RMID, NULL);
	}

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
	WaitingQ_num = 0;

	//초기 자식 프로세스 생성 구간 - 부모 프로세스(처음 1번만 실행)
	for (int i = 0; i < 10; i++) {
		//sleep(1);
		ckey[i] = 0x1000 * (i + 1);
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
				if (bust_time[child_proc_num] == 0) {				//IO_bust part

					//임시코드	:	랜덤으로 생성하도록 수정해야
					if (iobust_time[child_proc_num] == 0) {
						iobust_time[child_proc_num] = 500;
					}
					//

					printf("pid[%d] : work1\n", getpid());
					temp = iobust_time[child_proc_num];
					iobust_time[child_proc_num] = iobust_time[child_proc_num] - 100;
					printf("pids[%d] = io_bust decrease %d - 100 = %d\n", getpid(), temp, iobust_time[child_proc_num]);
					//send parents message left io_bust time
					cmsgSnd(ckey[child_proc_num], iobust_time[child_proc_num]);

					//임시코드	:	랜덤으로 생성하도록 수정해야
					if (iobust_time[child_proc_num] == 0) {
						bust_time[child_proc_num] = 500;
					}
					//
				}
				else {												//CPU_bust part

					printf("\npid[%d] : work2\n", getpid());
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
				}
				kill(getpid(), SIGSTOP);
			}
		}
		//자식 프로세스 코드 구간
	}

	//부모 프로세스 코드 구간 - 시그널 통해 자식 프로세스 통제 및 종료

	delnode(ReadyQ, RunningQ);
	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
	while(1) {

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
	char ready[] = "ReadyQ";

	//writenode(ReadyQ, fp, ready);
	writeallnode(ReadyQ, WaitingQ, RunningQ, fp);

	for (int i = 0; i < WaitingQ_num; i++) {						//현재 WaitingQ의 모든 노드들의 io_bust감소 코드
		printf("iobust check\n");
		delnode(WaitingQ, IORunningQ);
		kill(pids[IORunningQ->proc_num], SIGCONT);					//WaitingQ의 맨앞 노드의 io_bust를 감소 -> 자식 프로세스의 io_bust감소 코드로

		//검토 필요 -> 아마 작동됨
		printf("parents Receive message %d\n", IORunningQ->proc_num);
		pmsgRcv(IORunningQ->proc_num, iobust_time);		//receive message from child
		//messagechk[IORunningQ->proc_num] = 0;
		printf("pid[%d] = iobust_time left : %d\n",pids[IORunningQ->proc_num], iobust_time[IORunningQ->proc_num]);

		if (iobust_time[IORunningQ->proc_num] == 0) {
			addnode(ReadyQ, IORunningQ->proc_num);
			WaitingQ_num--;
			printf("2Current WaitingQ_num = %d\n", WaitingQ_num);
		}
		else {
			addnode(WaitingQ, IORunningQ->proc_num);
		}
		//
	}

	//자식 프로세스의 cpu_bust 감소 부분
	if (RunningQ->proc_num != -1) {							//예외처리 ReadyQ가 비어있을 경우 -> 전부 WaitingQ에 있는경우
		child_proc = pids[RunningQ->proc_num];				//Running Queue의 자식 프로세스 pid 지정
		kill(pids[RunningQ->proc_num], SIGCONT);			//ReadyQ의 맨앞 노드의 cpu_bust를 감소 -> 자식 프로세스의 cpu_bust감소 코드로
	}
}

void signal_decrease(int signo) {			//실행중인 자식 프로세스에서 작동 SIGUSR1 -> 남은 cpu_bust가 0이 아님 -> ReadyQ맨 뒤로
	if (tickCount < QUANTUM_TIME-1) {		//퀀텀이 안끝남 -> RunningQ 유지
		tickCount++;
	}
	else {									//퀀텀이 끝남 -> ReadyQ맨 뒤로 -> RunningQ 업데이트
		addnode(ReadyQ, RunningQ->proc_num);
		delnode(ReadyQ, RunningQ);
		tickCount = 0;
	}
}

void signal_bustend(int signo) {			//실행중인 자식 프로세스에서 작동 SIGUSR2 -> 남은 cpu_bust가 0임 -> WaitingQ맨 뒤로
	int temp = 0;
	addnode(WaitingQ, RunningQ->proc_num);
	WaitingQ_num ++;
	printf("1Current WaitingQ_num = %d\n", WaitingQ_num);
	temp = delnode(ReadyQ, RunningQ);
	if (temp == 0) {						//예외처리 ReadyQ가 비어있는경우 -> 모든 프로세스가 WaitingQ에 있는경우
		RunningQ->proc_num = -1;
	}

	tickCount = 0;							//퀀텀 초기화
}

//합칠 코드
void pmsgSnd(int curProc) {				//->사용 X
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
void cmsgRcv(int* ckey) {				//->사용 X
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
void cmsgSnd(int ckey, int iobust_time) {
	int qid;
	int ret;
	int key = ckey;// 자식 프로세스 고유의 키 값.
	struct msgbuf msg;

	qid = msgget(key, IPC_CREAT | 0666);
	memset(&msg, 0, sizeof(msg));

	msg.mtype = 1;
	msg.mdata.pid = getpid();
	msg.mdata.cpuTime = 0;			//->사용 X
	msg.mdata.ioTime = iobust_time;	// io time을 부모에게 보낸다.
	printf("send c->p iobust_time : %d\n",iobust_time);

	if (ret = msgsnd(qid, (void*)&msg, sizeof(struct data), 0) == -1) {
		perror("msgsnd error");
		exit(1);
	}
	return;
}

// 자식이 보낸 데이터로 자식 iobust_time 업데이트
void pmsgRcv(int curProc, int* iobust_time) {
	int qid;
	int ret;
	int key = 0x1000 * (curProc + 1);
	struct msgbuf msg;

	qid = msgget(key, IPC_CREAT | 0666);
	memset(&msg, 0, sizeof(msg));

	if (ret = msgrcv(qid, (void*)&msg, sizeof(msg), 0, 0) == -1) {
		perror("msgrcv error");
		exit(1);
	}


	*(iobust_time + curProc) = msg.mdata.ioTime;

	//printf("%ld\t%d\t%d\t\t%d\n", msg.mtype, msg.mdata.pid, msg.mdata.cpuTime, msg.mdata.ioTime);
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
		//printf("Add first node\n");
	}
	else {									//첫번째 노드가 아니면 마지막 노드 업데이트
		list->tail->next = addnode;
		list->tail = addnode;
		//printf("Add node\n");
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
		//printf("Delete last node\n");
	}
	else {
		list->head = delnode->next;
		//printf("Delete node\n");
	}
	*return_node = *delnode;
	free(delnode);

	return 1;								//성공
}

void writenode(list* list, FILE* fp, char* listname) {
	node* nodepointer;
	fprintf(fp, "\nRunning tick = %d\n", running_ticks);
	running_ticks++;
	if (list->head == NULL) {	//list empty
		return;
	}
	nodepointer = list->head;
	for (int i = 0; ; ) {
		fprintf(fp, "%s[%d] = %d\n", listname, i, nodepointer->proc_num);
		if (nodepointer->next == NULL) {
			return;
		}
		else {
			nodepointer = nodepointer->next;
			i++;
		}
	}
}

void writeallnode(list* ready, list* waiting, node* running, FILE* fp) {
	node* nodepointer1;
	node* nodepointer2;
	
	nodepointer1 = ready->head;
	nodepointer2 = waiting->head;

	fprintf(fp, "\nRunning tick = %d\n", running_ticks);
	running_ticks++;

	for (int i = 0; i < MAX_PROC; i++) {
		if (nodepointer1 == NULL) {
			if (i == 0) {
				fprintf(fp, "ReadyQ is empty | ");
			}
			else {
				fprintf(fp, "\t       |");
			}
		}
		else{
			fprintf(fp, "ReadyQ[%d] = %d | ", i, nodepointer1->proc_num);
			nodepointer1 = nodepointer1->next;
		}
		if (nodepointer2 == NULL) {
			if (i == 0) {
				fprintf(fp, "WaitingQ is empty | ");
			}
			else {
				fprintf(fp, "\t\t         |");
			}
		}
		else {
			fprintf(fp, "WaitingQ[%d] = %d    | ", i, nodepointer2->proc_num);
			nodepointer2 = nodepointer2->next;
		}
		if (i == 0) {
			if (running->proc_num == -1) {
				fprintf(fp, "RunningQ is empty\n");
			}
			else {
				fprintf(fp, "RunningQ = %d\n", running->proc_num);
			}
		}
		else {
			fprintf(fp, "\n");
		}
	}
}
//헤더로 분리할 함수들 - 1
