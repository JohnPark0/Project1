#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <sched.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include "LinkedList.h"
#include "Process.h"

#define MAX_CHILD_PROCESS 10
#define TIME_TICK 10000// 10ms.
#define TIME_QUANTUM 30000// 30ms.

typedef struct Message {
	int procNum;
	int pid;
	int cpuTime;
	int ioTime;
} Message;

typedef struct msgbuf {
	Message data;
	long type;
} msg;

void signal_timeTick(int signo);
void signal_cpuSchedOut(int signo);
void signal_ioSchedIn(int signo);
void cmsgSnd(int key, int cpuBurstTime, int ioBurstTime);
void pmsgRcv(int procNum, Node* ptr);

List readyQueue;
List subReadyQueue;
List waitQueue;
Node cpuRunProcess;
FILE* rfp;
FILE* wfp;

int CPID[MAX_CHILD_PROCESS];
int KEY[MAX_CHILD_PROCESS];
int CONST_TICK_COUNT;
int TICK_COUNT;
int RUN_TIME;

int main(int argc, char* argv[]) {
	int cpuBurstTimeArray[1024];
	int ioBurstTimeArray[1024];
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
	cpu.sa_handler = &signal_cpuSchedOut;
	io.sa_handler = &signal_ioSchedIn;

	sigaction(SIGALRM, &tick, NULL);
	sigaction(SIGUSR1, &cpu, NULL);
	sigaction(SIGUSR2, &io, NULL);

	InitializeList(&readyQueue);
	InitializeList(&subReadyQueue);
	InitializeList(&waitQueue);
	InitializeNode(&cpuRunProcess);

	wfp = fopen("schedule_dump.txt", "w");
	if (wfp == NULL) {
		perror("file open error");
		exit(EXIT_FAILURE);
	}
	fclose(wfp);

	for (int innerLoopIndex = 0; innerLoopIndex < MAX_CHILD_PROCESS; innerLoopIndex++) {
		KEY[innerLoopIndex] = 0x3216 * (innerLoopIndex + 1);
		msgctl(msgget(KEY[innerLoopIndex], IPC_CREAT | 0666), IPC_RMID, NULL);
	}

	if (argc == 1 || argc == 2) {
		printf("COMMAND <TEXT FILE> <RUN TIME(sec)>\n");
		printf("./RR time_set.txt 10\n");
		exit(EXIT_SUCCESS);
	}
	else {
		rfp = fopen((char*)argv[1], "r");
		if (rfp == NULL) {
			perror("file open error");
			exit(EXIT_FAILURE);
		}

		int tempCpuBurstTime;
		int tempIoBurstTime;

		for (int innerLoopIndex = 0; innerLoopIndex < 1024; innerLoopIndex++) {
			if (fscanf(rfp, "%d , %d", &tempCpuBurstTime, &tempIoBurstTime) == EOF) {
				printf("fscanf error");
				exit(EXIT_FAILURE);
			}
			cpuBurstTimeArray[innerLoopIndex] = tempCpuBurstTime;
			ioBurstTimeArray[innerLoopIndex] = tempIoBurstTime;
		}

		RUN_TIME = atoi(argv[2]) * 1000000 / TIME_TICK;
		CONST_TICK_COUNT = 0;
		TICK_COUNT = 0;
	}
	printf("\x1b[33m");
	printf("TIME TICK   PROC NUMBER   REMAINED CPU TIME\n");
	printf("\x1b[0m");

	for (int outerLoopIndex = 0; outerLoopIndex < MAX_CHILD_PROCESS; outerLoopIndex++) {
		int ret = fork();

		if (ret > 0) {
			CPID[outerLoopIndex] = ret;
			Process p = ScanProcess(outerLoopIndex, ret, cpuBurstTimeArray[outerLoopIndex], ioBurstTimeArray[outerLoopIndex]);
			InsertRear(&readyQueue, &p);
		}
		else {
			int RANDOM = 1;
			int procNum = outerLoopIndex;
			int cpuBurstTime = cpuBurstTimeArray[procNum];
			int ioBurstTime = ioBurstTimeArray[procNum];
			kill(getpid(), SIGSTOP);

			while (true) {
				cpuBurstTime--;
				printf("            %02d            %02d\n", procNum, cpuBurstTime);
				printf("───────────────────────────────────────────\n");

				if (cpuBurstTime != 0) {
					kill(ppid, SIGUSR1);
				}
				else {
					cmsgSnd(KEY[procNum], cpuBurstTime, ioBurstTime);
					cpuBurstTime = cpuBurstTimeArray[procNum + (RANDOM * 10)];
					ioBurstTime = ioBurstTimeArray[procNum + (RANDOM * 10)];
					RANDOM = (RANDOM + 1) % 100;
					kill(ppid, SIGUSR2);
				}
				kill(getpid(), SIGSTOP);
			}
		}

	}
	cpuRunProcess.data = readyQueue.head->data;
	RemoveFront(&readyQueue);
	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
	while (RUN_TIME != 0);

	for (int innerLoopIndex = 0; innerLoopIndex < MAX_CHILD_PROCESS; innerLoopIndex++) {
		msgctl(msgget(KEY[innerLoopIndex], IPC_CREAT | 0666), IPC_RMID, NULL);
		kill(CPID[innerLoopIndex], SIGKILL);
	}

	Clear(&readyQueue);
	Clear(&subReadyQueue);
	Clear(&waitQueue);
	return 0;
}

