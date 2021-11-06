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
#define TICK_TIME 1000000		//micro sec 1000000 = 1sec
#define QUANTUM_TIME 3

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
	int pid;// �ڽ� ���μ����� id.
	int cpuTime;
	int ioTime;// �ڽ��� io time.
};// �޼��� ť�� ����  �ڽ� ���μ�����  ������.

struct msgbuf {
	long mtype;// ������ �־�� �ϴ� mtype.
	struct data mdata;
};// �޼��� ť�� ���� ������.

//Function Define
void signal_bustend(int signo);
void signal_decrease(int signo);
void addnode(list* list, int proc_num);
int delnode(list* list, node* return_node);
void inilist(list* list);
void signal_handler(int signo);

//Global Parameter
int child_proc_num = 0;		//���Ǹ� ���� �ڽ� ���μ��� ��ȣ����
//child_proc_num �� ���� �� �ڽ� ���μ����� bust_time[child_proc_num]���� ���� ����Ʈ Ÿ�� ���

int bust_time[MAX_PROC];
int iobust_time[MAX_PROC];
int pids[MAX_PROC];
int parents_pid;

list* WaitingQ;
list* ReadyQ;
node* RunningQ;

//��ĥ �ڵ�
void pmsgSnd(int curProc);
void pmsgRcv(int curProc);
void cmsgSnd(int* ckey);
void cmsgRcv(int* ckey);
void signalHandler(int signo);

int curProc = 1;// ���� ���� ���� �ڽ� ���μ���.
int tickCount = 0; //Ÿ�� ī����
int quantumCount = 0;//ƽ ī����.
int cpid[3];// parent holds child process pid array.
int cqid[3];// parent holds message queue id array.
int* ckey[3];// child holds child process key array.
//��ĥ �ڵ�

