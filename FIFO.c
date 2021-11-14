/*
 * class	: Operating System(MS)
 * Project 01	: First-In First-Out Scheduling Simulation
 * Author	: jaeil Park, junseok Tak
 * Student ID	: 32161786, 32164809
 * Date		: 2021-11-16
 * Professor	: seehwan Yoo
 * Left freedays: 2
 */

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
#define TIME_TICK 10000// 0.01 second(10ms).

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

// message buffer that contains child process's data.
struct msgbuf {
	long mtype;
	struct data mdata;
};

void initList(List* list);
void pushBackNode(List* list, int procNum, int cpuTime, int ioTime);
void popFrontNode(List* list, Node* runNode);
bool isEmptyList(List* list);
void writeNode(List* readyQueue, List* waitQueue, Node* cpuRunNode, FILE* wfp);

void signal_timeTick(int signo);
void signal_FIFOcpuSchedOut(int signo);
void signal_ioSchedIn(int signo);

void cmsgSnd(int key, int cpuBurstTime, int ioBurstTime);
void pmsgRcv(int curProc, Node* nodePtr);

List* waitQueue;
List* readyQueue;
List* subReadyQueue;
Node* cpuRunNode;
Node* ioRunNode;
FILE* rfp;
FILE* wfp;

int CPID[MAX_PROCESS];// child process pid.
int KEY[MAX_PROCESS];// key value for message queue.
int CONST_TICK_COUNT;
int TICK_COUNT;
int RUN_TIME;

//////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
	int originCpuBurstTime[3000];
	int originIoBurstTime[3000];
	int ppid = getpid();// get parent process id.

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
	cpu.sa_handler = &signal_FIFOcpuSchedOut;
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

	// initialize ready, sub-ready, wait queues.
	initList(waitQueue);
	initList(readyQueue);
	initList(subReadyQueue);

	wfp = fopen("FIFO_schedule_dump.txt", "w");
	if (wfp == NULL) {
		perror("file open error");
		exit(EXIT_FAILURE);
	}
	fclose(wfp);

	CONST_TICK_COUNT = 0;
	TICK_COUNT = 0;
	RUN_TIME = 0;

	// create message queue key.
	for (int innerLoopIndex = 0; innerLoopIndex < MAX_PROCESS; innerLoopIndex++) {
		KEY[innerLoopIndex] = 0x3216 * (innerLoopIndex + 1);
		msgctl(msgget(KEY[innerLoopIndex], IPC_CREAT | 0666), IPC_RMID, NULL);
	}

	// handle main function arguments.
	if (argc == 1 || argc == 2) {
		printf("COMMAND <TEXT FILE> <RUN TIME(sec)>\n");
		printf("./RR.o time_set.txt 10\n");
		exit(EXIT_SUCCESS);
	}
	else {
		// open time_set.txt file.
		rfp = fopen((char*)argv[1], "r");
		if (rfp == NULL) {
			perror("file open error");
			exit(EXIT_FAILURE);
		}

		int preCpuTime;
		int preIoTime;

		// read time_set.txt file.
		for (int innerLoopIndex = 0; innerLoopIndex < 3000; innerLoopIndex++) {
			if (fscanf(rfp, "%d , %d", &preCpuTime, &preIoTime) == EOF) {
				printf("fscanf error");
				exit(EXIT_FAILURE);
			}
			originCpuBurstTime[innerLoopIndex] = preCpuTime;
			originIoBurstTime[innerLoopIndex] = preIoTime;
		}
		// set program run time.
		RUN_TIME = atoi(argv[2]);
		RUN_TIME = RUN_TIME * 1000000 / TIME_TICK;
	}
	printf("\x1b[33m");
	printf("TICK   PROC NUMBER   REMAINED CPU TIME\n");
	printf("\x1b[0m");

	//////////////////////////////////////////////////////////////////////////////////////////////////

	for (int outerLoopIndex = 0; outerLoopIndex < MAX_PROCESS; outerLoopIndex++) {
		// create child process.
		int ret = fork();

		// parent process.
		if (ret > 0) {
			CPID[outerLoopIndex] = ret;
			pushBackNode(readyQueue, outerLoopIndex, originCpuBurstTime[outerLoopIndex], originIoBurstTime[outerLoopIndex]);
		}

		// child process.
		else {
			int RANDOM = 1;
			int procNum = outerLoopIndex;
			int cpuBurstTime = originCpuBurstTime[procNum];
			int ioBurstTime = originIoBurstTime[procNum];

			// child process waits until a tick happens.
			kill(getpid(), SIGSTOP);

			// cpu burst part.
			while (true) {
				cpuBurstTime--;// decrease cpu burst time by 1.
				printf("            %02d            %02d\n", procNum, cpuBurstTime);
				printf("───────────────────────────────────────────\n");

				// cpu task is over.
				if (cpuBurstTime == 0) {
					cpuBurstTime = originCpuBurstTime[procNum + (RANDOM*10)];// set the next cpu burst time.

					// send the data of child process to parent process.
					cmsgSnd(KEY[procNum], cpuBurstTime, ioBurstTime);
					ioBurstTime = originIoBurstTime[procNum * RANDOM];// set the next io burst time.
					RANDOM++;
					if (RANDOM > 298)
					{
						RANDOM = 1;
					}
					kill(ppid, SIGUSR2);
				}
				// cpu task is not over.
				else {
					kill(ppid, SIGUSR1);
				}
				// child process waits until the next tick happens.
				kill(getpid(), SIGSTOP);
			}
		}
	}

	// get the first node from ready Queue.
	popFrontNode(readyQueue, cpuRunNode);
	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);

	// parent process excutes until run time is over.
	while (RUN_TIME != 0);

	// remove message queues and terminate child processes.
	for (int innerLoopIndex = 0; innerLoopIndex < MAX_PROCESS; innerLoopIndex++) {
		msgctl(msgget(KEY[innerLoopIndex], IPC_CREAT | 0666), IPC_RMID, NULL);
		kill(CPID[innerLoopIndex], SIGKILL);
	}
	
	// free dynamic memory allocation.
	free(readyQueue);
	free(subReadyQueue);
	free(waitQueue);
	free(cpuRunNode);
	free(ioRunNode);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

