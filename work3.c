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
#define TIME_TICK 1// 0.1 second
#define TIME_QUANTUM 3// 3 seconds

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
void pushBackNode(List* list, int procNum, int cpuTime, int ioTime);
int popFrontNode(List* list, Node* runNode);
void writeNode(List* readyQueue, List* waitQueue, Node* cpuRunNode, FILE* wfp);

void signal_timeTick(int signo);
void signal_cpuSchedOut(int signo);
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
int RUN_TIME;

//////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
	int originCpuBurstTime[2048];
	int originIoBurstTime[2048];

	struct itimerval new_itimer;
	struct itimerval old_itimer;

	new_itimer.it_interval.tv_sec = TIME_TICK;
	new_itimer.it_interval.tv_usec = 0;
	new_itimer.it_value.tv_sec = 1;
	new_itimer.it_value.tv_usec = 0;

	struct sigaction tick;
	struct sigaction cpu;
	struct sigaction io;

	memset(&tick, 0, sizeof(tick));
	memset(&cpu, 0, sizeof(cpu));
	memset(&io, 0, sizeof(io));

	tick.sa_handler = &signal_timeTick;
	cpu.sa_handler = &signal_cpuSchedOut;
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

	wfp = fopen("schedule_dump.txt", "w");
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
		printf("./sched.o time.txt 10\n");
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
	}

	printf("initialization is completed!\n");
	//////////////////////////////////////////////////////////////////////////////////////////////////

	for (int outerLoopIndex = 0; outerLoopIndex < MAX_PROCESS; outerLoopIndex++) {
		int ret = fork();
		int ppid = getpid();

		// parent process
		if (ret > 0) {
			CPID[outerLoopIndex] = ret;
			printf("%d, %d, %d\n", outerLoopIndex, originCpuBurstTime[outerLoopIndex], originIoBurstTime[outerLoopIndex]);
			pushBackNode(readyQueue, outerLoopIndex, originCpuBurstTime[outerLoopIndex], originIoBurstTime[outerLoopIndex]);
		}

		// child process
		else {
			printf("child process waits...\n");
			int procNum = outerLoopIndex;
			int cpuBurstTime = originCpuBurstTime[procNum];
			int ioBurstTime = originIoBurstTime[procNum];

			// child process waits until tick happens.
			kill(getpid(), SIGSTOP);// kill no.1

			while (true) {
				// cpu burst part
				cpuBurstTime =- 1;

				if (cpuBurstTime == 0) {
					printf("sigusr 2 called");
					cpuBurstTime = originCpuBurstTime[procNum];
					kill(ppid, SIGUSR2);
					cmsgSnd(KEY[procNum], cpuBurstTime, ioBurstTime);
				}
				else {
					printf("sigusr 1 called");
					kill(ppid, SIGUSR1);
				}
				kill(getpid(), SIGSTOP);
			}
		}
	}

	popFrontNode(readyQueue, cpuRunNode);// cpuRunNode has cpu, io time.
	printf("*** %d %d %d ***", cpuRunNode->procNum, cpuRunNode->cpuTime, cpuRunNode->ioTime);
	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
	while (RUN_TIME != 0);

	for (int innerLoopIndex = 0; innerLoopIndex < MAX_PROCESS; innerLoopIndex++) {
		msgctl(msgget(KEY[innerLoopIndex], IPC_CREAT | 0666), IPC_RMID, NULL);
		kill(CPID[innerLoopIndex], SIGKILL);
	}

	printf("processes are killed.\n");
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void signal_timeTick(int signo) {
	//writeNode(readyQueue, waitQueue, cpuRunNode, wfp);

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
			pushBackNode(readyQueue, ioRunNode->procNum, ioRunNode->cpuTime, ioRunNode->ioTime);
			waitQueueSize--;
		}
		else {
			pushBackNode(waitQueue, ioRunNode->procNum, ioRunNode->cpuTime, ioRunNode->ioTime);
		}
	}

	// cpu burst part.
	if (cpuRunNode->procNum != -1) {
		kill(CPID[cpuRunNode->procNum], SIGCONT);// go to kill no.1
	}

	// run time decreased by 1.
	RUN_TIME--;
	printf("1 time tick passed.\n");
	return;
}

