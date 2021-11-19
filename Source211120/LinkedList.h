#ifndef __LinkedList
#define __LinkedList

#include "Process.h"

/* 노드 */
typedef struct node {
	Process data;
	struct node* next;
} Node;

/* 연결 리스트 */
typedef struct {
	Node* head;
	Node* crnt;
} List;

/* 노드를 초기화 */
void InitializeNode(Node* node);

/* 연결 리스트를 초기화 */
void InitializeList(List* list);

/* 함수 compare로 x와 같은 노드를 검색 */
Node* Search(List* list, int y, int compare(const Process* x, int y));

/* 머리에 노드를 삽입 */
void InsertFront(List* list, const Process* x);

/* 꼬리에 노드를 삽입 */
void InsertRear(List* list, const Process* x);

/* 머리 노드를 삭제 */
void RemoveFront(List* list);

/* 꼬리 노드를 삭제 */
void RemoveRear(List* list);

/* 선택한 노드를 삭제 */
void RemoveCurrent(List* list);

/* 모든 노드를 삭제 */
void Clear(List* list);

/* 선택한 노드의 데이터를 출력 */
void PrintCurrent(const List* list);

/* 선택한 노드의 데이터를 출력(줄 바꿈 문자 포함) */
void PrintLnCurrent(const List* list);

/* 모든 노드의 데이터를 리스트 순서대로 출력 */
void Print(const List* list);

/* 연결 리스트를 종료 */
void Terminate(List* list);

#endif
