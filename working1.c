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
#define TICK_TIME 10000		//micro sec 1000000 = 1sec
#define QUANTUM_TIME 2

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
void writenode(list* list, FILE* fp, char* listname);
void writeallnode(list* ready, list* waiting, node* running, FILE* fp);

void inilist(list* list);
void signal_handler(int signo);

//Global Parameter
int child_proc_num = 0;		//���Ǹ� ���� �ڽ� ���μ��� ��ȣ����
//child_proc_num �� ���� �� �ڽ� ���μ����� bust_time[child_proc_num]���� ���� ����Ʈ Ÿ�� ���

int bust_time[MAX_PROC];
int iobust_time[MAX_PROC];
int pids[MAX_PROC];
int parents_pid;
int WaitingQ_num;

list* WaitingQ;
list* ReadyQ;
node* RunningQ;
node* IORunningQ;

FILE* fp;

int running_ticks = 0;
int tickCount = 0; //Ÿ�� ī����
//int messagechk[MAX_PROC];

//��ĥ �ڵ�
void pmsgSnd(int curProc);
//void pmsgRcv(int curProc);
//void cmsgSnd(int* ckey);
void cmsgRcv(int* ckey);

void cmsgSnd(int ckey, int iobust_time);
void pmsgRcv(int curProc, int* iobust_time);

void signalHandler(int signo);

int curProc = 1;// ���� ���� ���� �ڽ� ���μ���.
int quantumCount = 0;//ƽ ī����.
int cpid[MAX_PROC];// parent holds child process pid array.
int cqid[MAX_PROC];// parent holds message queue id array.
int ckey[MAX_PROC];// child holds child process key array.
//��ĥ �ڵ�

