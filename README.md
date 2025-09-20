# Computer Networks - Project 1

## Overview
This project is part of the **Computer Networks** course, taken during the **first semester of the third year**. The main goal of this project is to develop a reliable communication protocol for file transfer over a serial connection. By implementing this protocol, students gain hands-on experience with the challenges and solutions involved in data transmission, including error detection, retransmission, and flow control.

## Objectives
- Design and implement a reliable data transfer protocol that operates over a serial port.
- Gain a deeper understanding of the application, link, and physical layers as they relate to real-world networking protocols.
- Develop a system capable of handling transmission errors and ensuring data integrity.
- Test and validate the protocol by transferring files between two systems.

## Project Structure
```
code/
├── main.c                # Entry point of the application
├── Makefile              # Build configuration
├── penguin.gif           # File to be transmitted
├── penguin-received.gif  # File received after transmission
├── README.txt            # Additional project details
├── bin/                  # Compiled binaries
│   ├── cable             # Simulated cable binary
│   └── main              # Main application binary
├── cable/                # Cable simulation source code
│   └── cable.c
├── include/              # Header files
│   ├── application_layer.h
│   ├── link_layer.h
│   └── serial_port.h
├── src/                  # Source files
    ├── application_layer.c
    ├── link_layer.c
    └── serial_port.c
```

## Files
- **penguin.gif**: The file to be transmitted.
- **penguin-received.gif**: The file received after transmission, used to verify the correctness of the protocol.

## Authors
- Gonçalo Marques
- Rui Cruz

## License
This project is for educational purposes and is not licensed for commercial use.
