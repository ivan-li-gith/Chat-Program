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

void checkArgs(int argc, char * argv[]);
void clientControl(int socketNum, const char *clientHandle);
void processMsgFromServer(int socketNum);
void sendPacket(int socketNum, uint8_t *payloadBuf, int payloadLen);
void initialPacket(int socketNum, const char *clientHandle);
void processInitMsg(int socketNum, const char *clientHandle);
void readFromSTDIN(int socketNum, const char *clientHandle);
char* getDestHandle(uint8_t *userInput);
char* getTxtMsg(uint8_t *userInput);
void dmPacket(int socketNum, const char *clientHandle, const char *destHandle, const char *txtMsg);
char* getClientHandle(uint8_t *responseBuf);
int parseDM(uint8_t *msgBuffer, int msgLen, char *clientHandle, char *txtMsg);
void processHandleDne(uint8_t *responseBuf);
int multicastHandleNum(uint8_t *userInput);
void multicastHandles(const char *userInput, int numHandles, char destHandlesArray[][MAX_HANDLE_LEN]);
char* getMulticastTxtMsg(uint8_t *userInput, int numHandles);
void multicastMsg(int socketNum, const char *clientHandle, int numOfDest, char destHandles[][MAX_HANDLE_LEN], const char *txtMsg);
void printList(int socketNum, uint8_t *responseBuf, int msgLen);
char* getBroadcastText(uint8_t *userInput);
void broadcast(int socketNum, const char *clientHandle, const char *txtMsg);
int processBroadcast(uint8_t *msgBuffer, int msgLen, char *clientHandle, char *txtMsg);

int main(int argc, char * argv[]){
	int socketNum = 0;         
	checkArgs(argc, argv);
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);		// create socket for client
	char *clientHandle = argv[1];									// gets the handle of the client 
	initialPacket(socketNum, clientHandle);							// sends initial packet to server
	processInitMsg(socketNum, clientHandle);						// checks for flags 2 and 3 
	clientControl(socketNum, clientHandle);							
	close(socketNum);
	return 0;
}

void checkArgs(int argc, char * argv[]){
	/* check command line arguments  */
	if (argc != 4){
		printf("usage: %s handle server-name server-port \n", argv[0]);
		exit(1);
	}
}

void clientControl(int socketNum, const char *clientHandle){
	setupPollSet();
	addToPollSet(STDIN_FILENO);
	addToPollSet(socketNum);

	while(1){
		printf("$:");									// prompt
		fflush(stdout);									// flush out stdout
		int socket = pollCall(POLL_WAIT_FOREVER);		// wait for any triggers on sockets

		if(socket == STDIN_FILENO){
			readFromSTDIN(socketNum, clientHandle);
		}else{
			processMsgFromServer(socketNum);
		}
	}

	printf("Server shutting down\n");
    close(socketNum);
}

// takes echoed message and calls functions to handle things from server
void processMsgFromServer(int socketNum){	
    uint8_t recvBuffer[MAXBUF];
    int msgReceived = recvPDU(socketNum, recvBuffer, MAXBUF);

    if(msgReceived == 0){		// control C
        exit(0);
    }else if(msgReceived < 0) {
        perror("Error receiving from server");
        exit(-1);
    }else{
		uint8_t flag = recvBuffer[0];

		if(flag == 5 || flag == 6){
			char clientHandle[MAXBUF];
			char txtMsg[MAXBUF];

			if(parseDM(recvBuffer, msgReceived, clientHandle, txtMsg) == 0){
				printf("\n%s: %s\n", clientHandle, txtMsg);
			}else{
				printf("Cant parse message\n");
			}
		}

		if(flag == 7){
			processHandleDne(recvBuffer);
		}

		if(flag == 11){
			printList(socketNum, recvBuffer, msgReceived);
		}

		if(flag == 4){
			char clientHandle[MAXBUF];
			char txtMsg[MAXBUF];
			if(processBroadcast(recvBuffer, msgReceived, clientHandle, txtMsg) == 0){
				printf("\n%s: %s\n", clientHandle, txtMsg);
			}else{
				printf("Cant parse message\n");
			}
		}
    }
}

// calls sendPDU and checks for errors
void sendPacket(int socketNum, uint8_t *payloadBuf, int payloadLen){
	if(sendPDU(socketNum, payloadBuf, payloadLen) < 0){
		perror("Unable to send PDU");
		exit(-1);
	}
}