int main(int argc, char* arg[]) {
	int ret;
	int temp;

	struct sigaction new_sa;
	memset(&new_sa, 0, sizeof(new_sa));
	new_sa.sa_handler = &signal_handler;
	sigaction(SIGALRM, &new_sa, NULL);

	struct itimerval new_itimer, old_itimer;
	new_itimer.it_interval.tv_sec = 1;
	//new_itimer.it_interval.tv_usec = TICK_TIME;
	new_itimer.it_value.tv_sec = 1;
	new_itimer.it_value.tv_usec = 0;

	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = &signal_decrease;
	sigaction(SIGUSR1, &act, NULL);

	struct sigaction act2;
	memset(&act2, 0, sizeof(act2));
	act2.sa_handler = &signal_bustend;
	sigaction(SIGUSR2, &act2, NULL);

	ReadyQ = malloc(sizeof(list));
	WaitingQ = malloc(sizeof(list));
	RunningQ = malloc(sizeof(node));

	inilist(ReadyQ);
	inilist(WaitingQ);

	//CPU bust Ÿ�� ���� ����
	//�߰� �� �� : setting.txt ���ϵ����� �̸� ���õ� ������ �ҷ��ͼ� ���� or ���������� ���α׷� ���۽� �������� ������ �������� ����(���)
	bust_time[0] = 100;
	bust_time[1] = 500;
	bust_time[2] = 100;
	bust_time[3] = 200;
	bust_time[4] = 100;
	bust_time[5] = 300;
	bust_time[6] = 700;
	bust_time[7] = 400;
	bust_time[8] = 100;
	bust_time[9] = 600;

	//IO bust Ÿ�� ���� ����
	iobust_time[0] = 100;
	iobust_time[1] = 500;
	iobust_time[2] = 100;
	iobust_time[3] = 200;
	iobust_time[4] = 100;
	iobust_time[5] = 300;
	iobust_time[6] = 700;
	iobust_time[7] = 400;
	iobust_time[8] = 100;
	iobust_time[9] = 600;

	parents_pid = getpid();

	//�ʱ� �ڽ� ���μ��� ���� ���� - �θ� ���μ���(ó�� 1���� ����)
	for (int i = 0; i < 10; i++) {
		//sleep(1);
		ret = fork();
		if (ret > 0) {								//�θ� ���μ���
			child_proc_num++;
			pids[i] = ret;
			addnode(ReadyQ, i);

		}
		//�ʱ� �ڽ� ���μ��� ���� ����

			//�ڽ� ���μ��� �ڵ� ���� - �θ� ���μ����� kill �ñ׳� Ȥ�� ���� �ڽ� ���μ����� ���� ���Ǳ��� �ݺ� �� ����
		else if (ret == 0) {						//�ڽ� ���μ���
			//printf("pid[%d] = proc num [%d]\n",getpid(),child_proc_num);		//����� ��
			printf("pid[%d] : stop\n", getpid());
			//raise(SIGSTOP);
			kill(getpid(), SIGSTOP);

			while (1) {								//������ ������ �ѹ� ���� �� �ڽ� ���μ����� �ٸ� �ڽ� ���μ��� ���� ����
				printf("pid[%d] : work\n", getpid());

				temp = bust_time[child_proc_num];

				if (bust_time[child_proc_num] - 100 == 0) {
					bust_time[child_proc_num] = bust_time[child_proc_num] - 100;
					printf("pids[%d] = cpu_bust decrease %d - 100 = %d\n", getpid(), temp, bust_time[child_proc_num]);
					kill(parents_pid, SIGUSR2);
				}
				else {
					bust_time[child_proc_num] = bust_time[child_proc_num] - 100;
					printf("pids[%d] = cpu_bust decrease %d - 100 = %d\n", getpid(), temp, bust_time[child_proc_num]);
					kill(parents_pid, SIGUSR1);
				}
				printf("pid[%d] : work done\n", getpid());
				kill(getpid(), SIGSTOP);
			}
		}
		//�ڽ� ���μ��� �ڵ� ����
	}

	//�θ� ���μ��� �ڵ� ���� - �ñ׳� ���� �ڽ� ���μ��� ���� �� ����

	delnode(ReadyQ, RunningQ);
	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
	while(1) {
		//addnode(ReadyQ, RunningQ->proc_num);

		if (ReadyQ->head == NULL && ReadyQ->tail == NULL) {
			break;
		}
	}

	//�߰��� �� : ���α׷��� ���� ����(��� �ڽ� ���μ����� CPU bust�� 0)
	for (int i = 0; i < 10; i++) {
		kill(pids[i], SIGKILL);
		printf("sigkill\n");
	}
	//�θ� ���μ��� �ڵ� ����

	return 0;
}

void signal_handler(int signo)				//�θ� ���μ������� �۵� SIGALRM
{
	int child_proc;
	child_proc = pids[RunningQ->proc_num];		//Running Queue�� �ڽ� ���μ��� pid ����
	kill(pids[RunningQ->proc_num], SIGCONT);

}

void signal_decrease(int signo) {			//�������� �ڽ� ���μ������� �۵� SIGUSR1
	if (tickCount < QUANTUM_TIME-1) {
		tickCount++;
	}
	else {
		addnode(ReadyQ, RunningQ->proc_num);
		delnode(ReadyQ, RunningQ);
		tickCount = 0;
	}
}

void signal_bustend(int signo) {			//�ڽ� ���μ��� -> �θ� ���μ��� �ñ׳� ���� �׽�Ʈ SIGUSR2
	printf("SIGUSR2 called1\n");
	addnode(WaitingQ, RunningQ->proc_num);
	delnode(ReadyQ, RunningQ);

	tickCount = 0;
	printf("SIGUSR2 called2\n");
}

