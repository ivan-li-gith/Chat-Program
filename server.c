/******************************************************************************
* myServer.c
* 
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>
#include "networks.h"
#include "safeUtil.h"
#include "new.h"
#include "pollLib.h"
#include "server_handle_table.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1

int checkArgs(int argc, char *argv[]);
void addNewSocket(int mainServerSocket);
void processClient(int clientSocket);
void serverControl(int mainServerSocket);
void sendPacket(int socketNum, uint8_t *payloadBuf, int payloadLen);
void forwardDm(int socketNum, uint8_t *msgBuffer, int msgLen);
void handleDNE(int socketNum, const char *destHandle);
void forwardMulticast(int socketNum, uint8_t *msgBuffer, int msgLen);
void sendHandles(int socketNum);
void forwardBroadcast(uint8_t *msgBuffer, int msgLen);

// to handle closing the server with multiple clients
volatile int activeClients = 0;

int main(int argc, char *argv[]){
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	portNumber = checkArgs(argc, argv);
	mainServerSocket = tcpServerSetup(portNumber);
	serverControl(mainServerSocket);
	close(mainServerSocket);
	return 0;
}

int checkArgs(int argc, char *argv[]){
	// Checks args and returns port number
	int portNumber = 0;

	if(argc > 2){
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if(argc == 2){
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}


void serverControl(int mainServerSocket){
	// start the poll set
	setupPollSet();

	// add main server to poll set
	addToPollSet(mainServerSocket);

	while(1){
		// call poll
		int socket = pollCall(POLL_WAIT_FOREVER);

		// if poll returns main server socket call addNewSocket else if poll returns client socket call processClient
		if(socket == mainServerSocket){
			addNewSocket(mainServerSocket);
		}else{
			processClient(socket);
		}
	}

	// close all sockets
	printf("Server shutting down\n");
    close(mainServerSocket);
}

// adds new socket and adds to linkedlist
void addNewSocket(int mainServerSocket){
	// new client is trying to connect -> accept 
	int newClientSocket = tcpAccept(mainServerSocket, DEBUG_FLAG);

	if(newClientSocket < 0){
		perror("Cant accept new client");
		return;
	}

	uint8_t responseBuf[MAXBUF];
	int responseLen = recvPDU(newClientSocket, responseBuf, MAXBUF);

	if (responseLen < 2) {
		printf("Initial packet needs at least 2 bytes");
		close(newClientSocket);
		return;
    }

	// uint8_t flag = responseBuf[0];
	uint8_t handleLen = responseBuf[1];
	char clientHandle[MAX_HANDLE_LEN];
    memcpy(clientHandle, responseBuf + 2, handleLen);
    clientHandle[handleLen] = '\0';

	// check to see if the handle exists
	if(findHandle(clientHandle) != -1) {
        uint8_t response[3];
        response[0] = 3;         	// flag = 3 (error)
        response[1] = handleLen;   	
        memcpy(response + 2, clientHandle, handleLen);
        sendPDU(newClientSocket, response, 2 + handleLen);
        close(newClientSocket);
        return;
    }

	// add to linked list
	addHandle(clientHandle, newClientSocket);
    // printf("Handle added to linked list: %s\n", clientHandle);
	// printHandle();

    uint8_t successResponse[1];
    successResponse[0] = 2;  	// flag = 2 indicates success
	// printf("Flag is set to 2");
    if(sendPDU(newClientSocket, successResponse, 1) < 0) {
        perror("Failed to send success response");
        close(newClientSocket);
        return;
    }

    // Add the new client to the poll set for future messages
    addToPollSet(newClientSocket);
    activeClients++;
}

void processClient(int clientSocket) {
    uint8_t buffer[MAXBUF];
    int bytesReceived;
	bytesReceived = recvPDU(clientSocket, buffer, MAXBUF);

	// when the client hits Control C remove the socket from the poll set
	if (bytesReceived == 0) {
		printf("Client on socket %d disconnected\n", clientSocket);
		removeFromPollSet(clientSocket);
		close(clientSocket);
		activeClients--;

		if (activeClients == 0) {
            exit(0); 
        }
		return;
	}

	if (bytesReceived < 0) {
		perror("Could not receive PDU");
		removeFromPollSet(clientSocket);
		close(clientSocket);
        activeClients--;

		if (activeClients == 0) {
            exit(0); 
        }
		return;
	}

	// printf("Message received on socket %d, length: %d Data: %s\n", clientSocket, bytesReceived, buffer);

	// check what kind of message is being sent to server
	uint8_t flag = buffer[0];
	// printf("Flag is: %d\n", flag);

	switch(flag){
		case 5:
			forwardDm(clientSocket, buffer, bytesReceived);
			break;
		case 6:
			forwardMulticast(clientSocket, buffer, bytesReceived);
			break;
		case 10:
			sendHandles(clientSocket);
			break;
		case 4:
			forwardBroadcast(buffer, bytesReceived);
			break;
		default:
			printf("Unknown flag\n");
			break;
	}
}

// calls sendPDU and checks for errors
void sendPacket(int socketNum, uint8_t *payloadBuf, int payloadLen){
	if(sendPDU(socketNum, payloadBuf, payloadLen) < 0){
		perror("Unable to send PDU");
		exit(-1);
	}
}

void forwardDm(int socketNum, uint8_t *msgBuffer, int msgLen){
	int index = 1;		// start at 1 because 0 is the flag 

	// 1 byte client handle len
	uint8_t clientHandleLen = msgBuffer[index++];

	// client handle name
	char clientHandle[clientHandleLen];
	memcpy(clientHandle, msgBuffer + index, clientHandleLen);
	clientHandle[clientHandleLen] = '\0';
	index += clientHandleLen;

	// num of destinations
	uint8_t numOfDest = msgBuffer[index++];		// dont use just increment index 

	// 1 byte len of dest handle
	uint8_t destHandleLen = msgBuffer[index++];
	
	// dest handle name
	char destHandle[destHandleLen];
	memcpy(destHandle, msgBuffer + index, destHandleLen);
	destHandle[destHandleLen] = '\0';
	index += destHandleLen;

	// text message 
	char txtMsg[MAXBUF];
	int txtMsgLen = msgLen - index;
	if(txtMsgLen > 0){
		memcpy(txtMsg, msgBuffer + index, txtMsgLen);
		txtMsg[txtMsgLen] = '\0';
	}else{
		txtMsg[0] = '\0';
	}

	int destSocket = findHandle(destHandle);

	// this means there is no socket for this handle 
	if(destSocket < 0){
		handleDNE(socketNum, destHandle);
	}else{
		sendPacket(destSocket, msgBuffer, msgLen);
	}
}


// for handles that dont exist
void handleDNE(int socketNum, const char *destHandle){
	// going to contain the message of the DNE
	uint8_t dneMsg[MAXBUF];
	int index = 0;
	uint8_t flag = 7;
	dneMsg[index++] = flag;

	// dest handle
	uint8_t destHandleLen = (uint8_t)strlen(destHandle);
	dneMsg[index++] = destHandleLen;
	memcpy(dneMsg + index, destHandle, destHandleLen);
	index += destHandleLen;
	sendPacket(socketNum, dneMsg, index); 
}

void forwardMulticast(int socketNum, uint8_t *msgBuffer, int msgLen){
	int index = 1;

	uint8_t clientHandleLen = msgBuffer[index++];
	
	// client handle name
	char clientHandle[clientHandleLen];
	memcpy(clientHandle, msgBuffer + index, clientHandleLen);
	clientHandle[clientHandleLen] = '\0';
	index += clientHandleLen;
	uint8_t numOfHandles = msgBuffer[index++];

	// add all the dest handles length and name
	for(int i = 0; i < numOfHandles; i++){
		uint8_t destHandleLen = msgBuffer[index++];
		char destHandle[destHandleLen];
		memcpy(destHandle, msgBuffer + index, destHandleLen);
		destHandle[destHandleLen] = '\0';
		index += destHandleLen;

		int destSocket = findHandle(destHandle);
		// this means there is no socket for this handle 
		if(destSocket < 0){
			handleDNE(socketNum, destHandle);
		}else{
			sendPacket(destSocket, msgBuffer, msgLen);
		}
	}
}

void sendHandles(int socketNum){
	int index = 0;
	uint8_t numOfHandleBuf[MAXBUF];
	uint8_t flag = 11;
	uint32_t numOfHandles = htonl((uint32_t)activeClients);
	numOfHandleBuf[index++] = flag;

	// printf("Flag is set to %d\n", flag);
    memcpy(numOfHandleBuf + index, &numOfHandles, sizeof(numOfHandles));
    index += sizeof(numOfHandles);
	sendPacket(socketNum, numOfHandleBuf, index); 

	// send flag 12 packets
	Node *curr = getHead();

	// if(curr == NULL){
	// 	printf("Node is null\n");
	// }

	// go through linked list and copy the handles into buffer
	while(curr != NULL){
		index = 0;
		flag = 12;
		numOfHandleBuf[index++] = flag;
		// printf("In the Node part flag should be %d\n", flag);
		uint8_t handleLen = (uint8_t)strlen(curr->handleName);
		numOfHandleBuf[index++] = handleLen;
		memcpy(numOfHandleBuf + index, curr->handleName, handleLen);
		index += handleLen;
		sendPacket(socketNum, numOfHandleBuf, index);
		curr = curr->next;
	}

	// send flag 13
	flag = 13;
	numOfHandleBuf[0] = flag;
	// printf("Finished with list so flag shoulld be %d\n", flag);
	sendPacket(socketNum, numOfHandleBuf, 1);
}

void forwardBroadcast(uint8_t *msgBuffer, int msgLen){
	int index = 1;
	uint8_t clientHandleLen = msgBuffer[index++];
	
	// client handle name
	char clientHandle[clientHandleLen];
	memcpy(clientHandle, msgBuffer + index, clientHandleLen);
	clientHandle[clientHandleLen] = '\0';
	index += clientHandleLen;

	// text message
	char txtMsg[MAXBUF];
	int txtMsgLen = msgLen - index;
	if(txtMsgLen > 0){
		memcpy(txtMsg, msgBuffer + index, txtMsgLen);
		txtMsg[txtMsgLen] = '\0';
	}else{
		txtMsg[0] = '\0';
	}

	Node *curr = getHead();
	while(curr != NULL){
		// ignore the handle that is broadcasting 
		if(strcmp(curr->handleName, clientHandle) == 0){
			curr = curr->next;
			continue;
		}
		int socketNum = findHandle(curr->handleName);
		sendPacket(socketNum, msgBuffer, msgLen);
		curr = curr->next;
	}
}
