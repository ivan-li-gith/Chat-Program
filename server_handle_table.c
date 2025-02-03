#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server_handle_table.h"

// going to use linked lists as my dynamic data structure
typedef struct Node{
    char handleName[101];   // max length is 100 + null 
    int socketNum;
    struct Node *next;
} Node;

static Node *head = NULL;   
static int clientCount = 0; 

// helper func to add client 
void addHandle(const char *handleName, int socketNum){
    Node *newNode = (Node *)malloc(sizeof(Node));

    // check if null
    if(newNode == NULL){
        perror("Could not add new node");
        exit(-1);
    }

    // enter data into the node 
    strncpy(newNode->handleName, handleName, 100);  // copy client name into node
    newNode->handleName[100] = '\0';                // add null to the end of the client name
    newNode->socketNum = socketNum;
    newNode->next = head;                           // first node made the next is going to be null 
    head = newNode;                                 // head is initially null so when you make the first node that newNode becomes the new head
    clientCount++;                                  // increment count
}

// helper func to remove client
void removeHandle(const char *handleName){
    Node *curr = head;
    Node *prev = NULL;

    while(curr != NULL){
        // if the current node is the client to be removed
        if(strcmp(curr->handleName, handleName) == 0){
            // if the client is the first node to be removed head is the next node else the previous next is going to be the node to be removed's next
            if(prev == NULL){
                head = curr->next;
            }else{
                prev->next = curr->next;
            }
            free(curr);                             // free memory for removed client
            clientCount--;                          // decrement count
            return;
        }

        // moving down the list
        prev = curr;
        curr = curr->next;
    }
}

char* getHandle(int socketNum){
    Node *curr = head;

    while(curr != NULL){
        if(curr->socketNum == socketNum){
            
            return curr->handleName;
        }
        curr = curr->next;
    }
    return NULL;
}

int findHandle(const char *handleName){
    Node *curr = head;
    while(curr != NULL){
        // finds the handle name and returns the socket it is on 
        if(strcmp(curr->handleName, handleName) == 0){
            return curr->socketNum;
        }
        curr = curr->next;
    }
    return -1;
}

// prints out the number of clients and their names
void printHandle(){
    printf("Number of clients: %d", clientCount);
    
    Node *curr = head;
    while(curr != NULL){
        printf("    %s\n", curr->handleName);
        curr = curr->next;
    }
}