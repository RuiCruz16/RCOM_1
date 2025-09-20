// write_noncanonical.c
//
// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]
 
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "alarmHandler.h"
 
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source
 
#define FALSE 0
#define TRUE 1
 
#define BUF_SIZE 5
 
#define FLAG 0x7E
#define ADDRESS 0x03
#define CONTROL_UA 0x07
#define CONTROL_SET 0x03
 
extern int alarmEnabled;
extern int alarmCount;
volatile int STOP = FALSE;
 
int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }
 
    const char *serialPortName = argv[1];
 
    // Open serial port device for reading and writing, and not as controlling tty
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }
 
    struct termios oldtio, newtio;
    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }
 
    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
 
    // Set non-canonical input mode (non-blocking read, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 1; 
    newtio.c_cc[VMIN] = 0;  
 
    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }
 
    printf("New termios structure set\n");
 
    // Create string to send
    unsigned char buf[BUF_SIZE] = {FLAG, ADDRESS, CONTROL_SET, ADDRESS ^ CONTROL_SET, FLAG};
 
    // Set up the alarm mechanism
	int bytes;
 
    STOP = FALSE;
    while (STOP == FALSE && alarmCount < 3)
    {
 
		if(!alarmEnabled) {
			bytes = write(fd, buf, BUF_SIZE); // Write only 5 bytes of the buffer

            sleep(1);

			printf("%d bytes written\n", bytes);
		}
 
        setupAlarm();
 
        // Read incoming data in non-blocking mode
        unsigned char UA[BUF_SIZE + 1] = {0};
        bytes = read(fd, UA, BUF_SIZE);
        if (bytes > 0)
        {
            //printf("UA[0]: %02x\n", UA[0]);
            //printf("UA[1]: %02x\n", UA[1]);
            //printf("UA[2]: %02x\n", UA[2]);
            //printf("UA[3]: %02x\n", UA[3]);
            //printf("UA[4]: %02x\n", UA[4]);
            UA[bytes] = '\0'; // Null-terminate the buffer
            if (UA[0] == FLAG && UA[1] == ADDRESS && UA[2] == CONTROL_UA && UA[3] == (ADDRESS ^ CONTROL_UA) && UA[4] == FLAG)
            {
                printf("Received correct response: DONE\n");
                resetAlarm();   // Reset the alarm
                STOP = TRUE;    // Stop the loop
            }
        }
    }
 
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }
 
    close(fd);
    return 0;
}