int main(int argc, char* arg[]) {
	int ret;
	int temp;

	fp = fopen("schedule_dump.txt", "w");

	//memset(&messagechk, 0, sizeof(messagechk));

	struct sigaction new_sa;
	memset(&new_sa, 0, sizeof(new_sa));
	new_sa.sa_handler = &signal_handler;
	sigaction(SIGALRM, &new_sa, NULL);

	struct itimerval new_itimer, old_itimer;
	if (TICK_TIME > 999999) {
		new_itimer.it_interval.tv_sec = TICK_TIME/1000000;
	}
	//new_itimer.it_interval.tv_sec = 0;
	new_itimer.it_interval.tv_usec = TICK_TIME % 1000000;
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
	IORunningQ = malloc(sizeof(node));

	inilist(ReadyQ);
	inilist(WaitingQ);

	for (int i = 0; i < MAX_PROC; i++) {						//������ ����ߴ� �޽��� ť�� ��� ���� ����(���Ѵ�� ���⶧���� ���α׷� ������ �ʱ�ȭ �Ұ���)
		ckey[i] = 0x1000 * (i + 1);
		cqid[i] = msgget(ckey[i], IPC_CREAT | 0666);
		msgctl(cqid[i], IPC_RMID, NULL);
	}

	//CPU bust Ÿ�� ���� ����
	//�߰� �� �� : setting.txt ���ϵ����� �̸� ���õ� ������ �ҷ��ͼ� ���� or ���������� ���α׷� ���۽� �������� ������ �������� ����(����)
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
	WaitingQ_num = 0;

	//�ʱ� �ڽ� ���μ��� ���� ���� - �θ� ���μ���(ó�� 1���� ����)
	for (int i = 0; i < 10; i++) {
		//sleep(1);
		ckey[i] = 0x1000 * (i + 1);
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
				if (bust_time[child_proc_num] == 0) {				//IO_bust part

					//�ӽ��ڵ�	:	�������� �����ϵ��� �����ؾ�
					if (iobust_time[child_proc_num] == 0) {
						iobust_time[child_proc_num] = 500;
					}
					//

					printf("pid[%d] : work1\n", getpid());
					temp = iobust_time[child_proc_num];
					iobust_time[child_proc_num] = iobust_time[child_proc_num] - 100;
					printf("pids[%d] = io_bust decrease %d - 100 = %d\n", getpid(), temp, iobust_time[child_proc_num]);
					//send parents message left io_bust time
					cmsgSnd(ckey[child_proc_num], iobust_time[child_proc_num]);

					//�ӽ��ڵ�	:	�������� �����ϵ��� �����ؾ�
					if (iobust_time[child_proc_num] == 0) {
						bust_time[child_proc_num] = 500;
					}
					//
				}
				else {												//CPU_bust part

					printf("\npid[%d] : work2\n", getpid());
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
				}
				kill(getpid(), SIGSTOP);
			}
		}
		//�ڽ� ���μ��� �ڵ� ����
	}

	//�θ� ���μ��� �ڵ� ���� - �ñ׳� ���� �ڽ� ���μ��� ���� �� ����

	delnode(ReadyQ, RunningQ);
	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
	while(1) {

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
	char ready[] = "ReadyQ";

	//writenode(ReadyQ, fp, ready);
	writeallnode(ReadyQ, WaitingQ, RunningQ, fp);

	for (int i = 0; i < WaitingQ_num; i++) {						//���� WaitingQ�� ��� ������ io_bust���� �ڵ�
		printf("iobust check\n");
		delnode(WaitingQ, IORunningQ);
		kill(pids[IORunningQ->proc_num], SIGCONT);					//WaitingQ�� �Ǿ� ����� io_bust�� ���� -> �ڽ� ���μ����� io_bust���� �ڵ��

		//���� �ʿ� -> �Ƹ� �۵���
		printf("parents Receive message %d\n", IORunningQ->proc_num);
		pmsgRcv(IORunningQ->proc_num, iobust_time);		//receive message from child
		//messagechk[IORunningQ->proc_num] = 0;
		printf("pid[%d] = iobust_time left : %d\n",pids[IORunningQ->proc_num], iobust_time[IORunningQ->proc_num]);

		if (iobust_time[IORunningQ->proc_num] == 0) {
			addnode(ReadyQ, IORunningQ->proc_num);
			WaitingQ_num--;
			printf("2Current WaitingQ_num = %d\n", WaitingQ_num);
		}
		else {
			addnode(WaitingQ, IORunningQ->proc_num);
		}
		//
	}

	//�ڽ� ���μ����� cpu_bust ���� �κ�
	if (RunningQ->proc_num != -1) {							//����ó�� ReadyQ�� ������� ��� -> ���� WaitingQ�� �ִ°��
		child_proc = pids[RunningQ->proc_num];				//Running Queue�� �ڽ� ���μ��� pid ����
		kill(pids[RunningQ->proc_num], SIGCONT);			//ReadyQ�� �Ǿ� ����� cpu_bust�� ���� -> �ڽ� ���μ����� cpu_bust���� �ڵ��
	}
}

void signal_decrease(int signo) {			//�������� �ڽ� ���μ������� �۵� SIGUSR1 -> ���� cpu_bust�� 0�� �ƴ� -> ReadyQ�� �ڷ�
	if (tickCount < QUANTUM_TIME-1) {		//������ �ȳ��� -> RunningQ ����
		tickCount++;
	}
	else {									//������ ���� -> ReadyQ�� �ڷ� -> RunningQ ������Ʈ
		addnode(ReadyQ, RunningQ->proc_num);
		delnode(ReadyQ, RunningQ);
		tickCount = 0;
	}
}

void signal_bustend(int signo) {			//�������� �ڽ� ���μ������� �۵� SIGUSR2 -> ���� cpu_bust�� 0�� -> WaitingQ�� �ڷ�
	int temp = 0;
	addnode(WaitingQ, RunningQ->proc_num);
	WaitingQ_num ++;
	printf("1Current WaitingQ_num = %d\n", WaitingQ_num);
	temp = delnode(ReadyQ, RunningQ);
	if (temp == 0) {						//����ó�� ReadyQ�� ����ִ°�� -> ��� ���μ����� WaitingQ�� �ִ°��
		RunningQ->proc_num = -1;
	}

	tickCount = 0;							//���� �ʱ�ȭ
}

