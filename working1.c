#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

int pids[10];

int main(int argc, char* arg[]) {

	int i;
	int ret;
	for (i = 0; i < 10; i++) {
		ret = fork();
		if (ret > 0) {
			pids[i] = ret;

		}
		else if (ret == 0) {


		}
	}

	for (i = 0; i < 10; i++) {
		kill(pids[i], SIGKILL);
	}


	return 0;
}