#define FLAG 0x7E
#define ADDRESS 0x03
#define CONTROL_SET_0 0x00
#define CONTROL_SET_1 0x80
#define REJ_0 0x54
#define REJ_1 0x55
#define RR_0 0xAA
#define RR_1 0xAB
#define ESCAPE 0x7D
#define FLAG_REPLACEMENT 0x5E
#define ESCAPE_REPLACEMENT 0x5D

int llread(unsigned char* packet) {
    unsigned char stuffedData[1024];
    int bytesRead = readByteSerialPort(stuffedData, sizeof(stuffedData));

    if (bytesRead < 6) {
        printf("ERROR: LESS BYTES RECEIVED THAN EXPECTED\n");
        sendREJ(0); 
        return -1;
    }

    unsigned char destuffedData[1024];
    int destuffedDataLength = 0;
    destuffBytes(destuffedData, &destuffedDataLength, stuffedData, bytesRead);

    int sequenceNumber = (destuffedData[2] == CONTROL_SET_0) ? 0 : 1;

    if (destuffedData[0] != FLAG || destuffedData[destuffedDataLength - 1] != FLAG) {
        printf("ERROR: WRONG FLAG RECEIVED\n");
        sendREJ(sequenceNumber);
        return -1;
    }

    if (destuffedData[1] != ADDRESS) {
        printf("ERROR: WRONG ADDRESS RECEIVED\n");
        sendREJ(sequenceNumber);
        return -1;
    }

    unsigned char calculatedBCC2 = 0;
    for (int i = 4; i < destuffedDataLength - 2; i++) {
        calculatedBCC2 ^= destuffedData[i];
    }

    if (destuffedData[destuffedDataLength - 2] != calculatedBCC2) {
        printf("ERROR: WRONG DATA RECEIVED (BCC2 MISMATCH)\n");
        sendREJ(sequenceNumber);
        return -1;
    }

    sendRR(sequenceNumber); 
    memcpy(packet, &destuffedData[4], destuffedDataLength - 6); 

    return destuffedDataLength - 6;
}

void sendRR(int sequenceNumber) {
    unsigned char rrFrame[5];
    rrFrame[0] = FLAG;
    rrFrame[1] = ADDRESS;
    rrFrame[2] = (sequenceNumber == 0) ? RR_0 : RR_1;
    rrFrame[3] = rrFrame[1] ^ rrFrame[2]; // BCC1
    rrFrame[4] = FLAG;

    writeBytesSerialPort(rrFrame, sizeof(rrFrame));
    printf("Sent RR%d acknowledgment.\n", sequenceNumber);
}

void sendREJ(int sequenceNumber) {
    unsigned char rejFrame[5];
    rejFrame[0] = FLAG;
    rejFrame[1] = ADDRESS;
    rejFrame[2] = (sequenceNumber == 0) ? REJ_0 : REJ_1;
    rejFrame[3] = rejFrame[1] ^ rejFrame[2]; // BCC1
    rejFrame[4] = FLAG;

    writeBytesSerialPort(rejFrame, sizeof(rejFrame));
    printf("Sent REJ%d acknowledgment.\n", sequenceNumber);
}

void destuffBytes(unsigned char* destuffed, int* destuffedDataLength, const unsigned char *stuffed, int stuffedDataLength) {
    int currentIndex = 0;
    for (int i = 0; i < stuffedDataLength; i++) {
        unsigned char current = stuffed[i];
        if (current == ESCAPE) {
            if (i + 1 < stuffedDataLength) {  
                unsigned char next = stuffed[++i];
                if (next == FLAG_REPLACEMENT) {
                    destuffed[currentIndex++] = FLAG;
                } else if (next == ESCAPE_REPLACEMENT) {
                    destuffed[currentIndex++] = ESCAPE;
                } else {
                    destuffed[currentIndex++] = next;
                }
            }
        } else {
            destuffed[currentIndex++] = current;
        }
    }
    *destuffedDataLength = currentIndex; 
}
