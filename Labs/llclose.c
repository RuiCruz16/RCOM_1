#define ADDRESS 0x03
#define DISC 0x0B
#define FLAG 0x7E
#define CONTROL_UA 0x07

int llclose(int showStatistics) {
    unsigned char buf[5] = {0};
    buf[0] = FLAG;
    buf[1] = ADDRESS;
    buf[2] = DISC;
    buf[3] = ADDRESS ^ DISC;
    buf[4] = FLAG;

    int bytesWritten = writeBytesSerialPort(buf, 5);

    if(bytesWritten < 5) {
        perror("ERROR: WRITING DISC TO SERIAL PORT\n");
        return -1;

    }
    unsigned char ackDisc[5] = {0};

    int bytesRead = readByteSerialPort(ackDisc, 5);

    if(bytesWritten < 5) {
        perror("ERROR: READING DISC FROM SERIAL PORT\n");
        return -1;
    }

    buf[2] = CONTROL_UA;
    buf[3] = buf[1] ^ buf[2];
    
    bytesWritten = writeBytesSerialPort(buf, 5);

    if(bytesWritten < 5) {
        perror("ERROR: WRITING UA TO SERIAL PORT\n");
        return -1;

    }

    int clstat = closeSerialPort();
    return clstat;
}
