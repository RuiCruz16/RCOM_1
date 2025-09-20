#include "link_layer.h"
#include "serial_port.h"
 
#include <termios.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
 
// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BAUDRATE 38400  
 
#define FLAG 0x7E
#define ADDRESS_TM 0x03
#define ADDRESS_RC 0x01
#define DISC 0x0B
#define CONTROL_UA 0x07
#define CONTROL_SET 0x03
#define CONTROL_SET_0 0x00
#define CONTROL_SET_1 0x80
#define REJ_0 0x54
#define REJ_1 0x55
#define RR_0 0xAA
#define RR_1 0xAB
#define ESCAPE 0x7D
#define FLAG_REPLACEMENT 0x5E
#define ESCAPE_REPLACEMENT 0x5D
#define ESC 0x7D
#define FALSE 0
#define TRUE 1

typedef enum
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    DATA_FOUND_ESC,
    READING_DATA,
    STOP_STATE
} State;
 
 
int alarmEnabled = FALSE;
int alarmCount = 0;
int totalNumRetransmissions = 0;
int totalNumFrames = 0;
int totalRejectedFrames = 0;
 
int nRetransmissions = 0;
int timeout = 0;
 
int sequenceNumber = 0;
 
unsigned char controlField;
unsigned char addressField;
 
LinkLayerRole role;
 
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    totalNumRetransmissions++;
    printf("Alarm #%d\n", alarmCount);
}
 
void setupAlarm(int timeout)
{
    if (alarmEnabled == FALSE)
    {
        (void)signal(SIGALRM, alarmHandler);
        alarm(timeout); 
        alarmEnabled = TRUE;
    }
}
 
void resetAlarm()
{
    alarm(0);         
    alarmEnabled = FALSE;
    alarmCount = 0;
}
 
int writeSupervisionFrame(unsigned char control, unsigned char address) {
    unsigned char buf[5] = {FLAG, address, control, address ^ control, FLAG};
 
    int bytes = writeBytesSerialPort(buf, 5);
    return (bytes == 5) ? 0 : -1;
}
 
void processReceivedByte(State* state, unsigned char byte, LinkLayerRole role) {
    switch (role) {
        case LlTx:
            switch (*state) {
                    case START:
                        if (byte == FLAG) {
                            *state = FLAG_RCV;
                        }
                        break;
 
                    case FLAG_RCV:
                        if (byte == ADDRESS_RC || byte == ADDRESS_TM) {
                            *state = A_RCV;
                            addressField = byte;
                        }
                        else if (byte != FLAG) {
                            *state = START; 
                        }
                        break;
 
                    case A_RCV:
                        if (byte == CONTROL_UA || byte == DISC || byte == RR_0 || byte == RR_1 || byte == REJ_0 || byte == REJ_1) {
                            *state = C_RCV;
                            controlField = byte;
                        }
                        else if (byte == FLAG) {
                            *state = FLAG_RCV; 
                        }
                        else {
                            *state = START; 
                        }
                        break;
 
                    case C_RCV:
                        if (byte == (controlField ^ addressField)) {
                            *state = BCC_OK;
                        }
                        else if (byte == FLAG) {
                            *state = FLAG_RCV; 
                        }
                        else {
                            *state = START; 
                        }
                        break;
 
                    case BCC_OK:
                        if (byte == FLAG) {
                            *state = STOP_STATE; 
                        }
                        else {
                            *state = START; 
                        }
                        break;
 
                    default:
                        *state = START;
                        break;
                }
            break;
            case LlRx: {
                        switch (*state) {
                            case START:  
                                if (byte == FLAG) {
                                    *state = FLAG_RCV;
                                }
                                break;
 
                            case FLAG_RCV:  
                                if (byte == ADDRESS_TM || byte == ADDRESS_RC) {
                                    *state = A_RCV;
                                } else if (byte != FLAG) {  
                                    *state = START;
                                }
                                break;
 
                            case A_RCV: 
                                if (byte == CONTROL_SET || byte == DISC || byte == CONTROL_UA) {
                                    *state = C_RCV;
                                } else if (byte == FLAG) {  
                                    *state = FLAG_RCV;
                                } else {
                                    *state = START;  
                                }
                                break;
 
                            case C_RCV:  
                                if (byte == (ADDRESS_TM ^ CONTROL_SET) || byte == (ADDRESS_TM ^ DISC) || byte == (ADDRESS_RC ^ CONTROL_UA)) {
                                    *state = BCC_OK;
                                } else if (byte == FLAG) {  
                                    *state = FLAG_RCV;
                                } else {
                                    *state = START;  
                                }
                                break;
 
                            case BCC_OK:  
                                if (byte == FLAG) {
                                    *state = STOP_STATE;  
                                } else {
                                    *state = START;  
                                }
                                break;
 
                            default:
                                *state = START;  
                                break;
 
                    }
 
                break;
            }
    }
}
 
