#ifndef __SERVER_HANDLE_TABLE_H__
#define __SERVER_HANDLE_TABLE_H__

#define MAX_HANDLE_LEN 101

typedef struct Node{
    char handleName[MAX_HANDLE_LEN];  
    int socketNum;
    struct Node *next;
} Node;

void addHandle(const char *handleName, int socketNum);
void removeHandle(const char *handleName);
char* getHandle(int socketNum);
int findHandle(const char *handleName);
Node *getHead();


#endif
