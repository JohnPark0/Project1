#include <stdio.h>
#include <string.h>
#include "Process.h"

void PrintProcess(const Process* x) {
	printf("%d %d %d %d", x->proNum, x->pid, x->cpuBurstTime, x->ioBurstTime);
}

void PrintLnProcess(const Process* x) {
	printf("%d %d %d %d\n", x->proNum, x->pid, x->cpuBurstTime, x->ioBurstTime);
}

Process ScanProcess(int procNum, int pid, int cpuBurstTime, int ioBurstTime) {
	Process temp;
	temp.proNum = procNum;
	temp.pid = pid;
	temp.cpuBurstTime = cpuBurstTime;
	temp.ioBurstTime = ioBurstTime;
	return temp;
}

int ProcessIoCmp(const Process* x, int y) {
	return x->ioBurstTime == y ? 1 : 0;
}