//��ĥ �ڵ�
void pmsgSnd(int curProc) {
	int qid;
	int ret;
	int key = 0x1000 * curProc;
	struct msgbuf msg;

	qid = msgget(key, IPC_CREAT | 0666);
	cqid[curProc - 1] = qid;
	memset(&msg, 0, sizeof(msg));

	msg.mtype = 1;
	msg.mdata.pid = 0;
	msg.mdata.cpuTime = 0;
	msg.mdata.ioTime = 0;

	if (ret = msgsnd(qid, (void*)&msg, sizeof(struct data), 0) == -1) {
		perror("pmsgsnd error");
		exit(1);
	}
	return;
}

// �θ� ���� �޽����� �ڽ��� �޴´�.
void cmsgRcv(int* ckey) {
	int qid;
	int ret;
	int key = *ckey;// �ڽ� ���μ��� ������ Ű ��.
	struct msgbuf msg;

	qid = msgget(key, IPC_CREAT | 0666);
	memset(&msg, 0, sizeof(msg));

	if (ret = msgrcv(qid, (void*)&msg, sizeof(msg), 0, 0) == -1) {
		perror("cmsgrcv error");
		exit(1);
	}
	return;
}

// �ڽ��� �θ𿡰� �ڽ��� �����Ͱ� ���  �޽����� ������.
void cmsgSnd(int* ckey) {
	int qid;
	int ret;
	int key = *ckey;// �ڽ� ���μ��� ������ Ű ��.
	struct msgbuf msg;

	qid = msgget(key, IPC_CREAT | 0666);
	memset(&msg, 0, sizeof(msg));

	msg.mtype = 1;
	msg.mdata.pid = getpid();
	msg.mdata.cpuTime = 100;
	msg.mdata.ioTime = 100;// io time�� �θ𿡰� ������.

	if (ret = msgsnd(qid, (void*)&msg, sizeof(struct data), 0) == -1) {
		perror("msgsnd error");
		exit(1);
	}
	return;
}

// �ڽ��� ���� �޽����� ���� �θ�� �� �����͸� ����Ѵ�.
void pmsgRcv(int curProc) {
	int qid;
	int ret;
	int key = 0x1000 * curProc;
	struct msgbuf msg;

	qid = msgget(key, IPC_CREAT | 0666);
	memset(&msg, 0, sizeof(msg));

	if (ret = msgrcv(qid, (void*)&msg, sizeof(msg), 0, 0) == -1) {
		perror("msgrcv error");
		exit(1);
	}
	printf("%ld\t%d\t%d\t\t%d\n", msg.mtype, msg.mdata.pid, msg.mdata.cpuTime, msg.mdata.ioTime);
	return;
}




//����� �и��� �Լ��� - 1	(���� list.h)

void inilist(list* list) {
	list->head = NULL;
	list->tail = NULL;
	list->list_num = 0;
}

void addnode(list* list, int proc_num) {
	node* addnode = (node*)malloc(sizeof(node));
	addnode->next = NULL;
	addnode->proc_num = proc_num;
	if (list->head == NULL) {				//ù��° ����϶� list->head = list->tail
		list->head = addnode;
		list->tail = addnode;
		printf("Add first node\n");
	}
	else {									//ù��° ��尡 �ƴϸ� ������ ��� ������Ʈ
		list->tail->next = addnode;
		list->tail = addnode;
		printf("Add node\n");
	}
}

int delnode(list* list, node* return_node) {
	int proc_num;
	node* delnode;

	if (list->head == NULL) {				//����ִ� ����Ʈ ������ ����ó��
		printf("There is no node to delete\n");
		return 0;							//����
	}
	delnode = list->head;
	proc_num = list->head->proc_num;
	if (list->head->next == NULL) {			//������ ��带 ����� list�� NULL�� �ʱ�ȭ
		list->head = NULL;
		list->tail = NULL;
		printf("Delete last node\n");
	}
	else {
		list->head = delnode->next;
		printf("Delete node\n");
	}
	*return_node = *delnode;
	free(delnode);

	return 1;								//����
}
//����� �и��� �Լ��� - 1
