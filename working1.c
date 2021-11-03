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
int child_proc_num = 0;		//편의를 위해 자식 프로세스 번호지정
//child_proc_num 을 통해 각 자식 프로세스가 bust_time[child_proc_num]으로 남은 버스트 타임 계산
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
	
	

	//CPU bust 타임 임의 세팅
	//추가 할 것 : setting.txt 파일등으로 미리 세팅된 파일을 불러와서 저장 or 세팅파일을 프로그램 시작시 지정하지 않으면 랜덤으로 생성(고려)
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


	//초기 자식 프로세스 생성 구간 - 부모 프로세스(처음 1번만 실행)
	for (i = 0; i < 10; i++) {
		sleep(1);
		ret = fork();
		child_proc_num++;
		if (ret > 0) {								//부모 프로세스
			pids[i] = ret;
			printf("pid[%d] : stop\n", pids[i]);
			addnode(ReadyQ, i);

		}
		//초기 자식 프로세스 생성 구간

			//자식 프로세스 코드 구간 - 부모 프로세스가 kill 시그널 혹은 일정 자식 프로세스의 일정 조건까지 반복 후 종료
		else if (ret == 0) {						//자식 프로세스
			//kill(getpid(), SIGSTOP);				//첫 실행시 자식 프로세스 스스로 정지 시그널
			raise(SIGSTOP);

			while (1) {								//루프가 없으면 한번 실행 후 자식 프로세스가 다른 자식 프로세스 무한 생성
				printf("pid[%d] : work\n", getpid());
				kill(getpid(), SIGSTOP);			//임시코드



			}
		}
		//자식 프로세스 코드 구간
	}

	//부모 프로세스 코드 구간 - 시그널 통해 자식 프로세스 통제 및 종료

	//추가할 것 : 자식 프로세스의 CPU bust가 0이 될때까지 스케줄링
	for (i = 0; i < 10; i++) {
		
		delnode(ReadyQ, RunningQ);				//Queue를 POP할때 Return_node에 해당 노드 저장
		setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
		sleep(1);
		kill(pids[RunningQ->data], SIGCONT);
	}


	//추가할 것 : 프로그램의 종료 조건(모든 자식 프로세스의 CPU bust가 0)
	for (i = 0; i < 10; i++) {
		kill(pids[i], SIGKILL);
		printf("sigkill\n");
	}

	//부모 프로세스 코드 구간

	return 0;
}

//부모 프로세스에서 cpu_bust 감소
//signal_handler에서 다시 자식 프로세스로 시그널 보내서 자식 프로세스에서 감소 테스트 해봐야
void signal_handler(int signo)
{
	int child_proc;
	int temp;
	child_proc = pids[RunningQ->data];
	temp = bust_time[RunningQ->data];
	bust_time[RunningQ->data] = bust_time[RunningQ->data] - 100;
	printf("pids[%d] bust_time - 100 (%d - 100 = %d)\n",child_proc,temp, bust_time[RunningQ->data]);
}


//헤더로 분리할 함수들 - 1	(가명 list.h)

void inilist(list* list) {
	list->head = NULL;
	list->tail = NULL;
	list->list_num = 0;
}

void addnode(list* list, int data) {
	node* addnode = (node*)malloc(sizeof(node));
	addnode->next = NULL;
	addnode->data = data;
	if (list->head == NULL) {				//첫번째 노드일때 list->head = list->tail
		list->head = addnode;
		list->tail = addnode;
		printf("Add first node\n");
	}
	else {									//첫번째 노드가 아니면 마지막 노드 업데이트
		list->tail->next = addnode;
		list->tail = addnode;
		printf("Add node\n");
	}
}

int delnode(list* list, node* return_node) {
	int data;
	node* delnode;

	if (list->head == NULL) {				//비어있는 리스트 삭제시 예외처리
		printf("There is no node to delete\n");
		return 0;							//실패
	}
	delnode = list->head;
	data = list->head->data;
	if (list->head->next == NULL) {			//마지막 노드를 지우면 list를 NULL로 초기화
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

	return 1;								//성공
}
//헤더로 분리할 함수들 - 1
