#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

#define MAX_PROC 10

typedef struct node {
	struct node* next;
	int data;
} node;

typedef struct list {
	node* head;
	node* tail;
	int list_num;

} list;

void signal_bustend(int signo);

void signal_decrease(int signo);
void addnode(list* list, int data);
int delnode(list* list, node* return_node);
void inilist(list* list);
void signal_handler(int signo);
int child_proc_num = 0;		//���Ǹ� ���� �ڽ� ���μ��� ��ȣ����
//child_proc_num �� ���� �� �ڽ� ���μ����� bust_time[child_proc_num]���� ���� ����Ʈ Ÿ�� ���

int bust_time[MAX_PROC];
int pids[MAX_PROC];
int parents_pid;

list* WaitingQ;
node* RunningQ;
list* ReadyQ;

int main(int argc, char* arg[]) {
	int i;
	int ret;
	int temp;

	ReadyQ = malloc(sizeof(list));
	inilist(ReadyQ);

	WaitingQ = malloc(sizeof(list));
	inilist(WaitingQ);

	RunningQ = malloc(sizeof(node));

	struct sigaction old_sa;
	struct sigaction new_sa;
	memset(&new_sa, 0, sizeof(new_sa));
	new_sa.sa_handler = &signal_handler;
	sigaction(SIGALRM, &new_sa, &old_sa);

	struct itimerval new_itimer, old_itimer;
	new_itimer.it_interval.tv_sec = 1;
	new_itimer.it_interval.tv_usec = 0;
	new_itimer.it_value.tv_sec = 1;
	new_itimer.it_value.tv_usec = 0;

	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = &signal_decrease;
	sigaction(SIGUSR1, &act, NULL);

	struct sigaction act2;
	memset(&act, 0, sizeof(act2));
	act2.sa_handler = &signal_bustend;
	sigaction(SIGUSR2, &act2, NULL);

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

	parents_pid = getpid();

	//�ʱ� �ڽ� ���μ��� ���� ���� - �θ� ���μ���(ó�� 1���� ����)
	for (i = 0; i < 10; i++) {
		sleep(1);
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
				kill(getpid(), SIGSTOP);			//�ӽ��ڵ�
			}
		}
		//�ڽ� ���μ��� �ڵ� ����
	}

	//�θ� ���μ��� �ڵ� ���� - �ñ׳� ���� �ڽ� ���μ��� ���� �� ����

	//�߰��� �� : �ڽ� ���μ����� CPU bust�� 0�� �ɶ����� �����ٸ�

	delnode(ReadyQ, RunningQ);
	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
	while(1) {
		
		
		sleep(3);
		//addnode(ReadyQ, RunningQ->data);

		if (ReadyQ->head == NULL && ReadyQ->tail == NULL) {
			break;

		}
	}

	//for (i = 0; i < 10; i++) {
	//	delnode(ReadyQ, RunningQ);				//Queue�� POP�Ҷ� RunningQ�� �ش� ��� ����
	//	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
	//	sleep(10);
	//	kill(pids[RunningQ->data], SIGCONT);
	//}


	//�߰��� �� : ���α׷��� ���� ����(��� �ڽ� ���μ����� CPU bust�� 0)
	for (i = 0; i < 10; i++) {
		kill(pids[i], SIGKILL);
		printf("sigkill\n");
	}

	//�θ� ���μ��� �ڵ� ����

	return 0;
}

//�θ� ���μ������� cpu_bust ����
//signal_handler���� �ٽ� �ڽ� ���μ����� �ñ׳� ������ �ڽ� ���μ������� ���� �׽�Ʈ �غ���
void signal_handler(int signo)				//�θ� ���μ������� �۵� SIGALRM
{
	int child_proc;
	printf("test1\n");

	
	child_proc = pids[RunningQ->data];		//Running Queue�� �ڽ� ���μ��� pid ����
	kill(pids[RunningQ->data], SIGCONT);

	//kill(child_proc, SIGUSR1);				//�ڽ� ���μ����� SIGUSR1 �ñ׳� ���� -> sigaction���� signal_decrease�Լ� ȣ��
}

//void signal_decrease(int signo) {			//�������� �ڽ� ���μ������� �۵� SIGUSR1
//	int temp;
//	temp = bust_time[child_proc_num];
//
//	if (bust_time[child_proc_num] == 0) {
//		kill(parents_pid, SIGUSR2);
//	}
//	else {
//		bust_time[child_proc_num] = bust_time[child_proc_num] - 100;
//		printf("pids[%d] = cpu_bust decrease %d - 100 = %d\n", getpid(), temp, bust_time[child_proc_num]);
//	}
//}

void signal_decrease(int signo) {			//�������� �ڽ� ���μ������� �۵� SIGUSR1
	addnode(ReadyQ, RunningQ->data);
	delnode(ReadyQ, RunningQ);
}

void signal_bustend(int signo) {			//�ڽ� ���μ��� -> �θ� ���μ��� �ñ׳� ���� �׽�Ʈ SIGUSR2
	addnode(WaitingQ, RunningQ->data);
	delnode(ReadyQ, RunningQ);
	printf("SIGUSR2 called\n");
}


//����� �и��� �Լ��� - 1	(���� list.h)

void inilist(list* list) {
	list->head = NULL;
	list->tail = NULL;
	list->list_num = 0;
}

void addnode(list* list, int data) {
	node* addnode = (node*)malloc(sizeof(node));
	addnode->next = NULL;
	addnode->data = data;
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
	int data;
	node* delnode;

	if (list->head == NULL) {				//����ִ� ����Ʈ ������ ����ó��
		printf("There is no node to delete\n");
		return 0;							//����
	}
	delnode = list->head;
	data = list->head->data;
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
