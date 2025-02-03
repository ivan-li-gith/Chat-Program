#ifndef __PDU_H__
#define __PDU_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PDU_HEADER_SIZE 2  // 2-byte length field
#define MAXBUF 1024

// Function prototypes
int sendPDU(int socketNumber, uint8_t *dataBuffer, int lengthOfData);
int recvPDU(int socketNumber, uint8_t *dataBuffer, int bufferSize);

#endif