unsigned char readControl() {
    unsigned char controlField = 0, addressField = 0;
    State state = START;
    while(state != STOP_STATE && alarmEnabled) {
        unsigned char byte;
        if (readByteSerialPort(&byte) > 0) {
            switch (state)
            {
            case START:
                if (byte == FLAG) {
                    state = FLAG_RCV;
                }
                break;
            case FLAG_RCV:
                if (byte == ADDRESS_RC || byte == ADDRESS_TM) {
                    state = A_RCV;
                    addressField = byte;
                } else if (byte != FLAG) {
                    state = START;
                }
                break;
            case A_RCV:
                if (byte == RR_0 || byte == RR_1 || byte == REJ_0 || byte == REJ_1 || byte == DISC) {
                    state = C_RCV;
                    controlField = byte;
                } else if (byte == FLAG) {
                    state = FLAG_RCV;
                } else {
                    state = START;
                }
                break;
            case C_RCV:
                if (byte == (controlField ^ addressField)) {
                    state = BCC_OK;
                } else if (byte == FLAG) {
                    state = FLAG_RCV;
                } else {
                    state = START;
                }
                break;
            case BCC_OK:
                if (byte == FLAG) {
                    state = STOP_STATE;
                } else {
                    state = START;
                }
                break;
            default:
                break;
            }
        }
    }
    return controlField;
}
 
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
 int llopen(LinkLayer connectionParameters)
{
    int fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);
 
    if (fd < 0) return -1;
 
    nRetransmissions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
    State state = START;
    role = connectionParameters.role;  
 
    switch (connectionParameters.role) {
        case LlTx:
            while (alarmCount <= nRetransmissions) {
                if (!alarmEnabled) {
                    writeSupervisionFrame(CONTROL_SET, ADDRESS_TM);
                    setupAlarm(timeout); 
                }
 
                while (state != STOP_STATE && alarmEnabled) {  
                    unsigned char byte;
                    if (readByteSerialPort(&byte) > 0) {
                        processReceivedByte(&state, byte, connectionParameters.role);  
                    }
                }
 
                if (state == STOP_STATE) {
                    break;
                } else {
                    alarmEnabled = FALSE;
                    state = START; 
                }
            }
 
            if (state != STOP_STATE) return -1;
            break;
 
        case LlRx:
            while (state != STOP_STATE) {  
                unsigned char byte;
                if (readByteSerialPort(&byte) > 0) {
                    processReceivedByte(&state, byte, connectionParameters.role);
                }
            }
            state = START;
            writeSupervisionFrame(CONTROL_UA, ADDRESS_RC);
            break;
 
        default:
            return -1;
    }
    return fd;
}
 
 
////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
    unsigned int frameSize = 6;
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG || buf[i] == ESC) {
            frameSize++;
        }
        frameSize++;
    }
 
    unsigned char* frame = (unsigned char*) malloc(frameSize);
 
    if (!frame) {
        return -1;
    }
    int index = 0;
    frame[index++] = FLAG;
    frame[index++] = ADDRESS_TM;
    frame[index++] = sequenceNumber == 0 ? RR_0 : RR_1;
    frame[index++] = frame[1] ^ frame[2];
 
    // Calculate BCC2 prior to stuffing
    unsigned char bcc2 = 0;
    for (int i = 0; i < bufSize; i++) {
        bcc2 ^= buf[i];
    }
 
    // Stuff bytes
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG || buf[i] == ESC) {
            frame[index++] = ESC;
            frame[index++] = buf[i] ^ 0x20;
        } else {
            frame[index++] = buf[i];
        }
    }
    frame[index++] = bcc2;
    frame[index++] = FLAG;
 
    resetAlarm();

 
    while (alarmCount <= nRetransmissions) {
        if (!alarmEnabled) {
            if(writeBytesSerialPort(frame, frameSize) > 0) {
 
                setupAlarm(timeout);
                unsigned char controlField = readControl();
 
                if(controlField == 0) {
                    continue;
                }

                else if (controlField == RR_1 || controlField == RR_0) {
                    sequenceNumber = controlField == RR_1 ? 1 : 0;
                    free(frame);
                    resetAlarm();
                    return bufSize;
                }   

                else if (controlField == REJ_0 || controlField == REJ_1) {
                    resetAlarm();
                    totalRejectedFrames++;
                } 
            }
        }
    }
 
    free(frame);

    return -1;
}
 
 
////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
    unsigned char byte, controlField;
    State state = START;
    int index = 0;
 
    while(state != STOP_STATE) {
        if (readByteSerialPort(&byte) > 0) {
            switch (state)
            {
            case START:
                if(byte == FLAG) state = FLAG_RCV;
                break;
            case FLAG_RCV:
                if(byte == ADDRESS_TM) state = A_RCV;
                else if(byte != FLAG) state = START;
                break;
            case A_RCV:
                if(byte == RR_0 || byte == RR_1) {
                    controlField = byte;
                    state = C_RCV;
                } else if(byte == DISC) {
                    writeSupervisionFrame(CONTROL_UA, ADDRESS_RC);
                    return 0;
                } else if(byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;
            case C_RCV:
                if(byte == (ADDRESS_TM ^ controlField)) state = BCC_OK;
                else if(byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;
            case BCC_OK:
                if(byte == ESC) state = DATA_FOUND_ESC;
                else if(byte == FLAG) {
                    state = STOP_STATE;
                    unsigned char bcc2 = packet[index-1];
                    unsigned char bcc2Calc = 0;
                    for(int i = 0; i < index - 1; i++) {
                        bcc2Calc ^= packet[i];
                    }

                    if(bcc2 == bcc2Calc) {
                        writeSupervisionFrame(controlField == RR_0 ? RR_1 : RR_0, ADDRESS_TM);
                        if (packet[0] == 2) {
                            totalNumFrames++;
                        }
                        return index - 1;
                    }
                    else {
                        writeSupervisionFrame(controlField == RR_0 ? REJ_0 : REJ_1, ADDRESS_TM);
                        printf("Received BCC2 differs from calculated BCC2. Sending REJ: 0x%x \n", controlField == RR_0 ? REJ_0 : REJ_1);
                        return -1;
                    }
                }
                else {
                    packet[index++] = byte;
                }
                break;
            case DATA_FOUND_ESC:
                state = BCC_OK;
                if(byte == FLAG_REPLACEMENT) packet[index++] = FLAG;
                else if(byte == ESCAPE_REPLACEMENT) packet[index++] = ESC;
                break;
            default:
                break;
            }
        }
    }
    return -1;
}
 
 
////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    State state = START;
 
    resetAlarm();
    switch (role) {
    case LlTx:
        while (alarmCount <= nRetransmissions) {
            if (!alarmEnabled) {
                if (writeSupervisionFrame(DISC, ADDRESS_TM) < 0) {
                    return -1;
                }
                setupAlarm(timeout); 
            }
 
            while (state != STOP_STATE && alarmEnabled) {
                unsigned char byte;
                if (readByteSerialPort(&byte) > 0) {
                    processReceivedByte(&state, byte, LlTx);
                }
            }
 
            if (state == STOP_STATE) {
                break;
            } else {
                alarmEnabled = FALSE;
                state = START;
            }
        }
 
        if (state != STOP_STATE) {
            return -1;
        }
 
        if (writeSupervisionFrame(CONTROL_UA, ADDRESS_RC) < 0) {
            return -1;
        }
        break;
 
    case LlRx:
        while (state != STOP_STATE) {
            unsigned char byte;
            if (readByteSerialPort(&byte) > 0) {
                processReceivedByte(&state, byte, LlRx);
            }
        }
 
        if (writeSupervisionFrame(DISC, ADDRESS_RC) < 0) {
            return -1;
        }
 
        state = START;
        while (state != STOP_STATE) {
            unsigned char byte;
            if (readByteSerialPort(&byte) > 0) {
                processReceivedByte(&state, byte, LlRx);
            }
        }
        break;
 
    default:
        return -1;
    }
 
    int clstat = closeSerialPort();
    if (showStatistics) {
        printf("Connection closed. Statistics: \n");
        if (role == LlTx) {
            printf("Frames rejeitados durante a transmissão: %d\n", totalRejectedFrames);
            printf("Number of retransmissions: %d.\n", totalNumRetransmissions);
        } else if (role == LlRx) {
            printf("Number of information frames received: %d.\n", totalNumFrames);
        }
        printf("Número total de frames enviados: %d.\n", totalNumFrames + totalRejectedFrames);
    }
 
    return clstat;
}
