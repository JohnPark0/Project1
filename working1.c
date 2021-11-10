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
void signal_ready2wait(int signo);
void signal_cpubustRR(int signo);
void signal_cpubustFIFO(int signo);
void signal_tick(int signo);
void addnode(list* list, int proc_num);
int delnode(list* list, node* return_node);
void writenode(list* list, FILE* fp, char* listname);
void writeallnode(list* ready, list* waiting, node* running, FILE* fp);
void inilist(list* list);
void cmsgSnd(int ckey, int iobust_time);
void pmsgRcv(int curProc, int* iobust_time);

int bust_time[MAX_PROC];
int iobust_time[MAX_PROC];
int pids[MAX_PROC];
int parents_pid;
int WaitingQ_num;
int running_ticks = 0;
int tickCount = 0; //타임 카운터
int curProc = 1;// 현재 실행 중인 자식 프로세스.
int ckey[MAX_PROC];// child holds child process key array.
int running_time = 0;	//프로그램 구동 시간 = sec

list* WaitingQ;
list* ReadyQ;
node* RunningQ;
node* IORunningQ;

FILE* fp;

int main(int argc, char* argv[]) {
	int ret;
	int temp;
	int Set = 0;
	int Origin = 1;
	int child_proc_num = 0;		//편의를 위해 자식 프로세스 번호지정
	int Origin_bust_time[3000];
	int Origin_iobust_time[3000];

	FILE* settingfp;

	fp = fopen("schedule_dump.txt", "w");
	fclose(fp);

	struct sigaction new_sa;
	memset(&new_sa, 0, sizeof(new_sa));
	new_sa.sa_handler = &signal_tick;
	sigaction(SIGALRM, &new_sa, NULL);

	struct itimerval new_itimer, old_itimer;
	if (TICK_TIME > 999999) {
		new_itimer.it_interval.tv_sec = TICK_TIME / 1000000;
	}
	new_itimer.it_interval.tv_usec = TICK_TIME % 1000000;
	new_itimer.it_value.tv_sec = 1;
	new_itimer.it_value.tv_usec = 0;

	struct sigaction act;
	memset(&act, 0, sizeof(act));
	

	struct sigaction act2;
	memset(&act2, 0, sizeof(act2));
	act2.sa_handler = &signal_ready2wait;
	sigaction(SIGUSR2, &act2, NULL);

	ReadyQ = malloc(sizeof(list));
	WaitingQ = malloc(sizeof(list));
	RunningQ = malloc(sizeof(node));
	IORunningQ = malloc(sizeof(node));

	inilist(ReadyQ);
	inilist(WaitingQ);

	for (int i = 0; i < MAX_PROC; i++) {						//이전에 사용했던 메시지 큐를 비워 에러 방지(무한대로 돌기때문에 프로그램 끝날때 초기화 불가능)
		ckey[i] = 0x1000 * (i + 1);
		temp = msgget(ckey[i], IPC_CREAT | 0666);
		msgctl(temp, IPC_RMID, NULL);
	}

	if(argc == 1) {
		Set = 0;
		bust_time[0] = 1;
		bust_time[1] = 5;
		bust_time[2] = 1;
		bust_time[3] = 2;
		bust_time[4] = 1;
		bust_time[5] = 3;
		bust_time[6] = 7;
		bust_time[7] = 4;
		bust_time[8] = 1;
		bust_time[9] = 6;

		iobust_time[0] = 1;
		iobust_time[1] = 5;
		iobust_time[2] = 1;
		iobust_time[3] = 2;
		iobust_time[4] = 1;
		iobust_time[5] = 3;
		iobust_time[6] = 7;
		iobust_time[7] = 4;
		iobust_time[8] = 1;
		iobust_time[9] = 6;
	}
	else {//Setting.txt 파일을 입력 받았을 때
		Set = 1;
		settingfp = fopen((char*)argv[1], "r");
		if (settingfp == NULL) {
			printf("setting.txt Reading ERROR\n");
			exit(0);
		}
		for (int i = 0; i < 3000; i++) {
			if (fscanf(settingfp, "%d , %d", &ret, &temp) == EOF) {		//변수 선언을 최소화 하기위해 선언한 변수 활용
				printf("Setting Data type ERROR\n");
				exit(0);
			}
			Origin_bust_time[i] = ret;
			Origin_iobust_time[i] = temp;
			if (i < MAX_PROC) {
				bust_time[i] = ret;
				iobust_time[i] = temp;
			}
		}
		ret = 0;
		temp = 0;

		if (argv[2] != NULL) {										//작동시간 = 작동시간/틱타임(micro sec)
			running_time = atoi(argv[2]);
			running_time = running_time * 1000000 / TICK_TIME;
		}
		else {
			running_time = 0xFFFFFF;
		}

		if (argv[3] != NULL) {
			temp = atoi(argv[3]);
			if (temp == 0) {			//RR
				act.sa_handler = &signal_cpubustRR;
			}
			else {						//FIFO
				act.sa_handler = &signal_cpubustFIFO;
			}
			sigaction(SIGUSR1, &act, NULL);
		}
		else {							//Default RR
			act.sa_handler = &signal_cpubustRR;
			sigaction(SIGUSR1, &act, NULL);
		}
	}
	parents_pid = getpid();
	WaitingQ_num = 0;

	//초기 자식 프로세스 생성 구간 - 부모 프로세스(처음 1번만 실행)
	for (int i = 0; i < 10; i++) {
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
			printf("pid[%d] : stop\n", getpid());
			kill(getpid(), SIGSTOP);

			while (1) {
				if (bust_time[child_proc_num] == 0) {				//IO_bust part
					if (Set == 1) {					//세팅 파일 존재
						iobust_time[child_proc_num] = Origin_iobust_time[child_proc_num * Origin];
						bust_time[child_proc_num] = Origin_bust_time[child_proc_num * Origin];
						Origin++;
						if (Origin > 300) {
						Origin = 1;
						}
					}
					else if (Set == 0) {
						iobust_time[child_proc_num] = 5;
						bust_time[child_proc_num] = 5;
					}
					cmsgSnd(ckey[child_proc_num], iobust_time[child_proc_num]);
				}
				else {												//CPU_bust part

					printf("\npid[%d] : work2\n", getpid());
					temp = bust_time[child_proc_num];
					if (bust_time[child_proc_num] - 1 == 0) {
						bust_time[child_proc_num] = bust_time[child_proc_num] - 1;
						printf("pids[%d] = cpu_bust decrease %d - 1 = %d\n", getpid(), temp, bust_time[child_proc_num]);
						kill(parents_pid, SIGUSR2);
					}
					else {
						bust_time[child_proc_num] = bust_time[child_proc_num] - 1;
						printf("pids[%d] = cpu_bust decrease %d - 1 = %d\n", getpid(), temp, bust_time[child_proc_num]);
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
	while (running_time != 0) {

	}

	//추가할 것 : 프로그램의 종료 조건(모든 자식 프로세스의 CPU bust가 0)
	for (int i = 0; i < 10; i++) {
		kill(pids[i], SIGKILL);
		printf("sigkill\n");
	}
	//부모 프로세스 코드 구간

	return 0;
}

void signal_tick(int signo){				//부모 프로세스에서 작동 SIGALRM
	int child_proc;

	writeallnode(ReadyQ, WaitingQ, RunningQ, fp);

	for (int i = 0; i < WaitingQ_num; i++) {				//WaitingQ의 모든 노드 io_bust 감소
		printf("iobust check\n");
		delnode(WaitingQ, IORunningQ);

		iobust_time[IORunningQ->proc_num]--;
		if (iobust_time[IORunningQ->proc_num] == 0) {		//io가 끝남 -> ReadyQ 맨 뒤로
			kill(pids[IORunningQ->proc_num], SIGCONT);
			pmsgRcv(IORunningQ->proc_num, iobust_time);
			addnode(ReadyQ, IORunningQ->proc_num);
			WaitingQ_num--;
			printf("2Current WaitingQ_num = %d\n", WaitingQ_num);
		}
		else {												//io가 안끝남 -> WaitingQ 맨 뒤로
			addnode(WaitingQ, IORunningQ->proc_num);
		}
	}

	//자식 프로세스의 cpu_bust 감소 부분
	//예외처리 ReadyQ가 비어있을 경우 -> 전부 WaitingQ에 있는경우 -> RunningQ->proc_num == -1 <아무것도 안함>
	if (RunningQ->proc_num != -1) {							
		child_proc = pids[RunningQ->proc_num];				//Running Queue의 자식 프로세스 pid 지정
		kill(pids[RunningQ->proc_num], SIGCONT);			//ReadyQ의 맨앞 노드의 cpu_bust를 감소 -> 자식 프로세스의 cpu_bust감소 코드로
	}
	running_time--;
}

void signal_cpubustRR(int signo) {							//실행중인 자식 프로세스에서 작동 SIGUSR1 -> 남은 cpu_bust가 0이 아님 -> (조건)ReadyQ맨 뒤로
	if (tickCount < QUANTUM_TIME - 1) {						//퀀텀이 안끝남 -> RunningQ 유지
		tickCount++;
	}
	else {													//퀀텀이 끝남 -> ReadyQ맨 뒤로 -> RunningQ 업데이트
		addnode(ReadyQ, RunningQ->proc_num);
		delnode(ReadyQ, RunningQ);
		tickCount = 0;
	}
}

void signal_cpubustFIFO(int signo) {						//실행중인 자식 프로세스에서 작동 SIGUSR1 -> 남은 cpu_bust가 0이 아님 -> (조건)ReadyQ맨 뒤로
	if (tickCount < 1000000000) {						//퀀텀이 안끝남 -> RunningQ 유지
		tickCount++;
	}
	else {													//퀀텀이 끝남 -> ReadyQ맨 뒤로 -> RunningQ 업데이트
		addnode(ReadyQ, RunningQ->proc_num);
		delnode(ReadyQ, RunningQ);
		tickCount = 0;
	}
}

void signal_ready2wait(int signo) {			//실행중인 자식 프로세스에서 작동 SIGUSR2 -> 남은 cpu_bust가 0임 -> WaitingQ맨 뒤로
	int temp = 0;
	addnode(WaitingQ, RunningQ->proc_num);
	WaitingQ_num++;
	printf("1Current WaitingQ_num = %d\n", WaitingQ_num);
	temp = delnode(ReadyQ, RunningQ);
	if (temp == 0) {						//예외처리 ReadyQ가 비어있는경우 -> 모든 프로세스가 WaitingQ에 있는경우
		RunningQ->proc_num = -1;
	}

	tickCount = 0;							//퀀텀 초기화
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
	printf("send c->p iobust_time : %d\n", iobust_time);

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
	}
	else {									//첫번째 노드가 아니면 마지막 노드 업데이트
		list->tail->next = addnode;
		list->tail = addnode;
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
	}
	else {
		list->head = delnode->next;
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
	node* nodePtr1 = ready->head;
	node* nodePtr2 = waiting->head;

	fp = fopen("schedule_dump.txt", "a+");				//덤프파일 사용때만 열고닫아 파일 깨짐 방지

	fprintf(fp, "\n");
	fprintf(fp, "┌──────┬───────┬─────────────────┬─────────────┬───────────────┐\n");
	fprintf(fp, "│ TICK │ INDEX │ RUNNING PROCESS │ READY QUEUE │ WAITING QUEUE │\n");
	running_ticks++;

	for (int i = 0; i < MAX_PROC; i++) {
		// print index, running process.
		if (i == 0) {
			if (running->proc_num == -1) {
				fprintf(fp, "│ %04d │ %02d    │ none            ", running_ticks, i);
			}
			else {
				fprintf(fp, "│ %04d │ %02d    │ %02d              ", running_ticks, i, running->proc_num);
			}
		}
		else {
			fprintf(fp, "│      │ %02d    │                 ", i);
		}

		// print ready queue.
		if (nodePtr1 == NULL) {
			if (i == 0) {
				fprintf(fp, "│ none        ");
			}
			else {
				fprintf(fp, "│             ");
			}
		}
		else {
			fprintf(fp, "│ %02d          ", nodePtr1->proc_num);
			nodePtr1 = nodePtr1->next;
		}

		// printf waiting queue.
		if (nodePtr2 == NULL) {
			if (i == 0) {
				fprintf(fp, "│ none          │\n");
			}
			else {
				fprintf(fp, "│               │\n");
			}
		}
		else {
			fprintf(fp, "│ %02d            │\n", nodePtr2->proc_num);
			nodePtr2 = nodePtr2->next;
		}
	}
	fprintf(fp, "└──────┴───────┴─────────────────┴─────────────┴───────────────┘\n");

	fclose(fp);
}
//헤더로 분리할 함수들 - 1