// makes initial packet from client to server
void initialPacket(int socketNum, const char *clientHandle){
	int index = 0;
	u_int8_t flag = 1;										
	int handleLen = strlen(clientHandle);					// len of the handle  
	uint8_t payloadBuf[MAXBUF];								// buffer to store the data
	payloadBuf[index++] = flag;								// 1 byte flag
	payloadBuf[index++] = (uint8_t)handleLen;				// 1 byte handle length -> type cast it so its 1 byte
	memcpy(payloadBuf + index, clientHandle, handleLen);	// copy over the handle
	index += handleLen;
	sendPacket(socketNum, payloadBuf, index);				// index is technically payload length
}

// handles the message with flags 2 or 3
void processInitMsg(int socketNum, const char *clientHandle){
	uint8_t responseBuf[MAXBUF];
	int responseLen = recvPDU(socketNum, responseBuf, MAXBUF);

	if(responseLen < 0){
		perror("Unable to get initial packet response\n");
		exit(-1);
	}

	uint8_t flag = responseBuf[0];

	if(flag == 2){
		// printf("Able to connect to handle\n");
	}else if(flag == 3){
		printf("Handle already in use: %s\n", clientHandle);
		exit(-1);
	}else{
		printf("Unexpected response from server\n");
		exit(-1);
	}
}

// reads input and calls functions based on commands
void readFromSTDIN(int socketNum, const char *clientHandle){
    uint8_t userInput[MAXBUF];
    int inputLen;
    inputLen = read(STDIN_FILENO, userInput, MAXBUF);		// read user input

    if (inputLen <= 1) { 
        printf("Invalid Command. Please enter something\n");
        return;
    }

    userInput[inputLen - 1] = '\0';							// replace new line with null to format string
	char command = userInput[1];							// letter for command

	switch(command){
		case 'M':
		case 'm':
			char *destHandle = getDestHandle(userInput);			// dest handle 
			char *txtMsg = getTxtMsg(userInput);					// text message
			dmPacket(socketNum, clientHandle, destHandle, txtMsg);	// send DM packet to server
			break;
		case 'C':
		case 'c':
			int numHandles = multicastHandleNum(userInput);					
			char *multicastTxtMsg = getMulticastTxtMsg(userInput, numHandles);						// multicast message
		    char destHandlesArray[9][MAX_HANDLE_LEN];
			multicastHandles((char *)userInput, numHandles, destHandlesArray);						// makes the handle array
			multicastMsg(socketNum, clientHandle, numHandles, destHandlesArray, multicastTxtMsg);	// send multicast message to server
			break;
		case 'L':
		case 'l':
			uint8_t flag[1];
			flag[0] = 10;
			sendPacket(socketNum, flag, 1);		// send over flag 10 only
			break;
		case 'B':
		case 'b':
			char *broadcastTxtMsg = getBroadcastText(userInput);	// broadcast message
			broadcast(socketNum, clientHandle, broadcastTxtMsg);	// send broadcast message to server
			break;
		default:
			printf("Invalid command. Options are M C L or B (can be lowercase as well).\n");
			break;
	}
}

// returns the name of the destination handle for DM packet
char* getDestHandle(uint8_t *userInput){
	static char clientNameBuffer[MAX_HANDLE_LEN];
	char *firstSpace = strchr((char *)userInput, ' ');			// find first space in user input
	char *secondSpace = strchr(firstSpace + 1, ' ');			// find second space
	int clientNameLen = secondSpace - (firstSpace + 1);			// get dest handle length
	strncpy(clientNameBuffer, firstSpace + 1, clientNameLen);	// copy dest handle name into buffer
	clientNameBuffer[clientNameLen] = '\0';						// add null in end
	return clientNameBuffer;
}

// return the text message for DM packet
char* getTxtMsg(uint8_t *userInput){
	static char txtMsg[200];
	txtMsg[0] = '\0'; 										// make it into an empty string
	char *message = strchr((char *)userInput + 3, ' '); 	// move to where text message begins + 1
	message++;	

	if(message == NULL || *(message + 1) == '\0'){
        return txtMsg; 										// return empty string if no message
    }

	strncpy(txtMsg, message, 199);							// copy message into buffer
	txtMsg[199] = '\0'; 									// add null
	return txtMsg;
}

