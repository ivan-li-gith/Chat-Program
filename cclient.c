/******************************************************************************
* myClient.c
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

void checkArgs(int argc, char * argv[]);
void clientControl(int socketNum);
void processStdin(int socketNum);
int command(int socketNum, uint8_t *userInput);
char* getDestHandle(uint8_t *userInput);
char* getTxtMsg(uint8_t *userInput);
void sendMessage(int socketNum, char* senderHandle, char* destHandle, char* txtMsg);
void processMsgFromServer(int socketNum);

char clientHandle[MAX_HANDLE_LEN];

int main(int argc, char * argv[]){
	int socketNum = 0;         
	checkArgs(argc, argv);
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);		// create socket for client
	char *clientHandle = argv[1];									// gets the handle of the client that was just established (so when u type ./cclient c2 ...)
	initialPacket(socketNum, clientHandle);							// sends initial packet to server




	// strncpy(clientHandle, argv[1], MAX_HANDLE_LEN - 1);
	// clientHandle[MAX_HANDLE_LEN - 1] = '\0'; // Ensure null termination

	// printf("here is the sender handle: %s\n", clientHandle);



	// need to check the client handle to ensure it does not already exist
	// so send a msg over to the server
	// type cast the handle, we add 1 to the len cuz strlen only copies chars and not the null
	// sendPDU(socketNum, (uint8_t *)argv[1], strlen(argv[1]) + 1);

	// // check for in use message
	// uint8_t inUseBuffer[MAXBUF];
	// int inUseMsgLen =  recvPDU(socketNum, inUseBuffer, MAXBUF);

	// if(inUseMsgLen > 0){
	// 	inUseBuffer[inUseMsgLen] = '\0';

	// 	if(strncmp((char *)inUseBuffer, "Handle already in use", 21) == 0){
	// 		printf("%s\n", (char *)inUseBuffer);
	// 		close(socketNum);
	// 		exit(0);
	// 	}

	// }

	clientControl(socketNum);
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



void clientControl(int socketNum){
	// start the poll set
	setupPollSet();

	// add stdin and the socket to the poll set
	addToPollSet(STDIN_FILENO);
	addToPollSet(socketNum);

	while(1){
		// flush out stdout
		printf("$:");
		fflush(stdout);

		// call poll
		int socket = pollCall(POLL_WAIT_FOREVER);

		// if socket is user input -> send msg to server
		// if socket is not user input its a msg from the server
		if(socket == STDIN_FILENO){
			processStdin(socketNum);
		}else{
			processMsgFromServer(socketNum);
		}
	}

	// close all sockets
	printf("Server shutting down\n");
    close(socketNum);
}

// send message to server
void processStdin(int socketNum){
	// buffer to store input
    uint8_t userInput[MAXBUF];
    int inputLen;

    // read user input
    inputLen = read(STDIN_FILENO, userInput, MAXBUF);

    if (inputLen <= 1) { // If input is empty or just newline
        printf("Invalid Command\n");
        return;
    }

    // handle the new line -> replace new line with null to format string
    userInput[inputLen - 1] = '\0';

    // if (!command(socketNum, userInput)) { 
    //     return;  // Invalid command, do not send
    // }


    // send message to server
    if(sendPDU(socketNum, userInput, inputLen) < 0) {
        perror("Error sending message");
		exit(-1);
    }
}


// makes initial packet from client to server
void initialPacket(int socketNum, const char *clientHandle){
	// format: 3 byte header, 1 byte handle length, handle (no null)
	// sendPDU adds the 2 bytes in from the header already so no need to add the 2 bytes here
	int index = 0;
	u_int8_t flag = 1;
	int handleLen = strlen(clientHandle);					// len of the client handle name 
	uint8_t payloadBuf[MAXBUF];								// buffer to store the data

	payloadBuf[index++] = flag;								// 1 byte flag
	payloadBuf[index++] = (u_int8_t *) handleLen;			// 1 byte handle length -> type cast it so its 1 byte
	memcpy(payloadBuf + index, clientHandle, handleLen);	// copy over the handle
	index += handleLen;

	// return index;											// returns total payload length

	sendPacket(socketNum, payloadBuf, payloadLen);
}

// calls sendPDU and checks for errors
void sendPacket(int socketNum, uint8_t payloadBuf, int payloadLen){
	if(sendPDU(socketNum, payloadBuf, payloadLen) < 0){
		perror("Unable to send PDU");
		exit(-1);
	}
}

// calls recvPDU and checks for errors
void rcvPacket(int socketNum, uint8_t * dataBuffer, int bufferSize){
	uint8_t responseBuf[MAX_HANDLE_LEN];
	int responseLen = recvPDU(newClientSocket, handleBuffer, MAX_HANDLE_LEN);

	if (responseLen < 0) {
        perror("Unable to receive PDU");
        exit(-1);
    }
}

// handles the response from server checking if the handle is available or not
void initialPacketResponse(){
	uint8_t flag = responseBuf[0];		// 2 byte len not included in the response buffer
	if(flag == 2){
		printf("Good Handle");
	}else if(flag == 3){
		printf("Handle already exists");
		exit(-1);
	}
}



// int command(int socketNum, uint8_t *userInput){

// 	if (userInput[0] != '%') {  // Ensure it starts with '%'
//         printf("Invalid Command\n");
//         return 0;
//     }

// 	char commandLetter = userInput[1];

// 	switch(commandLetter){
// 		case 'M':
// 		case 'm':
// 			printf("Letter is M\n");
// 			printf("Letter is M\n");

//             // Extract the destination handle
//             char *destHandle = getDestHandle(userInput);
//             if (!destHandle) {
//                 printf("Error: Destination handle not found.\n");
//                 return 0;
//             }

//             // Extract the message
//             char *txtMsg = getTxtMsg(userInput);
//             if (!txtMsg) {
//                 printf("Error: Message extraction failed.\n");
//                 return 0;
//             }

//             // Use global variable for sender handle
//             // extern char clientHandle[100];

//             printf("Here is the dest handle: %s\n", destHandle);
//             printf("Here is the message: %s\n", txtMsg);

//             // Call sendMessage()
//             sendMessage(socketNum, clientHandle, destHandle, txtMsg);
//             return 1;
// 		case 'B':
// 		case 'b':
// 			printf("Letter is B\n");
// 			break;		
// 		case 'C':
// 		case 'c':
// 			printf("Letter is C\n");
// 			break;
// 		case 'L':
// 		case 'l':
// 			printf("Letter is L\n");
// 			break;
// 		default:
// 			printf("Invalid Command\n");
// 			return 0;			
// 	}
// 	return 1;
// }

// char* getDestHandle(uint8_t *userInput){
// 	static char clientNameBuffer[MAX_HANDLE_LEN];

// 	// find the first space
// 	char *firstSpace = strchr((char *) userInput, ' ');

// 	// find the second space
// 	char *secondSpace = strchr(firstSpace + 1, ' ');

// 	int clientNameLen = secondSpace - (firstSpace + 1);

// 	strncpy(clientNameBuffer, firstSpace + 1, clientNameLen);
// 	clientNameBuffer[clientNameLen] = '\0';

// 	return clientNameBuffer;
// }

// char* getTxtMsg(uint8_t *userInput){
// 	static char txtMsg[200];
// 	txtMsg[0] = '\0'; // Initialize empty string


// 	// move past %M(space) -> +3 -> move to first space which is after the client name and then add 1 to be at the message
// 	char *message = strchr((char *)userInput + 3, ' ');
// 	message++;	

// 	if (message == NULL || *(message + 1) == '\0') {
//         return txtMsg; // Return empty string if no message
//     }


// 	strncpy(txtMsg, message, 199);
// 	txtMsg[199] = '\0'; // Ensure null-termination


// 	return txtMsg;
// }



// void sendMessage(int socketNum, char* senderHandle, char* destHandle, char* txtMsg){
// 	// 3 byte header (2 byte packet len 1 byte flag) -> 1 byte of len of sending -> handle name of sending -> 1 byte with val of 1 -> 
// 	// 1 byte len of destination handle -> handle name of dest -> txt msg 
// 	// msgPacketLen is 3 + 1 + 1 + len txt msg

// 	uint8_t msgPacket[MAXBUF];
// 	uint8_t flag = 5;
// 	uint8_t senderHandleLen = strlen(senderHandle);
// 	uint8_t destHandleLen = strlen(destHandle); 
// 	uint8_t numOfDest = 1;
// 	u_int16_t msgPacketLen = htons(3 + senderHandleLen + numOfDest + destHandleLen + strlen(txtMsg) + 1);

// 	int index = 0;

// 	// add 2 byte message length 
// 	memcpy(&msgPacket[index], &msgPacketLen, 2);
// 	index += 2;

// 	// add flag
// 	msgPacket[index++] = flag;

// 	// add 1 byte of sender handle len
// 	msgPacket[index++] = senderHandleLen;

// 	// add sender handle
// 	memcpy(&msgPacket[index], senderHandle, senderHandleLen);
// 	index += senderHandleLen;

// 	// add num of dest
// 	msgPacket[index++] = numOfDest;

// 	// add dest handle len 
// 	msgPacket[index++] = destHandleLen;

// 	// add dest handle
// 	memcpy(&msgPacket[index], destHandle, destHandleLen);
// 	index += destHandleLen;

// 	// add text msg
// 	// strcpy((char *)&msgPacket[index], txtMsg);
// 	strncpy((char *)&msgPacket[index], txtMsg, strlen(txtMsg) + 1);


// 	if (sendPDU(socketNum, msgPacket, msgPacketLen) < 0) {
//         perror("Error sending message");
// 		exit(-1);
//     }
// }

// void sendMessage(int socketNum, char* senderHandle, char* destHandle, char* txtMsg){
//     uint8_t msgPacket[MAXBUF];
//     uint8_t flag = 5;
//     uint8_t senderHandleLen = strlen(senderHandle);
//     uint8_t destHandleLen = strlen(destHandle);
//     uint8_t numOfDest = 1;
//     uint16_t msgPacketLen = htons(3 + senderHandleLen + numOfDest + destHandleLen + strlen(txtMsg) + 1);

//     int index = 0;

//     memcpy(&msgPacket[index], &msgPacketLen, 2);
//     index += 2;
//     msgPacket[index++] = flag;

// 	printf("DEBUG: Flag set to %d (should be 5)\n", flag);


//     msgPacket[index++] = senderHandleLen;
//     memcpy(&msgPacket[index], senderHandle, senderHandleLen);
//     index += senderHandleLen;

//     msgPacket[index++] = numOfDest;

//     msgPacket[index++] = destHandleLen;
//     memcpy(&msgPacket[index], destHandle, destHandleLen);
//     index += destHandleLen;

//     strncpy((char *)&msgPacket[index], txtMsg, strlen(txtMsg) + 1);

// 	// Print out the buffer before sending
//     printf("DEBUG: Message Packet Before Sending:\n");
//     for (int i = 0; i < index + strlen(txtMsg) + 1; i++) {
//         printf("%02X ", msgPacket[i]);
//     }
//     printf("\n");

//     if (sendPDU(socketNum, msgPacket, ntohs(msgPacketLen)) < 0) {
//         perror("Error sending message");
//         exit(-1);
//     }
// }




// takes that echoed message from the server and prints it out
void processMsgFromServer(int socketNum){	
	// buffer for the message
    uint8_t recvBuffer[MAXBUF];

    int msgReceived = recvPDU(socketNum, recvBuffer, MAXBUF);

    if(msgReceived == 0){
		// control C
        exit(0);
    }else if(msgReceived < 0) {
        perror("Error receiving from server");
        exit(-1);
    }else{
		// print out the message
        printf("Received from server: %s\n", recvBuffer);
    }
}