//��ĥ �ڵ�
void pmsgSnd(int curProc) {				//->��� X
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
void cmsgRcv(int* ckey) {				//->��� X
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
void cmsgSnd(int ckey, int iobust_time) {
	int qid;
	int ret;
	int key = ckey;// �ڽ� ���μ��� ������ Ű ��.
	struct msgbuf msg;

	qid = msgget(key, IPC_CREAT | 0666);
	memset(&msg, 0, sizeof(msg));

	msg.mtype = 1;
	msg.mdata.pid = getpid();
	msg.mdata.cpuTime = 0;			//->��� X
	msg.mdata.ioTime = iobust_time;	// io time�� �θ𿡰� ������.
	printf("send c->p iobust_time : %d\n",iobust_time);

	if (ret = msgsnd(qid, (void*)&msg, sizeof(struct data), 0) == -1) {
		perror("msgsnd error");
		exit(1);
	}
	return;
}

// �ڽ��� ���� �����ͷ� �ڽ� iobust_time ������Ʈ
void pmsgRcv(int curProc, int* iobust_time) {
	int qid;
	int ret;
	int key = 0x1000 * (curProc + 1);
	struct msgbuf msg;

	qid = msgget(key, IPC_CREAT | 0666);
	memset(&msg, 0, sizeof(msg));

	if (ret = msgrcv(qid, (void*)&msg, sizeof(msg), 0, 0) == -1) {
		perror("msgrcv error");
		exit(1);
	}


	*(iobust_time + curProc) = msg.mdata.ioTime;

	//printf("%ld\t%d\t%d\t\t%d\n", msg.mtype, msg.mdata.pid, msg.mdata.cpuTime, msg.mdata.ioTime);
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
		//printf("Add first node\n");
	}
	else {									//ù��° ��尡 �ƴϸ� ������ ��� ������Ʈ
		list->tail->next = addnode;
		list->tail = addnode;
		//printf("Add node\n");
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
		//printf("Delete last node\n");
	}
	else {
		list->head = delnode->next;
		//printf("Delete node\n");
	}
	*return_node = *delnode;
	free(delnode);

	return 1;								//����
}

void writenode(list* list, FILE* fp, char* listname) {
	node* nodepointer;
	fprintf(fp, "\nRunning tick = %d\n", running_ticks);
	running_ticks++;
	if (list->head == NULL) {	//list empty
		return;
	}
	nodepointer = list->head;
	for (int i = 0; ; ) {
		fprintf(fp, "%s[%d] = %d\n", listname, i, nodepointer->proc_num);
		if (nodepointer->next == NULL) {
			return;
		}
		else {
			nodepointer = nodepointer->next;
			i++;
		}
	}
}

void writeallnode(list* ready, list* waiting, node* running, FILE* fp) {
	node* nodePtr1 = ready->head;
	node* nodePtr2 = waiting->head;

	fprintf(fp, "\n");
	fprintf(fp, "┌──────┬───────┬─────────────────┬─────────────┬───────────────┐\n");
	fprintf(fp, "│ TICK │ INDEX │ RUNNING PROCESS │ READY QUEUE │ WAITING QUEUE │\n");
	running_ticks++;

	for (int i = 0; i < MAX_PROC; i++) {
		// print index, running process.
		if (i == 0) {
			if (running->proc_num == -1) {
				fprintf(fp, "│ %04d │ %02d    │ none            ", running_ticks, i);
			}
			else {
				fprintf(fp, "│ %04d │ %02d    │ %02d              ", running_ticks, i, running->proc_num);
			}
		}
		else {
			fprintf(fp, "│      │ %02d    │                 ", i);
		}

		// print ready queue.
		if (nodePtr1 == NULL) {
			if (i == 0) {
				fprintf(fp, "│ none        ");
			}
			else {
				fprintf(fp, "│             ");
			}
		}
		else {
			fprintf(fp, "│ %02d          ", nodePtr1->proc_num);
			nodePtr1 = nodePtr1->next;
		}

		// printf waiting queue.
		if (nodePtr2 == NULL) {
			if (i == 0) {
				fprintf(fp, "│ none          │\n");
			}
			else {
				fprintf(fp, "│               │\n");
			}
		}
		else {
			fprintf(fp, "│ %02d            │\n", nodePtr2->proc_num);
			nodePtr2 = nodePtr2->next;
		}
	}
	fprintf(fp, "└──────┴───────┴─────────────────┴─────────────┴───────────────┘\n");
}
//헤더로 분리할 함수들 - 1
