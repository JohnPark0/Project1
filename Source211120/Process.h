#ifndef __Process
#define __Process

typedef struct {
	int proNum;
	int pid;
	int cpuBurstTime;
	int ioBurstTime;
} Process;

void PrintProcess(const Process* x);

void PrintLnProcess(const Process* x);

Process ScanProcess(int procNum, int pid, int cpuBurstTime, int ioBurstTime);

int ProcessIoCmp(const Process* x, int y);

#endif