// send a DM to another client -> goes to server first so it can find the dest handle socket number
void dmPacket(int socketNum, const char *clientHandle, const char *destHandle, const char *txtMsg){
	uint8_t dmBuf[MAXBUF];
	int index = 0;
	uint8_t flag = 5;
	uint8_t clientHandleLen = strlen(clientHandle);			// handle len
	uint8_t numOfDest = 1;									// for DM its a value of 1
	uint8_t destHandleLen = strlen(destHandle);				// dest handle len

	// making the DM packet
	dmBuf[index++] = flag;
	dmBuf[index++] = clientHandleLen;
	memcpy(dmBuf + index, clientHandle, clientHandleLen);	// add handle name
	index += clientHandleLen;
	dmBuf[index++] = numOfDest;
	dmBuf[index++] = destHandleLen;
	memcpy(dmBuf + index, destHandle, destHandleLen);		// add dest handle name
	index += destHandleLen;
	int txtMsgLen = strlen(txtMsg) + 1;						// add 1 for null character
	memcpy(dmBuf + index, txtMsg, txtMsgLen);				// add text message
	index += txtMsgLen;
	sendPacket(socketNum, dmBuf, index);
}

// server is going to send the DM to the appropriate socket and parse that buffer that was just sent
int parseDM(uint8_t *msgBuffer, int msgLen, char *clientHandle, char *txtMsg){
	int index = 0;		
	index++;
	// uint8_t flag = msgBuffer[index++];						// dont use
	uint8_t clientHandleLen = msgBuffer[index++];
	memcpy(clientHandle, msgBuffer + index, clientHandleLen);	// copy handle
	clientHandle[clientHandleLen] = '\0';						// add null
	index += clientHandleLen;
	uint8_t numOfHandles = msgBuffer[index++];					// num of dest

	// added for loop for the multicast -> just moves the index over so we can get the text message
	for(int i = 0; i < numOfHandles; i++){
		uint8_t destHandleLen = msgBuffer[index++];
		index += destHandleLen;
	}

	int txtMsgLen = msgLen - index;
	memcpy(txtMsg, msgBuffer + index, txtMsgLen);				// copy text message
	txtMsg[txtMsgLen] = '\0';									// add null
	return 0;
}

// handles for invalid handle names
void processHandleDne(uint8_t *responseBuf){
	uint8_t dneHandleLen = responseBuf[1];								// get len of handle that is already in use 
	char dneHandle[MAX_HANDLE_LEN]; 
	memcpy(dneHandle, responseBuf + 2, dneHandleLen);					// handle name into buffer
	dneHandle[dneHandleLen] = '\0';										// add null
	printf("\nClient with handle %s does not exist.\n", dneHandle);		
}

// grabs the handle number for multicast
int multicastHandleNum(uint8_t *userInput){
	int numHandles = userInput[3] - '0';			// the - '0' is to handle char conversion since user input is a string

	if(numHandles < 2 || numHandles > 9){
		printf("Handles has to be between 2-9\n");
		exit(-1);
	}

	return numHandles;
}

// places handles from multicast into an array
void multicastHandles(const char *userInput, int numHandles, char destHandlesArray[][MAX_HANDLE_LEN]){
    const char *p = userInput;

	// move to where the dest handle begins
    p = strchr(p, ' ');
    p++; 
    p = strchr(p, ' ');
    p++; 
    
    for (int i = 0; i < numHandles; i++) {
        const char *space = strchr(p, ' ');					// find space 
        int destHandleLen;

        if(space == NULL){
            // If no space found, assume the handle runs to the end of the string.
            destHandleLen = strlen(p);
        }else{
            destHandleLen = space - p;
        }
        
        strncpy(destHandlesArray[i], p, destHandleLen);		// copy dest handles into the array
        destHandlesArray[i][destHandleLen] = '\0'; 			// add null 
        
        // Advance pointer p to the start of the next token.
        if(space == NULL){
            break;  // no more tokens
        }else{
            p = space + 1;
        }
    }
}

char* getMulticastTxtMsg(uint8_t *userInput, int numHandles){
	static char txtMsg[200];
	txtMsg[0] = '\0'; 
	const char *p = (char *)userInput;

	// move to where the first dest handle begins
    p = strchr(p, ' ');
    p++; 
    p = strchr(p, ' ');
    p++; 

	// move past all the dest handles to where the text message begins
	for(int i = 0; i < numHandles; i++){
		p = strchr(p, ' ');
    	p++; 
	}

	if(p == NULL || *(p + 1) == '\0'){
        return txtMsg; 					// return empty string if no message
    }

	strncpy(txtMsg, p, 199);			// copy text message into buffer
	txtMsg[199] = '\0'; 				// add null
	return txtMsg;
}

