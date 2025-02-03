#ifndef __SERVER_HANDLE_TABLE_H__
#define __SERVER_HANDLE_TABLE_H__

#define MAX_HANDLE_LEN 101


void addHandle(const char *handleName, int socketNum);
void removeHandle(const char *handleName);
char* getHandle(int socketNum);
int findHandle(const char *handleName);
void printHandle();

#endif