/*
* void signal_timeTick(int signo);
*   This function is signal handler of SIGALARM which is called every time tick.
*
* parameter: int
*   signo:
*
* return: none.
*/
void signal_timeTick(int signo) {
	CONST_TICK_COUNT++;
	printf("%05d       PROC NUMBER   REMAINED CPU TIME\n", CONST_TICK_COUNT);

	// io burst part.
	Node* NodePtr = waitQueue->head;
	int waitQueueSize = 0;

	// get the size of wait queue.
	while (NodePtr != NULL) {
		NodePtr = NodePtr->next;
		waitQueueSize++;
	}

	for (int i = 0; i < waitQueueSize; i++) {
		popFrontNode(waitQueue, ioRunNode);
		ioRunNode->ioTime--;// decrease io time by 1.

		// io task is over, then push node to ready queue.
		if (ioRunNode->ioTime == 0) {
			pushBackNode(readyQueue, ioRunNode->procNum, ioRunNode->cpuTime, ioRunNode->ioTime);
		}
		// io task is not over, then push node to wait queue again.
		else {
			pushBackNode(waitQueue, ioRunNode->procNum, ioRunNode->cpuTime, ioRunNode->ioTime);
		}
	}

	// cpu burst part.
	if (cpuRunNode->procNum != -1) {
		kill(CPID[cpuRunNode->procNum], SIGCONT);
	}

	writeNode(readyQueue, waitQueue, cpuRunNode, wfp);// write ready, wait queue dump to txt file.
	RUN_TIME--;// run time decreased by 1.
	return;
}

/*
* void signal_FIFOcpuSchedOut(int signo);
*   This function counts the time tick.
*
* parameter: int
*	signo:
*
* return: none.
*/
void signal_FIFOcpuSchedOut(int signo) {
	TICK_COUNT++;
	return;
}

