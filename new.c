#include "new.h"

int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData){

    uint8_t pdu[MAXBUF + PDU_HEADER_SIZE];      // buffer for payload + 2 bytes
    uint16_t pduLength;                         // length of pdu

    // total length is payload + 2 bytes
    int totalLength = lengthOfData + PDU_HEADER_SIZE;
    int bytesSent;

    pduLength = htons(totalLength);     // big endian network
    memcpy(pdu, &pduLength, PDU_HEADER_SIZE);   // copy the 2 byte into buffer 
    memcpy(pdu + PDU_HEADER_SIZE, dataBuffer, lengthOfData);  // copy the data after the 2 byte into buffer

    bytesSent = send(clientSocket, pdu, totalLength, 0);    // send pdu over to server

    if(bytesSent < 0){
        perror("Did not send\n");
        return -1;
    }

    return bytesSent - PDU_HEADER_SIZE;     // sub the 2 bytes
} 

int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize){
    uint16_t pduLength;     // 2 bytes + payload
    int bytesReceived;
    int payloadLength;

    // wait to get the 2 bytes
    bytesReceived = recv(socketNumber, &pduLength, PDU_HEADER_SIZE, MSG_WAITALL);

    if(bytesReceived < 0){
        perror("could not get the 2 bytes\n");
        return -1;
    }else if(bytesReceived == 0){
        // printf("2 bytes is 0\n");
        return 0;
    }

    pduLength = ntohs(pduLength);       // big endian network
    payloadLength = pduLength - PDU_HEADER_SIZE;

    // wait to get the actual payload
    bytesReceived = recv(socketNumber, dataBuffer, payloadLength, MSG_WAITALL);
    
    if(bytesReceived < 0){
        perror("could not get the payload\n");
        return -1;
    }else if(bytesReceived == 0){
        // printf("payload is 0\n");
        return 0;
    }

    return bytesReceived;
}

