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
#define TIME_TICK 1
#define TIME_QUANTUM 3

//////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct Node {
	struct Node* next;
	int procNum;
	int pid;
	int cpuTime;
	int ioTime;
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
void pushNode(List* list, int procNum);
int popNode(List* list, Node* runNode);

void signal_handler(int signo);
void signal_cpuSchedOut(int signo);
void signal_ioSchedIn(int signo);

void cmsgSnd(int key, int cpuOrigTime, int ioOriginTime);
void pmsgRcv(int curProc, Node* nodePtr);

//////////////////////////////////////////////////////////////////////////////////////////////////

List* waitQueue;
List* readyQueue;
List* subReadyQueue;
Node* cpuRunNode;
Node* ioRunNode;

//////////////////////////////////////////////////////////////////////////////////////////////////

int key[MAX_PROCESS];
int tickCount;

//////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* arg[]) {
	struct itimerval new_itimer;
	struct itimerval old_itimer;
	new_itimer.it_interval.tv_sec = TIME_TICK;
	new_itimer.it_interval.tv_usec = 0;
	new_itimer.it_value.tv_sec = 3;
	new_itimer.it_value.tv_usec = 0;

	struct sigaction tick;
	struct sigaction cpu;
	struct sigaction io;

	memset(&tick, 0, sizeof(tick));
	memset(&cpu, 0, sizeof(cpu));
	memset(&io, 0, sizeof(io));

	tick.sa_handler = &signal_handler;
	cpu.sa_handler = &signal_cpuSchedOut;
	io.sa_handler = &signal_ioSchedIn;

	sigaction(SIGALRM, &tick, NULL);
	sigaction(SIGUSR_CPU, &cpu, NULL);
	sigaction(SIGUSR_IO, &io, NULL);

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

	FILE* wfp = fopen("schedule_dump.txt", "w");
	if (wfp == NULL) {
		perror("file open error");
		exit(EXIT_FAILURE);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////

	tickCount = 0;
	for (int innerLoopIndex = 0; innerLoopIndex < MAX_PROCESS; innerLoopIndex++)
		key[innerLoopIndex] = 0x3216 * (innerLoopIndex + 1);
	
	//////////////////////////////////////////////////////////////////////////////////////////////////

	for (int outerLoopIndex = 0; outerLoopIndex < MAX_PROCESS; outerLoopIndex++) {
		int ret = fork();
		int ppid = getpid();
		
		// parent process
		if (ret > 0) {
			pushNode(readyQueue, outerLoopIndex);// no cpu, io time.
		}
		// child process
		else {
			int cpuOriginTime, ioOriginTime;
			int cpuTime, ioTime;
			int procNum = outerLoopIndex;

			FILE* rfp = fopen("time_set.txt", "r");
			if (rfp == NULL) {
				perror("file open error");
				exit(EXIT_FAILURE);
			}

			for (int innerLoopIndex = 0; innerLoopIndex < outerLoopIndex + 1; innerLoopIndex++) {
				fscanf(rfp, "%d %d", &cpuOriginTime, &ioOriginTime);
			}
			cpuTime = cpuOriginTime;
			ioTime = ioOriginTime;
			fclose(rfp);

			// child process waits until tick happens.
			kill(getpid(), SIGSTOP);

			while (true) {
				// cpu burst part.
				if (cpuTime != 0) {
					cpuTime =- TIME_TICK;

					// move process to the end of readyQueue.
					if (cpuTime != 0) {
						kill(ppid, SIGUSR_CPU);
					}
					// move process from readyQueue to waitQueue.
					else {
						cmsgSnd(key[procNum], cpuOriginTime, ioOriginTime);
						kill(ppid, SIGUSR_IO);
					}
				}
				kill(getpid(), SIGSTOP);
			}
		}
	}

	popNode(readyQueue, cpuRunNode);
	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
	while (true);

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void signal_handler(int signo) {
	Node* nodePtr = waitQueue->head;

	// io burst part.
	while (nodePtr != NULL) {
		nodePtr->ioTime =- TIME_TICK;

		if (nodePtr->ioTime == 0) {
			popNode(waitQueue);
			pushNode(readyQueue, nodePtr->procNum);
		}
		nodePtr = nodePtr->next;
	}

	// cpu burst part.
	if (cpuRunNode->procNum != -1) {
		kill(cpuRunNode->pid, SIGCONT);
	}
	return;
}

void signal_cpuSchedOut(int signo) {
	if (++tickCount >= TIME_QUANTUM) {
		pushNode(readyQueue, cpuRunNode->procNum);
		popNode(readyQueue, cpuRunNode);
		tickCount = 0;
	}
	return;
}

void signal_ioSchedIn(int signo) {
	pmsgRcv(cpuRunNode->procNum, cpuRunNode);
	pushNode(waitQueue, cpuRunNode->procNum);

	if (popNode(readyQueue, cpuRunNode) == -1) {
		cpuRunNode->procNum = -1;
	}
	tickCount = 0;
	return;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void initList(List* list) {
	list->head = NULL;
	list->tail = NULL;
	list->listNum = 0;
	return;
}

void pushNode(List* list, int procNum) {
	Node* newNode = (Node*)malloc(sizeof(Node));
	if (newNode == NULL) {
		perror("push node malloc error");
		exit(EXIT_FAILURE);
	}

	// newNode->cpuTime = 0;
	// newNode->ioTime = 0;
	// newNode->pid = 0;
	newNode->next = NULL;
	newNode->procNum = procNum;

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

int popNode(List* list, Node* runNode) {
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
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void cmsgSnd(int key, int cpuOrigTime, int ioOriginTime) {
	int qid = msgget(key, IPC_CREAT | 0666);

	struct msgbuf msg;
	memset(&msg, 0, sizeof(msg));

	msg.mtype = 1;
	msg.mdata.pid = getpid();
	msg.mdata.cpuTime = cpuOrigTime;
	msg.mdata.ioTime = ioOriginTime;

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
