#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#define MAX_PROCESS 10
#define TIME_TICK 100000// 0.1 second(100ms).
#define TIME_QUANTUM 3// 0.3 seconds(300ms).

//////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct Node {
	struct Node* next;
	int procNum;
	int pid;
	int cpuTime;
	int ioTime;
	int remTimeQuantum;
} Node;

typedef struct List {
	Node* head;
	Node* tail;
	int listNum;
} List;

struct data {
	int pid;
	int cpuTime;
	int ioTime;
};

struct msgbuf {
	long mtype;
	struct data mdata;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

void initList(List* list);
void pushBackNode(List* list, int procNum, int cpuTime, int ioTime, int remTimeQuantum);
int popFrontNode(List* list, Node* runNode);
void writeNode(List* readyQueue, List* subReadyQueue, List* waitQueue, Node* cpuRunNode, FILE* wfp);

void signal_timeTick(int signo);
void signal_vRRcpuSchedOut(int signo);
void signal_ioSchedIn(int signo);

void cmsgSnd(int key, int cpuBurstTime, int ioBurstTime);
void pmsgRcv(int curProc, Node* nodePtr);

//////////////////////////////////////////////////////////////////////////////////////////////////

List* waitQueue;
List* readyQueue;
List* subReadyQueue;
Node* cpuRunNode;
Node* ioRunNode;
FILE* rfp;
FILE* wfp;

//////////////////////////////////////////////////////////////////////////////////////////////////

int CPID[MAX_PROCESS];
int KEY[MAX_PROCESS];
int CONST_TICK_COUNT;
int TICK_COUNT;
int REM_TIME_QUANTUM;// virtual RR has remained time quantum.
int RUN_TIME;

//////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
	int originCpuBurstTime[2048];
	int originIoBurstTime[2048];
	int ppid = getpid();

	struct itimerval new_itimer;
	struct itimerval old_itimer;

	new_itimer.it_interval.tv_sec = 0;
	new_itimer.it_interval.tv_usec = TIME_TICK;
	new_itimer.it_value.tv_sec = 1;
	new_itimer.it_value.tv_usec = 0;

	struct sigaction tick;
	struct sigaction cpu;
	struct sigaction io;

	memset(&tick, 0, sizeof(tick));
	memset(&cpu, 0, sizeof(cpu));
	memset(&io, 0, sizeof(io));

	tick.sa_handler = &signal_timeTick;
	cpu.sa_handler = &signal_vRRcpuSchedOut;
	io.sa_handler = &signal_ioSchedIn;

	sigaction(SIGALRM, &tick, NULL);
	sigaction(SIGUSR1, &cpu, NULL);
	sigaction(SIGUSR2, &io, NULL);

	waitQueue = malloc(sizeof(List));
	readyQueue = malloc(sizeof(List));
	subReadyQueue = malloc(sizeof(List));
	cpuRunNode = malloc(sizeof(Node));
	ioRunNode = malloc(sizeof(Node));

	if (waitQueue == NULL || readyQueue == NULL || subReadyQueue == NULL) {
		perror("list malloc error");
		exit(EXIT_FAILURE);
	}
	if (cpuRunNode == NULL || ioRunNode == NULL) {
		perror("node malloc error");
		exit(EXIT_FAILURE);
	}

	initList(waitQueue);
	initList(readyQueue);
	initList(subReadyQueue);

	wfp = fopen("VRR_schedule_dump.txt", "w");
	if (wfp == NULL) {
		perror("file open error");
		exit(EXIT_FAILURE);
	}
	fclose(wfp);

	//////////////////////////////////////////////////////////////////////////////////////////////////

