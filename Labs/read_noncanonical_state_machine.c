// Read from serial port in non-canonical mode
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

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5

#define FLAG 0x7E
#define ADDRESS 0x03
#define CONTROL_SET 0x03
#define CONTROL_UA 0x07
#define BCC_SET (ADDRESS ^ CONTROL_SET)

volatile int STOP = FALSE;

// Enum representing the different states of the state machine
typedef enum
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP_STATE
} State;

State currentState = START;

// Function to handle state transitions based on received byte
void processStateMachine(unsigned char byte)
{
    switch (currentState)
    {
    case START:
        if (byte == FLAG)
        {
            currentState = FLAG_RCV;
        }
        break;

    case FLAG_RCV:
        if (byte == ADDRESS)
        {
            currentState = A_RCV;
        }
        else if (byte != FLAG)
        {
            currentState = START; // Reset to START if not the expected byte
        }
        break;

    case A_RCV:
        if (byte == CONTROL_SET)
        {
            currentState = C_RCV;
        }
        else if (byte == FLAG)
        {
            currentState = FLAG_RCV; // Handle repeated FLAG
        }
        else
        {
            currentState = START; // Reset on error
        }
        break;

    case C_RCV:
        if (byte == BCC_SET)
        {
            currentState = BCC_OK;
        }
        else if (byte == FLAG)
        {
            currentState = FLAG_RCV; // Handle repeated FLAG
        }
        else
        {
            currentState = START; // Reset on error
        }
        break;

    case BCC_OK:
        if (byte == FLAG)
        {
            currentState = STOP_STATE; // Successfully received frame
        }
        else
        {
            currentState = START; // Reset on error
        }
        break;

    default:
        currentState = START;
        break;
    }
}

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

    // Open serial port device for reading and writing and not as controlling tty
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

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

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Read 1 byte at a time

    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Loop to process input using the state machine
    while (STOP == FALSE)
    {
        unsigned char byte;
        int bytesRead = read(fd, &byte, 1); // Read one byte at a time

        if (bytesRead > 0)
        {
            processStateMachine(byte); // Process the byte through the state machine

            if (currentState == STOP_STATE)
            {
                printf("Successfully received the frame!\n");

                // Send UA (Unnumbered Acknowledgment) response
                unsigned char UA[BUF_SIZE] = {FLAG, ADDRESS, CONTROL_UA, (ADDRESS ^ CONTROL_UA), FLAG};
                int bytesSent = write(fd, UA, BUF_SIZE);

                sleep(1);
                
                if (bytesSent < BUF_SIZE)
                {
                    perror("Error writing UA response to serial port");
                }

                STOP = TRUE; // End the loop when the frame is successfully received
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
