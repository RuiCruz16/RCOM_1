// Application layer protocol implementation

#include "application_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define PACKET_SIZE 256

float log2_manual(unsigned int x) {
    int result = 0;
    while (x >>= 1) {
        result++;
    }
    return (float)result;
}

int ceil_manual(float x) {
    int intPart = (int)x;
    if (x > (float)intPart) {
        return intPart + 1;
    }
    return intPart;
}

unsigned char* getControlPacket(unsigned char cField, unsigned int fileLength, unsigned char *filename, int *size) {
	int l1 = (int) ceil_manual(log2_manual(fileLength) / 8.0);
    int l2 = strlen((const char *)filename);
    
    *size = 5 + l1 + l2;

    unsigned char* packet = (unsigned char*) malloc(*size);
    if (!packet) {
        return NULL;
    }

    unsigned int i = 0;
 
    packet[i++] = cField;
    packet[i++] = 0;
    packet[i++] = l1;
    
    for(int j = 0; j < l1; j++) {
        packet[2 + l1 - j] = fileLength & 0xFF;
        fileLength >>= 8;
    }

    i += l1;
    packet[i++] = 1;
    packet[i++] = l2;
    memcpy(packet + i, filename, l2);

    return packet;
}

unsigned char* getDataPacket(unsigned int sequence, unsigned int dataLength, unsigned char *data, int *size) {
	*size = 4 + dataLength;
    unsigned char* packet = (unsigned char*) malloc(4 + dataLength);
    if (!packet) {
        return NULL;
    }

    unsigned int i = 0;
    packet[i++] = 2;
    packet[i++] = sequence;
    packet[i++] = dataLength >> 8 & 0xFF;
    packet[i++] = dataLength & 0xFF;
    memcpy(packet + i, data, dataLength);
    
    return packet;
}

void parseControlPacket(unsigned char* packet, unsigned int* fileLength, unsigned char** filename) {
    int fileLengthSize = packet[2];
    *fileLength = 0;
    for (int i = 0; i < fileLengthSize; i++) {
        *fileLength += packet[3 + i] << (8 * (fileLengthSize - i - 1));
    }

    *filename = (unsigned char*) malloc(packet[4 + fileLengthSize] + 1);
    if (!*filename) {
        return;
    }
    memcpy(*filename, packet + 5 + fileLengthSize, packet[4 + fileLengthSize]);
    (*filename)[packet[4 + fileLengthSize]] = '\0';
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename) {

    LinkLayer connectionParam;
    connectionParam.role = strcmp(role, "tx") == 0 ? LlTx : LlRx;
    connectionParam.baudRate = baudRate;
    connectionParam.nRetransmissions = nTries;
    connectionParam.timeout = timeout;
    strcpy(connectionParam.serialPort, serialPort);

    int fd = llopen(connectionParam);

    if (fd < 0) {
        return;
    }

    struct timespec start, end;

    switch (connectionParam.role) {
        case LlTx: {
            FILE *fp = fopen(filename, "rb");

            if (fp == NULL) {
                llclose(fd);
                return;
            }


            struct stat fileStatus;
            if (stat(filename, &fileStatus) < 0) {
                fclose(fp);
                llclose(fd);
                return;
            }


            int size;
            unsigned char *startPacket = getControlPacket(1, fileStatus.st_size, (unsigned char *)filename, &size);

            clock_gettime(CLOCK_MONOTONIC, &start);

            if (llwrite(startPacket, size) < 0) {
                free(startPacket);
                fclose(fp);
                llclose(fd);
                return;
            }
            free(startPacket);


            int bytesLeft = fileStatus.st_size;
            int packetNum = 0;
            unsigned char* fileContent = (unsigned char*) malloc(fileStatus.st_size);
            if (fileContent == NULL) {
                fclose(fp);
                llclose(fd);
                return;
            }
            
            fread(fileContent, 1, fileStatus.st_size, fp);
            int offset = 0;
            while (bytesLeft > 0) {
                int bytesToSend = bytesLeft > PACKET_SIZE ? PACKET_SIZE : bytesLeft;
                unsigned char* buffer = (unsigned char*) malloc(bytesToSend);
                memcpy(buffer, fileContent + offset, bytesToSend); 

                unsigned char *dataPacket = getDataPacket(packetNum, bytesToSend, buffer, &size);

                if (llwrite(dataPacket, size) <= 0) {
                    free(dataPacket);
                    free(buffer);
                    break;
                }

                free(buffer);
                free(dataPacket);

                offset += bytesToSend;
                bytesLeft -= bytesToSend;
                packetNum = (packetNum + 1) % 100;
            }
    
            unsigned char *endPacket = getControlPacket(3, fileStatus.st_size, (unsigned char *)filename, &size);
            if (llwrite(endPacket, size) <= 0) {
            }
            free(endPacket);

            clock_gettime(CLOCK_MONOTONIC, &end);

            double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
            printf("Tempo total de transferência: %.3f segundos\n", elapsed);

            fclose(fp);
            llclose(fd);
            break;
        }

        case LlRx: {
            unsigned char packet[PACKET_SIZE];
            unsigned int rxFileSize = 0;
            int packetSize;
            int sequenceNumber = 0;
            unsigned char *filenameTX;

            do {
                packetSize = llread(packet);
            } while (packetSize < 0);

            parseControlPacket(packet, &rxFileSize, &filenameTX);

            FILE *newFile = fopen((char *) filename, "wb+");

            if (newFile == NULL) {
                free(filenameTX);
                llclose(fd);
                return;
            }

            clock_gettime(CLOCK_MONOTONIC, &start);

            while (1) {
                do {
                    packetSize = llread(packet);
                } while (packetSize < 0);

                if (packetSize == 0 || packet[0] == 3) {
                    break;
                }

                if (packet[1] != sequenceNumber) {
                    continue;
                }
                sequenceNumber = (sequenceNumber + 1) % 100;

                fwrite(packet + 4, 1, packetSize - 4, newFile);
            }
            
            clock_gettime(CLOCK_MONOTONIC, &end);

            double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
            printf("Tempo total de receção: %.3f segundos\n", elapsed);

            fclose(newFile);
            free(filenameTX);
            llclose(fd);
            break;
        }

        default:
            llclose(fd);
            break;
    }
}