	CONST_TICK_COUNT = 0;
	TICK_COUNT = 0;
	RUN_TIME = 0;
	for (int innerLoopIndex = 0; innerLoopIndex < MAX_PROCESS; innerLoopIndex++) {
		KEY[innerLoopIndex] = 0x3216 * (innerLoopIndex + 1);
		msgctl(msgget(KEY[innerLoopIndex], IPC_CREAT | 0666), IPC_RMID, NULL);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////

	if (argc == 1 || argc == 2) {
		printf("COMMAND <TEXT FILE> <RUN TIME(sec)>\n");
		printf("./vRR.o time_set.txt 10\n");
		exit(EXIT_SUCCESS);
	}
	else {
		rfp = fopen((char*)argv[1], "r");
		if (rfp == NULL) {
			perror("file open error");
			exit(EXIT_FAILURE);
		}

		int preCpuTime;
		int preIoTime;

		for (int innerLoopIndex = 0; innerLoopIndex < 2048; innerLoopIndex++) {
			if (fscanf(rfp, "%d , %d", &preCpuTime, &preIoTime) == EOF) {
				printf("fscanf error");
				exit(EXIT_FAILURE);
			}
			originCpuBurstTime[innerLoopIndex] = preCpuTime;
			originIoBurstTime[innerLoopIndex] = preIoTime;
		}
		RUN_TIME = atoi(argv[2]);
		RUN_TIME = RUN_TIME * 1000000 / TIME_TICK;
	}
	printf("\x1b[33m");
	printf("[Log] process are initialized.\n");
	printf("\x1b[0m");

	//////////////////////////////////////////////////////////////////////////////////////////////////

	for (int outerLoopIndex = 0; outerLoopIndex < MAX_PROCESS; outerLoopIndex++) {
		int ret = fork();

		// parent process.
		if (ret > 0) {
			CPID[outerLoopIndex] = ret;
			pushBackNode(readyQueue, outerLoopIndex, originCpuBurstTime[outerLoopIndex], originIoBurstTime[outerLoopIndex], 0);
		}

		// child process.
		else {
			int procNum = outerLoopIndex;
			int cpuBurstTime = originCpuBurstTime[procNum];
			int ioBurstTime = originIoBurstTime[procNum];

			// child process waits until a tick happens.
			kill(getpid(), SIGSTOP);

			while (true) {
				// cpu burst part.
				cpuBurstTime--;
				printf("[Log] child process(%02d) rem. cpu burst time %d.\n", procNum, cpuBurstTime);
			
				if (cpuBurstTime == 0) {
					cpuBurstTime = originCpuBurstTime[procNum];
					cmsgSnd(KEY[procNum], cpuBurstTime, ioBurstTime);
					kill(ppid, SIGUSR2);
				}
				else {
					kill(ppid, SIGUSR1);
				}
				kill(getpid(), SIGSTOP);
			}
		}
	}

	popFrontNode(readyQueue, cpuRunNode);
	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
	while (RUN_TIME != 0);

	for (int innerLoopIndex = 0; innerLoopIndex < MAX_PROCESS; innerLoopIndex++) {
		msgctl(msgget(KEY[innerLoopIndex], IPC_CREAT | 0666), IPC_RMID, NULL);
		kill(CPID[innerLoopIndex], SIGKILL);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void signal_timeTick(int signo) {
	CONST_TICK_COUNT++;
	printf("[Log] tick %d\n", CONST_TICK_COUNT);

	// io burst part.
	Node* NodePtr = waitQueue->head;
	int waitQueueSize = 0;

	while (NodePtr != NULL) {
		NodePtr = NodePtr->next;
		waitQueueSize++;
	}

	for (int i = 0; i < waitQueueSize; i++) {
		popFrontNode(waitQueue, ioRunNode);
		ioRunNode->ioTime--;

		if (ioRunNode->ioTime == 0) {
			// io run node has no remained time quantum.
			if (ioRunNode->remTimeQuantum == 0) {
				pushBackNode(readyQueue, ioRunNode->procNum, ioRunNode->cpuTime, ioRunNode->ioTime, 0);
			}
			// io node has a remained time quantum.
			else {
				pushBackNode(subReadyQueue, ioRunNode->procNum, ioRunNode->cpuTime, ioRunNode->ioTime, ioRunNode->remTimeQuantum);
			}
			waitQueueSize--;
		}
		else {
			pushBackNode(waitQueue, ioRunNode->procNum, ioRunNode->cpuTime, ioRunNode->ioTime, ioRunNode->remTimeQuantum);
		}
	}

	// cpu burst part.
	if (cpuRunNode->procNum != -1) {
		kill(CPID[cpuRunNode->procNum], SIGCONT);// go to kill no.1
	}

	writeNode(readyQueue, subReadyQueue, waitQueue, cpuRunNode, wfp);
	// run time decreased by 1.
	RUN_TIME--;
	return;
}

// virtual Round Robin case.
void signal_vRRcpuSchedOut(int signo) {
	TICK_COUNT++;

	if (cpuRunNode->remTimeQuantum == TICK_COUNT || TICK_COUNT >= TIME_QUANTUM) {
		pushBackNode(readyQueue, cpuRunNode->procNum, cpuRunNode->cpuTime, cpuRunNode->ioTime, 0);

		if (subReadyQueue->head != NULL) {
			popFrontNode(subReadyQueue, cpuRunNode);
			printf("\x1b[33m");
			printf("[Log] sub ready queue pushed run node.\n");
			printf("\x1b[0m");
		}
		else {
			popFrontNode(readyQueue, cpuRunNode);
			printf("[Log] ready queue pushed run node.\n");
		}
		TICK_COUNT = 0;
	}
	return;
}

void signal_ioSchedIn(int signo) {
	TICK_COUNT++;
	REM_TIME_QUANTUM = TIME_QUANTUM - TICK_COUNT;

	pmsgRcv(cpuRunNode->procNum, cpuRunNode);
	pushBackNode(waitQueue, cpuRunNode->procNum, cpuRunNode->cpuTime, cpuRunNode->ioTime, REM_TIME_QUANTUM);

	if (subReadyQueue->head != NULL) {
		popFrontNode(subReadyQueue, cpuRunNode);
		printf("\x1b[33m");
		printf("[Log] sub ready queue pushed run node.\n");
		printf("\x1b[0m");
	}
	else if (readyQueue->head != NULL) {
		popFrontNode(readyQueue, cpuRunNode);
		printf("[Log] ready queue pushed run node.\n");
	}
	/*
	else if (popFrontNode(readyQueue, cpuRunNode) == -1) {
		cpuRunNode->procNum = -1;
	}*/
	TICK_COUNT = 0;
	return;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void initList(List* list) {
	list->head = NULL;
	list->tail = NULL;
	list->listNum = 0;
	return;
}

void pushBackNode(List* list, int procNum, int cpuTime, int ioTime, int remTimeQuantum) {
	Node* newNode = (Node*)malloc(sizeof(Node));
	if (newNode == NULL) {
		perror("push node malloc error");
		exit(EXIT_FAILURE);
	}

	newNode->next = NULL;
	newNode->procNum = procNum;
	newNode->cpuTime = cpuTime;
	newNode->ioTime = ioTime;
	newNode->remTimeQuantum = remTimeQuantum;

	// the first node case.
	if (list->head == NULL) {
		list->head = newNode;
		list->tail = newNode;
	}
	// another node cases.
	else {
		list->tail->next = newNode;
		list->tail = newNode;
	}
	return;
}

int popFrontNode(List* list, Node* runNode) {
	Node* oldNode = list->head;

	// empty list case.
	if (list->head == NULL) {
		return -1;
	}

	// pop the last node from a list case.
	if (list->head->next == NULL) {
		list->head = NULL;
		list->tail = NULL;
	}
	else {
		list->head = list->head->next;
	}

	*runNode = *oldNode;
	free(oldNode);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void cmsgSnd(int key, int cpuBurstTime, int ioBurstTime) {
	int qid = msgget(key, IPC_CREAT | 0666);

	struct msgbuf msg;
	memset(&msg, 0, sizeof(msg));

	msg.mtype = 1;
	msg.mdata.pid = getpid();
	msg.mdata.cpuTime = cpuBurstTime;
	msg.mdata.ioTime = ioBurstTime;

	if (msgsnd(qid, (void*)&msg, sizeof(struct data), 0) == -1) {
		perror("child msgsnd error");
		exit(EXIT_FAILURE);
	}
	return;
}

void pmsgRcv(int procNum, Node* nodePtr) {
	int key = 0x3216 * (procNum + 1);
	int qid = msgget(key, IPC_CREAT | 0666);

	struct msgbuf msg;
	memset(&msg, 0, sizeof(msg));

	if (msgrcv(qid, (void*)&msg, sizeof(msg), 0, 0) == -1) {
		perror("msgrcv error");
		exit(1);
	}

	nodePtr->pid = msg.mdata.pid;
	nodePtr->cpuTime = msg.mdata.cpuTime;
	nodePtr->ioTime = msg.mdata.ioTime;
	return;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void writeNode(List* readyQueue, List* subReadyQueue, List* waitQueue, Node* cpuRunNode, FILE* wfp) {
	Node* nodePtr1 = readyQueue->head;
	Node* nodePtr2 = subReadyQueue->head;
	Node* nodePtr3 = waitQueue->head;

	wfp = fopen("VRR_schedule_dump.txt", "a+");
	fprintf(wfp, "───────────────────────────────────────────────────────\n");
	fprintf(wfp, " TICK   %04d\n\n", CONST_TICK_COUNT);
	fprintf(wfp, " RUNNING PROCESS\n");
	fprintf(wfp, " %02d\n\n", cpuRunNode->procNum);
	fprintf(wfp, " READY QUEUE\n");

	if (nodePtr1 == NULL)
		fprintf(wfp, " none");
	while (nodePtr1 != NULL) {
		fprintf(wfp, " %02d ", nodePtr1->procNum);
		nodePtr1 = nodePtr1->next;
	}

	fprintf(wfp, "\n\n");
	fprintf(wfp, " SUB READY QUEUE\n");

	if (nodePtr2 == NULL)
		fprintf(wfp, " none");
	while (nodePtr2 != NULL) {
		fprintf(wfp, " %02d ", nodePtr2->procNum);
		nodePtr2 = nodePtr2->next;
	}

	fprintf(wfp, "\n\n");
	fprintf(wfp, " WAIT QUEUE\n");

	if (nodePtr3 == NULL)
		fprintf(wfp, " none");
	while (nodePtr3 != NULL) {
		fprintf(wfp, " %02d ", nodePtr3->procNum);
		nodePtr3 = nodePtr3->next;
	}

	fprintf(wfp, "\n");
	fclose(wfp);
	return;
}