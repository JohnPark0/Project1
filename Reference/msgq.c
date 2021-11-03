#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "msg.h"

int main()
{
	int msgq;
	int ret;
	int key = 0x12345;
	msgq = msgget( key, IPC_CREAT | 0666);
	printf("msgq id: %d\n", msgq);

	struct msgbuf msg;
	memset(&msg, 0, sizeof(msg));
	msg.mtype = 0;
	msg.pid = getpid();
	msg.io_time = 10;
	ret = msgsnd(msgq, &msg, sizeof(msg), NULL);
	printf("msgsnd ret: %d\n", ret);

	return 0;
}
