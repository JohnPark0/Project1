#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>



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

int pids[10];

int main(int argc, char* arg[]) {
	int i;
	int ret;
	list* list = malloc(sizeof(list));
	list->head = NULL;
	list->tail = NULL;
	list->list_num = 0;

	addnode(list, 1);
	addnode(list, 2);
	i = delnode(list);
	printf("%d\n", i);
	i = delnode(list);
	printf("%d\n", i);
	i = delnode(list);
	printf("%d\n", i);
	addnode(list, 3);
	addnode(list, 4);
	addnode(list, 5);
	i = delnode(list);
	printf("%d\n",i);
	i = delnode(list);
	printf("%d\n", i);
	i = delnode(list);
	printf("%d\n", i);
	i = delnode(list);
	printf("%d\n", i);


	/*for (i = 0; i < 10; i++) {
		ret = fork();
		if (ret > 0) {
			pids[i] = ret;

		}
		else if (ret == 0) {


		}
	}

	for (i = 0; i < 10; i++) {
		kill(pids[i], SIGKILL);
	}*/
	return 0;
}

void addnode(list* list, int data) {
	node* addnode = (node*)malloc(sizeof(node));
	addnode->next = NULL;
	addnode->data = data;
	if (list->head == NULL) {				//if add fist node
		list->head = addnode;
		list->tail = addnode;
		printf("Add first node\n");
	}
	else {
		list->tail->next = addnode;
		list->tail = addnode;
		printf("Add node\n");
	}
}

int delnode(list* list) {
	int data;
	node* delnode;

	if (list->head == NULL) {
		printf("There is no node to delete\n");
		return 0;
	}
	delnode = list->head;
	data = list->head->data;
	if (list->head->next == NULL) {			//if del last node
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