void signal_ioSchedIn(int signo) {
	pmsgRcv(cpuRunNode->procNum, cpuRunNode);
	pushBackNode(waitQueue, cpuRunNode->procNum, cpuRunNode->cpuTime, cpuRunNode->ioTime);

	if (popFrontNode(readyQueue, cpuRunNode) == -1) {
		cpuRunNode->procNum = -1;
	}
	TICK_COUNT = 0;
	return;
}// go to signal_timeTick.

void signal_cpuSchedOut(int signo) {
	TICK_COUNT++;

	if (TICK_COUNT >= TIME_QUANTUM) {
		printf("cpuRunNode %d %d %d\n", cpuRunNode->procNum, cpuRunNode->cpuTime, cpuRunNode->ioTime);
		pushBackNode(readyQueue, cpuRunNode->procNum, cpuRunNode->cpuTime, cpuRunNode->ioTime);
		popFrontNode(readyQueue, cpuRunNode);
		TICK_COUNT = 0;
	}
	return;
}// go to signal_timeTick.

//////////////////////////////////////////////////////////////////////////////////////////////////

void initList(List* list) {
	list->head = NULL;
	list->tail = NULL;
	list->listNum = 0;
	return;
}

void pushBackNode(List* list, int procNum, int cpuTime, int ioTime) {
	printf("push back node called.\n");
	Node* newNode = (Node*)malloc(sizeof(Node));
	if (newNode == NULL) {
		perror("push node malloc error");
		exit(EXIT_FAILURE);
	}

	newNode->next = NULL;
	newNode->procNum = procNum;
	newNode->cpuTime = cpuTime;
	newNode->ioTime = ioTime;

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
	printf("pop front node called.\n");
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

void writeNode(List* readyQueue, List* waitQueue, Node* cpuRunNode, FILE* wfp) {
	Node* nodePtr1 = readyQueue->head;
	Node* nodePtr2 = waitQueue->head;

	wfp = fopen("schedule_dump.txt", "a+");

	fprintf(wfp, "\n");
	fprintf(wfp, "┌──────┬───────┬─────────────────┬─────────────┬───────────────┐\n");
	fprintf(wfp, "│ TICK │ INDEX │ RUNNING PROCESS │ READY QUEUE │ WAITING QUEUE │\n");
	CONST_TICK_COUNT++;

	for (int i = 0; i < MAX_PROCESS; i++) {
		// print index, running process.
		if (i == 0) {
			if (cpuRunNode->procNum == -1) {
				fprintf(wfp, "│ %04d │ %02d    │ none            ", CONST_TICK_COUNT, i);
			}
			else {
				fprintf(wfp, "│ %04d │ %02d    │ %02d              ", CONST_TICK_COUNT, i, cpuRunNode->procNum);
			}
		}
		else {
			fprintf(wfp, "│      │ %02d    │                 ", i);
		}

		// print ready queue.
		if (nodePtr1 == NULL) {
			if (i == 0) {
				fprintf(wfp, "│ none        ");
			}
			else {
				fprintf(wfp, "│             ");
			}
		}
		else {
			fprintf(wfp, "│ %02d          ", nodePtr1->procNum);
			nodePtr1 = nodePtr1->next;
		}

		// printf waiting queue.
		if (nodePtr2 == NULL) {
			if (i == 0) {
				fprintf(wfp, "│ none          │\n");
			}
			else {
				fprintf(wfp, "│               │\n");
			}
		}
		else {
			fprintf(wfp, "│ %02d            │\n", nodePtr2->procNum);
			nodePtr2 = nodePtr2->next;
		}
	}
	fprintf(wfp, "└──────┴───────┴─────────────────┴─────────────┴───────────────┘\n");
	fclose(wfp);
	return;
}