// send a multicast to server -> server takes it 
void multicastMsg(int socketNum, const char *clientHandle, int numOfDest, char destHandles[][MAX_HANDLE_LEN], const char *txtMsg){
	uint8_t multicastBuf[MAXBUF];
	int index = 0;
	uint8_t flag = 6;
	multicastBuf[index++] = flag;									// flag
	uint8_t clientHandleLen = strlen(clientHandle);
	multicastBuf[index++] = clientHandleLen;						// handle length
	memcpy(multicastBuf + index, clientHandle, clientHandleLen);	// copy handle 
	index += clientHandleLen;
	multicastBuf[index++] = (uint8_t)numOfDest;						// number of handles 

	// add all the dest handles length and name to the buffer
	for(int i = 0; i < numOfDest; i++){
		uint8_t destHandleLen = (uint8_t) strlen(destHandles[i]);
		multicastBuf[index++] = destHandleLen;
		memcpy(multicastBuf + index, destHandles[i], destHandleLen);
		index += destHandleLen;
	}

	int txtMsgLen = strlen(txtMsg) + 1;								// add 1 for null character
	memcpy(multicastBuf + index, txtMsg, txtMsgLen);				// copy text message
	index += txtMsgLen;
	sendPacket(socketNum, multicastBuf, index);
}

// prints out all the handles once we get flags 11-13
void printList(int socketNum, uint8_t *responseBuf, int msgLen){
	// initially should be flag 11
	// printf("Beginning flag: %d\n", responseBuf[0]);
	uint32_t numOfHandles;
	memcpy(&numOfHandles, responseBuf + 1, sizeof(numOfHandles));	
	numOfHandles = ntohl(numOfHandles);								// change back order 
    printf("\nNumber of clients: %d\n", numOfHandles);

	// get the flag 12 packets
	for(uint32_t i = 0; i < numOfHandles; i++){
		// printf("Enters for loop\n");
		msgLen = recvPDU(socketNum, responseBuf, MAXBUF);			// call again because [0] changes
		// printf("Here is the flag: %d\n", responseBuf[0]);

		if(responseBuf[0] == 12){
			// printf("Flag is 12\n");
			uint8_t handleLen = responseBuf[1];
			char handle[MAX_HANDLE_LEN];
			memcpy(handle, responseBuf + 2, handleLen);				// copy handle 
			handle[handleLen] = '\0';								// add null
			printf("    %s\n", handle);								// print out handle
		}
	}

	if(responseBuf[0] == 13){
		// printf("Flag is 13\n");
		return;
	}
}

// returns the broadcast text message from user input
char* getBroadcastText(uint8_t *userInput){
	static char broadcastTxtMsg[200];
	broadcastTxtMsg[0] = '\0'; 
	char *message = strchr((char *)userInput, ' ');		// text message is right after space
	message++;

	if(message == NULL || *(message + 1) == '\0'){
        return broadcastTxtMsg;							// return empty string if no message
    }

	strncpy(broadcastTxtMsg, message, 199);				// copy text message into buffer
	broadcastTxtMsg[199] = '\0'; 						// add null
	return broadcastTxtMsg;
}

// make and send broadcast message over 
void broadcast(int socketNum, const char *clientHandle, const char *txtMsg){
	uint8_t broadcastBuf[MAXBUF];
	int index = 0;
	uint8_t flag = 4;
	uint8_t clientHandleLen = strlen(clientHandle);

	// making the broadcast packet
	broadcastBuf[index++] = flag;
	broadcastBuf[index++] = clientHandleLen;						// add handle length
	memcpy(broadcastBuf + index, clientHandle, clientHandleLen);	// add client handle
	index += clientHandleLen;
	int txtMsgLen = strlen(txtMsg) + 1;								// add 1 for null character
	memcpy(broadcastBuf + index, txtMsg, txtMsgLen);
	index += txtMsgLen;
	sendPacket(socketNum, broadcastBuf, index);
}

// handle server -> client broadcast
int processBroadcast(uint8_t *msgBuffer, int msgLen, char *clientHandle, char *txtMsg){
	int index = 0;		
	// uint8_t flag = msgBuffer[index++];
	index++;													// don't need to use flag
	uint8_t clientHandleLen = msgBuffer[index++];
	memcpy(clientHandle, msgBuffer + index, clientHandleLen);	// copy handle name
	clientHandle[clientHandleLen] = '\0';						// add null
	index += clientHandleLen;
	int txtMsgLen = msgLen - index;
	memcpy(txtMsg, msgBuffer + index, txtMsgLen);				// copy text message
	txtMsg[txtMsgLen] = '\0';									// add null
	return 0;
}