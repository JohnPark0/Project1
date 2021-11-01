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
int delnode(list* list);
void signal_handler(int signo);

int pids[10];

int main(int argc, char* arg[]) {
	int i;
	int ret;
	list* list = malloc(sizeof(list));
	list->head = NULL;
	list->tail = NULL;
	list->list_num = 0;
	int bust_time[MAX_PROC];
	int child_proc_num = 1;

	//CPU bust 타임 임의 세팅
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


	
	//초기 프로세스 생성 구간
	for (i = 0; i < 10; i++) {
		sleep(1);
		ret = fork();
		if (ret > 0) {				//부모 프로세스
			pids[i] = ret;
			printf("pid[%d] : stop\n",pids[i]);
			kill(pids[i], SIGSTOP);

		}
		//초기 프로세스 생성 구간 - 부모 프로세스(처음 1번만 실행)
		//자식 프로세스 코드 구간
		else if (ret == 0) {		//자식 프로세스
			printf("pid[%d] : work\n",getpid());
			while (1) {



			}
		}
		//자식 프로세스 코드 구간 - 부모 프로세스가 kill 시그널 혹은 일정 자식 프로세스의 일정 조건까지 반복 후 종료
	}

	//부모 프로세스 코드 구간

	//추가할 것 : 자식 프로세스의 CPU bust가 0이 될때까지 스케줄링
	for (i = 0; i < 10; i++) {
		sleep(1);
		//printf("pid[%d] : start\n", pids[i]);
		kill(pids[i], SIGCONT);
	}


	//추가할 것 : 프로그램의 종료 조건(모든 자식 프로세스의 CPU bust가 0)
	for (i = 0; i < 10; i++) {
		kill(pids[i], SIGKILL);
		printf("sigkill\n");
	}

	//부모 프로세스 코드 구간 - 시그널 통해 자식 프로세스 통제 및 종료
	
	return 0;
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

int delnode(list* list) {
	int data;
	node* delnode;

	if (list->head == NULL) {				//비어있는 리스트 삭제시 예외처리
		printf("There is no node to delete\n");
		return 0;							//반환값을 0말고 다른걸로 해야할듯
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
	free(delnode);

	return data;
}