void signal_timeTick(int signo) {
	CONST_TICK_COUNT++;
	printf("%05d       PROC NUMBER   REMAINED CPU TIME\n", CONST_TICK_COUNT);

	Node* ptr = waitQueue.head;
	while (ptr != NULL) {
		ptr->data.ioBurstTime--;
		ptr = ptr->next;
	}

	while (Search(&waitQueue, 0, ProcessIoCmp) != NULL) {
		InsertRear(&readyQueue, &waitQueue.crnt->data);
		RemoveCurrent(&waitQueue);
	}

	kill(CPID[cpuRunProcess.data.proNum], SIGCONT);
	RUN_TIME--;
}

void signal_cpuSchedOut(int signo) {
	TICK_COUNT++;

	if (TICK_COUNT == TIME_QUANTUM / TIME_TICK) {
		InsertRear(&readyQueue, &cpuRunProcess.data);
		cpuRunProcess.data = readyQueue.head->data;
		RemoveFront(&readyQueue);
		TICK_COUNT = 0;
	}
}

void signal_ioSchedIn(int signo) {
	pmsgRcv(cpuRunProcess.data.proNum, &cpuRunProcess);

	if (cpuRunProcess.data.ioBurstTime == 0)
		InsertRear(&readyQueue, &cpuRunProcess.data);
	else
		InsertRear(&waitQueue, &cpuRunProcess.data);

	cpuRunProcess.data = readyQueue.head->data;
	RemoveFront(&readyQueue);
	TICK_COUNT = 0;
}

void cmsgSnd(int key, int cpuBurstTime, int ioBurstTime) {
	int qid = msgget(key, IPC_CREAT | 0666);

	struct msgbuf msg;
	memset(&msg, 0, sizeof(msg));

	msg.type = 1;
	msg.data.pid = getpid();
	msg.data.cpuTime = cpuBurstTime;
	msg.data.ioTime = ioBurstTime;

	if (msgsnd(qid, (void*)&msg, sizeof(Message), 0) == -1) {
		perror("child msgsnd error");
		exit(EXIT_FAILURE);
	}
}

void pmsgRcv(int procNum, Node* ptr) {
	int key = 0x3216 * (procNum + 1);
	int qid = msgget(key, IPC_CREAT | 0666);

	struct msgbuf msg;
	memset(&msg, 0, sizeof(msg));

	if (msgrcv(qid, (void*)&msg, sizeof(msg), 0, 0) == -1) {
		perror("msgrcv error");
		exit(1);
	}

	ptr->data.pid = msg.data.pid;
	ptr->data.cpuBurstTime = msg.data.cpuTime;
	ptr->data.ioBurstTime = msg.data.ioTime;
}
