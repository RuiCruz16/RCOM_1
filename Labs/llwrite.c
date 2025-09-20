#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

#define MAX_TRIES 3

extern int alarmCount;
extern int alarmEnabled;

int llwrite(const unsigned char *data, int dataLength) {
    int sequenceNumber = 0;
    unsigned char controlField;
    unsigned char *frame = NULL;

    while (alarmCount < MAX_TRIES) {
        controlField = (sequenceNumber == 0) ? CONTROL_SET_0 : CONTROL_SET_1;

        unsigned char *stuffedData = NULL;
        int stuffedDataLength = 0;

        stuffedData = (unsigned char *)malloc(2 * dataLength);
        if (stuffedData == NULL) {
            perror("Error allocating memory for stuffed data");
            return -1;
        }

        stuffBytes(stuffedData, &stuffedDataLength, data, dataLength);

        frame[0] = FLAG;
        frame[1] = ADDRESS;
        frame[2] = controlField;
        frame[3] = frame[1] ^ frame[2]; // BCC1

        unsigned char bcc2 = data[0];
        for (int i = 1; i < dataLength; i++) {
            bcc2 ^= data[i];
        }

        memcpy(&frame[4], stuffedData, stuffedDataLength);
        frame[4 + dataLength] = bcc2; // BCC2
        frame[5 + dataLength] = FLAG;

        if (!alarmEnabled) {
            int bytesWritten = writeBytesSerialPort(frame, 6 + dataLength);
            if (bytesWritten < 6 + dataLength) {
                perror("Error writing frame to serial port");
                free(frame); 
                return -1;
            } else {
                printf("%d bytes written\n", bytesWritten);
            }
        }

        setupAlarm(); 

        unsigned char response[6] = {0};
        int bytesRead = readByteSerialPort(response, sizeof(response));

        // FLAG | A | C | BCC | FLAG
        if (bytesRead > 0) {
            if ((response[2] == REJ_0 && sequenceNumber == 0) || (response[2] == REJ_1 && sequenceNumber == 1)) {
                printf("Frame rejected, retransmitting...\n");
            } else if ((response[2] == RR_0 && sequenceNumber == 0) || (response[2] == RR_1 && sequenceNumber == 1)) {
                printf("Frame acknowledged by receiver.\n");
                free(frame); 
                sequenceNumber = 1 - sequenceNumber; 
                return bytesRead; 
            } else {
                printf("Unexpected response received, retransmitting...\n");
            }
        } else {
            printf("Timeout or no response, retransmitting...\n");
        }

        free(frame); 
    }

    free(frame); 
    return -1; 
}


void stuffBytes(unsigned char* stuffed, int* stuffedDataLength, const unsigned char *data, int dataLength) {
    int currentIndex = 0;
    for (int i = 0; i < dataLength; i++) {
        unsigned char current = data[i];
        if (current == FLAG) {
            stuffed[currentIndex++] = ESCAPE;
            stuffed[currentIndex++] = FLAG_REPLACEMENT;
        } else if (current == ESCAPE) {
            stuffed[currentIndex++] = ESCAPE;
            stuffed[currentIndex++] = ESCAPE_REPLACEMENT;
        } else {
            stuffed[currentIndex++] = current;
        }
    }
    *stuffedDataLength = currentIndex; 
}
