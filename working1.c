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

void addnode(list* list, int data);
//int delnode(list* list);
int delnode(list* list, node* return_node);
void inilist(list* list);
void signal_handler(int signo);
int child_proc_num = 0;		//���Ǹ� ���� �ڽ� ���μ��� ��ȣ����
//child_proc_num �� ���� �� �ڽ� ���μ����� bust_time[child_proc_num]���� ���� ����Ʈ Ÿ�� ���
int bust_time[10];
node* RunningQ;
int pids[10];



int main(int argc, char* arg[]) {
	int i;
	int ret;
	
	node* Return_node = malloc(sizeof(node));
	list* ReadyQ = malloc(sizeof(list));
	inilist(ReadyQ);

	RunningQ = malloc(sizeof(node));

	/*list* list = malloc(sizeof(list));
	inilist(list);*/

	//int bust_time[MAX_PROC];

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
	
	

	//CPU bust Ÿ�� ���� ����
	//�߰� �� �� : setting.txt ���ϵ����� �̸� ���õ� ������ �ҷ��ͼ� ���� or ���������� ���α׷� ���۽� �������� ������ �������� ����(���)
	bust_time[0] = 100;
	bust_time[1] = 500;
	bust_time[2] = 100;
	bust_time[3] = 200;
	bust_time[4] = 100;
	bust_time[5] = 300;
	bust_time[6] = 100;
	bust_time[7] = 400;
	bust_time[8] = 100;
	bust_time[9] = 600;


	//�ʱ� �ڽ� ���μ��� ���� ���� - �θ� ���μ���(ó�� 1���� ����)
	for (i = 0; i < 10; i++) {
		sleep(1);
		ret = fork();
		child_proc_num++;
		if (ret > 0) {								//�θ� ���μ���
			pids[i] = ret;
			printf("pid[%d] : stop\n", pids[i]);
			addnode(ReadyQ, i);

		}
		//�ʱ� �ڽ� ���μ��� ���� ����

			//�ڽ� ���μ��� �ڵ� ���� - �θ� ���μ����� kill �ñ׳� Ȥ�� ���� �ڽ� ���μ����� ���� ���Ǳ��� �ݺ� �� ����
		else if (ret == 0) {						//�ڽ� ���μ���
			//kill(getpid(), SIGSTOP);				//ù ����� �ڽ� ���μ��� ������ ���� �ñ׳�
			raise(SIGSTOP);

			while (1) {								//������ ������ �ѹ� ���� �� �ڽ� ���μ����� �ٸ� �ڽ� ���μ��� ���� ����
				printf("pid[%d] : work\n", getpid());
				kill(getpid(), SIGSTOP);			//�ӽ��ڵ�



			}
		}
		//�ڽ� ���μ��� �ڵ� ����
	}

	//�θ� ���μ��� �ڵ� ���� - �ñ׳� ���� �ڽ� ���μ��� ���� �� ����

	//�߰��� �� : �ڽ� ���μ����� CPU bust�� 0�� �ɶ����� �����ٸ�
	for (i = 0; i < 10; i++) {
		
		delnode(ReadyQ, RunningQ);				//Queue�� POP�Ҷ� Return_node�� �ش� ��� ����
		setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
		sleep(1);
		kill(pids[RunningQ->data], SIGCONT);
	}


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
void signal_handler(int signo)
{
	int child_proc;
	int temp;
	child_proc = pids[RunningQ->data];
	temp = bust_time[RunningQ->data];
	bust_time[RunningQ->data] = bust_time[RunningQ->data] - 100;
	printf("pids[%d] bust_time - 100 (%d - 100 = %d)\n",child_proc,temp, bust_time[RunningQ->data]);
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