/*
* void signal_ioSchedIn(int signo);
*   This function checks the child process whether it has io tasks or not,
*	and pushes the current cpu preemptive process to the end of the ready queue or wait queue.
*	Then, pop the next process from the ready queue to excute cpu task.
*
* parameter: int
*	signo:
*
* return: none.
*/
void signal_ioSchedIn(int signo) {
	pmsgRcv(cpuRunNode->procNum, cpuRunNode);

	if (cpuRunNode->ioTime == 0) {
		pushBackNode(readyQueue, cpuRunNode->procNum, cpuRunNode->cpuTime, cpuRunNode->ioTime);
	}
	else {
		pushBackNode(waitQueue, cpuRunNode->procNum, cpuRunNode->cpuTime, cpuRunNode->ioTime);
	}

	popFrontNode(readyQueue, cpuRunNode);
	TICK_COUNT = 0;
	return;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

/*
* void initList(List* list);
*   This function initializes the list to a null value.
*
* parameter: List*
*	list: the list which has to be initialized.
*
* return: none.
*/
void initList(List* list) {
	list->head = NULL;
	list->tail = NULL;
	list->listNum = 0;
	return;
}

/*
* void pushBackNode(List* list, int procNum, int cpuTime, int ioTime);
*   This function creates a new node and pushes to the end of the list.
*
* parameter: List*, int, int, int
*	list: the list that the new node will be pushed.
*	procNum: the index of the process.
*	cpuTime: the cpu burst time of the process.
*	ioTime: the io burst time of the process.
*
* return: none.
*/
void pushBackNode(List* list, int procNum, int cpuTime, int ioTime) {
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

/*
* void popFrontNode(List* list, Node* runNode);
*   This function pops the front node from the list.
*
* parameter: List*, Node*
*	list: the list that the front node will be poped.
*	runNode: the node pointer that pointed the poped node.
*
* return: none.
*/
void popFrontNode(List* list, Node* runNode) {
	Node* oldNode = list->head;

	// empty list case.
	if (isEmptyList(list) == true) {
		runNode->cpuTime = -1;
		runNode->ioTime = -1;
		runNode->procNum = -1;
		return;
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
	return;
}

/*
* bool isEmptyList(List* list);
*   This function checks whether the list is empty or not.
*
* parameter: List*
*	list: the list to check if it's empty or not.
*
* return: bool
*	true: the list is empty.
*	false: the list is not empty.
*/
bool isEmptyList(List* list) {
	if (list->head == NULL)
		return true;
	else
		return false;
}

/*
* void cmsgSnd(int key, int cpuBurstTime, int ioBurstTime)
*   This function is a function in which the child process puts data in the msg structure and sends it to the message queue.
*
* parameter: int, int, int
*	key: the key value of message queue.
*	cpuBurstTime: child process's cpu burst time.
*	ioBurstTime: child process's io burst time.
*
* return: none.
*/
void cmsgSnd(int key, int cpuBurstTime, int ioBurstTime) {
	int qid = msgget(key, IPC_CREAT | 0666);// create message queue ID.

	struct msgbuf msg;
	memset(&msg, 0, sizeof(msg));

	msg.mtype = 1;
	msg.mdata.pid = getpid();
	msg.mdata.cpuTime = cpuBurstTime;// child process cpu burst time.
	msg.mdata.ioTime = ioBurstTime;// child process io burst time.

	// child process sends its data to parent process.
	if (msgsnd(qid, (void*)&msg, sizeof(struct data), 0) == -1) {
		perror("child msgsnd error");
		exit(EXIT_FAILURE);
	}
	return;
}

/*
* void pmsgRcv(int procNum, Node* nodePtr);
*   This function is a function in which the parent process receives data from the message queue and gets it from the msg structure.
*
* parameter: int, Node*
*	procNum: the index of current cpu or io running process.
*	nodePtr:
*
* return: none.
*/
void pmsgRcv(int procNum, Node* nodePtr) {
	int key = 0x3216 * (procNum + 1);// create message queue key.
	int qid = msgget(key, IPC_CREAT | 0666);

	struct msgbuf msg;
	memset(&msg, 0, sizeof(msg));

	// parent process receives child process data.
	if (msgrcv(qid, (void*)&msg, sizeof(msg), 0, 0) == -1) {
		perror("msgrcv error");
		exit(1);
	}

	// copy the data of child process to nodePtr.
	nodePtr->pid = msg.mdata.pid;
	nodePtr->cpuTime = msg.mdata.cpuTime;
	nodePtr->ioTime = msg.mdata.ioTime;
	return;
}

/*
* void writeNode(List* readyQueue, List* waitQueue, Node* cpuRunNode, FILE* wfp);
*   This function write the ready queue dump and wait queue dump to scheduler_dump.txt file.
*
* parameter: List*, List*, Node*, FILE*
*   readyQueue: List pointer that points readyQueue List.
*	waitQueue: List pointer that points waitQueue List.
*	cpuRunNode: Node pointer that points cpuRunNode.
*	wfp: file pointer that points stream file.
*
* return: none.
*/
void writeNode(List* readyQueue, List* waitQueue, Node* cpuRunNode, FILE* wfp) {
	Node* nodePtr1 = readyQueue->head;
	Node* nodePtr2 = waitQueue->head;

	wfp = fopen("FIFO_schedule_dump.txt", "a+");// open stream file append+ mode.
	fprintf(wfp, "───────────────────────────────────────────────────────\n");
	fprintf(wfp, " TICK   %04d\n\n", CONST_TICK_COUNT);
	fprintf(wfp, " RUNNING PROCESS\n");
	fprintf(wfp, " %02d\n\n", cpuRunNode->procNum);
	fprintf(wfp, " READY QUEUE\n");

	if (nodePtr1 == NULL)
		fprintf(wfp, " none");
	while (nodePtr1 != NULL) {// write ready queue dump.
		fprintf(wfp, " %02d ", nodePtr1->procNum);
		nodePtr1 = nodePtr1->next;
	}

	fprintf(wfp, "\n\n");
	fprintf(wfp, " WAIT QUEUE\n");

	if (nodePtr2 == NULL)
		fprintf(wfp, " none");
	while (nodePtr2 != NULL) {// write wait queue dump.
		fprintf(wfp, " %02d ", nodePtr2->procNum);
		nodePtr2 = nodePtr2->next;
	}

	fprintf(wfp, "\n");
	fclose(wfp);
	return;
}
