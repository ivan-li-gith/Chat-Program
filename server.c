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
// void sendHandleToClient(int clientSocket);
void forwardMessage(int clientSocket, uint8_t *buffer, int length);
void sendHandleError(int clientSocket, const char *destHandle);




// to handle closing the server with multiple clients
volatile int activeClients = 0;

int main(int argc, char *argv[]){
	int mainServerSocket = 0;   //socket descriptor for the server socket
	// int clientSocket = 0;   //socket descriptor for the client socket
	int portNumber = 0;
	portNumber = checkArgs(argc, argv);
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);
	serverControl(mainServerSocket);
	close(mainServerSocket);
	return 0;
}

int checkArgs(int argc, char *argv[]){
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
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

void addNewSocket(int mainServerSocket){
	// new client is trying to connect -> accept 
	int newClientSocket = tcpAccept(mainServerSocket, DEBUG_FLAG);

	if(newClientSocket < 0){
		perror("Cant accept new client");
		return;
	}



	// receive the handle from the client -> get it from the socket triggered, put that data in buffer, size of the buffer
	uint8_t handleBuffer[MAX_HANDLE_LEN];
	int handleLen = recvPDU(newClientSocket, handleBuffer, MAX_HANDLE_LEN);

	// // ensure that the received msg is > 0 else close the socket and remove from poll set
	if (handleLen < 0) {
        printf("Failed to receive handle from client\n");
        removeFromPollSet(newClientSocket);
        close(newClientSocket);
        return;
    }

	handleBuffer[handleLen] = '\0';	

	// add to poll set
	// add handle to linked list
	addHandle((char *) handleBuffer, newClientSocket);
	printf("Handle added to linked list: %s\n", (char *)handleBuffer);
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

	uint8_t flag = buffer[2];

	printf("Flag is: %d\n", flag);

    if (flag == 5) {  // %M (Private Message)
        printf("Received private message request from client %d\n", clientSocket);
        forwardMessage(clientSocket, buffer, bytesReceived);
    }

	printf("DEBUG: Received Packet (Hex Dump):\n");
	for (int i = 0; i < bytesReceived; i++) {
		printf("%02X ", buffer[i]);
	}
	printf("\n");



	printf("Message received on socket %d, length: %d Data: %s\n", clientSocket, bytesReceived, buffer);

	// sendHandleToClient(clientSocket);


	// char *senderHandle = getHandle(clientSocket); 
	// printf("Socket Number: %d\n", clientSocket);
	// printf("Here is the sender handle: %s\n", senderHandle);



	// if i remove this line i can no longer continuosly repeat the message
	// echo message back to the client
	// sendPDU(clientSocket, buffer, bytesReceived);

}

// void sendHandleToClient(int clientSocket){
// 	uint8_t senderHandleBuffer[MAX_HANDLE_LEN];
// 	char *senderHandle = getHandle(clientSocket);
// 	if (!senderHandle) {
//         printf("Error: Sender handle not found for socket %d\n", clientSocket);
//         return;
//     }

// 	snprintf((char *)senderHandleBuffer, sizeof(senderHandleBuffer), "%s", senderHandle);  // Copy handle safely
// 	printf("Socket Number: %d\n", clientSocket);
// 	printf("Here is the sender handle: %s\n", senderHandle);
// 	printf("here is the buffer: %s\n", senderHandleBuffer);
// 	sendPDU(clientSocket, senderHandleBuffer, strlen((char *)senderHandleBuffer + 1));
// }

// void sendHandleToClient(int clientSocket) {
//     uint8_t sendBuf[MAXBUF];
//     uint8_t flag = 12;  // Flag for private messages
//     char *senderHandle = getHandle(clientSocket);
    
//     if (!senderHandle) {
//         printf("Error: Sender handle not found for socket %d\n", clientSocket);
//         return;
//     }

//     uint8_t senderHandleLen = strlen(senderHandle);
//     uint16_t totalLen = htons(3 + senderHandleLen);  // 3-byte header + handle length

//     int index = 0;

//     // Add 2-byte message length
//     memcpy(&sendBuf[index], &totalLen, 2);
//     index += 2;

//     // Add 1-byte flag
//     sendBuf[index++] = flag;

//     // Add sender handle length
//     sendBuf[index++] = senderHandleLen;

//     // Add sender handle
//     memcpy(&sendBuf[index], senderHandle, senderHandleLen);
//     index += senderHandleLen;

//     printf("Socket Number: %d\n", clientSocket);
//     printf("Sending handle: %s (Len: %d)\n", senderHandle, senderHandleLen);
// 	printf("the buffer should be %s\n", sendBuf);

//     // Send the properly formatted message
//     sendPDU(clientSocket, sendBuf, ntohs(totalLen));
// }


// void fowardMessage(int clientSocket, uint8_t *buffer, int length){
// 	char *senderHandle = getHandle(clientSocket);
//     if (!senderHandle) {
//         printf("Error: Sender handle not found for socket %d\n", clientSocket);
//         return;
//     }

//     // Extract sender and recipient handle lengths
//     uint8_t senderHandleLen = buffer[3];
//     uint8_t destHandleLen = buffer[4 + senderHandleLen];

//     // Extract recipient handle
//     char destHandle[100];
//     strncpy(destHandle, (char*)&buffer[5 + senderHandleLen], destHandleLen);
//     destHandle[destHandleLen] = '\0';

//     // Find recipient’s socket
//     int destSocket = findHandle(destHandle);
//     if (destSocket == -1) {
//         sendHandleError(clientSocket, destHandle);
//         return;
//     }

//     // Forward message ONLY to recipient, NOT back to sender
//     sendPDU(destSocket, buffer, length);
//     printf("Forwarded message from %s to %s\n", senderHandle, destHandle);
// }

void forwardMessage(int clientSocket, uint8_t *buffer, int length){
    char *senderHandle = getHandle(clientSocket);
    if (!senderHandle) {
        return;
    }

    uint8_t destHandleLen = buffer[4 + strlen(senderHandle)];
    char destHandle[100];
    strncpy(destHandle, (char*)&buffer[5 + strlen(senderHandle)], destHandleLen);
    destHandle[destHandleLen] = '\0';

    int destSocket = findHandle(destHandle);
    if (destSocket == -1) {
        sendHandleError(clientSocket, destHandle);
        return;
    }

    sendPDU(destSocket, buffer, length);
	printf("Forwarded message from %s to %s\n", senderHandle, destHandle);

}

void sendHandleError(int clientSocket, const char *destHandle) {
    uint8_t errorPacket[MAXBUF];
    uint8_t flag = 7;  // Error flag
    uint8_t destHandleLen = strlen(destHandle);
    uint16_t totalLen = htons(3 + destHandleLen); // 3 bytes = (2-byte length + 1-byte flag)

    int index = 0;

    // Add 2-byte PDU length
    memcpy(&errorPacket[index], &totalLen, 2);
    index += 2;

    // Add 1-byte flag
    errorPacket[index++] = flag;

    // Add 1-byte destination handle length
    errorPacket[index++] = destHandleLen;

    // Add destination handle
    memcpy(&errorPacket[index], destHandle, destHandleLen);
    index += destHandleLen;

    // Send error message to the sender
    sendPDU(clientSocket, errorPacket, ntohs(totalLen));

    printf("Error: Client with handle %s does not exist. Sent error to client on socket %d.\n", destHandle, clientSocket);
}
