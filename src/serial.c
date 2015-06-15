
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef linux
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#endif

#include "up02c.h"
#include "serial.h"
#include "tools.h"

HANDLE _portHandle = INVALID_HANDLE_VALUE;
#ifdef linux
int wait_flag=TRUE;
struct termios oldtio, newtio;
#endif

#ifdef linux
void signal_handler_IO(int status)
{
	printInfo(LOG_COMM, stdout, "received SIGIO signal\n");	
	wait_flag = FALSE;
}
#endif

HANDLE serial_openPort(const char *portName, int baud,
		unsigned char parity, unsigned char dataBits, unsigned char stopBits) {
	HANDLE portHandle = INVALID_HANDLE_VALUE;			
#ifdef _WIN32
	DCB dcb = { 0 }; 
	//char *portName = "\\\\.\\COM13"; 

	portHandle = CreateFile(portName, 
		GENERIC_READ | GENERIC_WRITE, 
		0, NULL, OPEN_EXISTING, 0, NULL); 
	if(portHandle == INVALID_HANDLE_VALUE) 
		return INVALID_HANDLE_VALUE; 

	dcb.DCBlength = sizeof(dcb); 
	dcb.BaudRate = baud; 
	dcb.fBinary = 1; 
	dcb.Parity = parity; 
	dcb.StopBits = ONESTOPBIT; 
	dcb.ByteSize = dataBits; 

	if(!SetCommState(portHandle, &dcb)) 
		return INVALID_HANDLE_VALUE;
#endif
#ifdef linux
	struct sigaction saio;
	long BAUD;
	long DATABITS;
	long STOPBITS;
	long PARITYON;
	long PARITY;

	switch(baud) {
		case 38400:
		default:
			BAUD = B38400;
			break;
		case 19200:
			BAUD  = B19200;
			break;
		case 9600:
			BAUD  = B9600;
			break;
		case 4800:
			BAUD  = B4800;
			break;
		case 2400:
			BAUD  = B2400;
			break;
		case 1800:
			BAUD  = B1800;
			break;
		case 1200:
			BAUD  = B1200;
			break;
		case 600:
			BAUD  = B600;
			break;
		case 300:
			BAUD  = B300;
			break;
		case 200:
			BAUD  = B200;
			break;
		case 150:
			BAUD  = B150;
			break;
		case 134:
			BAUD  = B134;
			break;
		case 110:
			BAUD  = B110;
			break;
		case 75:
			BAUD  = B75;
			break;
		case 50:
			BAUD  = B50;
			break;
	}

	switch(dataBits) {
		case 8:
		default:
			DATABITS = CS8;
			break;
		case 7:
			DATABITS = CS7;
			break;
		case 6:
			DATABITS = CS6;
			break;
		case 5:
			DATABITS = CS5;
			break;
	}

	switch(stopBits) {
		case 1:
		default:
			STOPBITS = 0;
			break;
		case 2:
			STOPBITS = CSTOPB;
			break;
	}

	switch(parity)	{
		case 0:
		default:
			PARITYON = 0;
			PARITY = 0;
			break;
		case 1:
			PARITYON = PARENB;
			PARITY = PARODD;
			break;
		case 2:
			PARITYON = PARENB;
			PARITY = 0;
			break;
	}

	portHandle = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(portHandle < 0)
		return INVALID_HANDLE_VALUE;

	//install the serial handler before making the device asynchronous
	saio.sa_handler = signal_handler_IO;
	sigemptyset(&saio.sa_mask);   //saio.sa_mask = 0;
	saio.sa_flags = 0;
	saio.sa_restorer = NULL;
	sigaction(SIGIO, &saio, NULL);
	
	// allow the process to receive SIGIO
	fcntl(fd, F_SETOWN, getpid());
	
	// Make the file descriptor asynchronous (the manual page says only
	// O_APPEND and O_NONBLOCK, will work with F_SETFL...)
	fcntl(portHandle, F_SETFL, FASYNC);
	  
	// save current port settings
	tcgetattr(portHandle, &oldtio);

	// set new port settings for canonical input processing 
	newtio.c_cflag = BAUD | CRTSCTS | DATABITS | 
					STOPBITS | PARITYON | PARITY | CLOCAL | 
					CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;       //ICANON;
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 0;
	tcflush(portHandle, TCIFLUSH);
	tcsetattr(portHandle, TCSANOW, &newtio);
#endif
	_portHandle = portHandle;
	return portHandle;
}

void serial_closePort(HANDLE portHandle) {
#ifdef _WIN32
	CloseHandle(portHandle);
#endif
#ifdef linux
	tcsetattr(portHandle, TCSANOW, &oldtio);
	close(portHandle);
#endif
	_portHandle = INVALID_HANDLE_VALUE;
}

int _inbyte(unsigned short timeout) {
	return serial_inByte(_portHandle, timeout);
}

int serial_inByte(HANDLE portHandle, unsigned short timeout) {
	unsigned char ch = 0; 
#ifdef _WIN32
	DWORD read = 0; 	
	COMMTIMEOUTS timeouts;
	
    timeouts.ReadIntervalTimeout = timeout;
    timeouts.ReadTotalTimeoutMultiplier = 1;
    timeouts.ReadTotalTimeoutConstant = timeout;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    if(!SetCommTimeouts(portHandle, &timeouts))
        printError(stderr, "setting port time-outs");	
	
	ReadFile(portHandle, &ch, 1, &read, NULL); 
#endif
#ifdef linux
	long read = 0; 
	unsigned long tx = getMs() + timeout;

	while(wait_flag && getMs() < tx)
		delay(20);

	if(!wait_flag)	
		read = read(portHandle, &ch, 1);
	wait_flag = TRUE;
#endif
	printInfo(LOG_COMM, stdout,
		"< %3d %c 0x%02x\n", read, (ch < 31 ? '.' : ch), ch);
			
	if(read <= 0)
		return -1;
	return ch;
}

void _outbyte(int c) {
	serial_outByte(_portHandle, (unsigned char)c);
}

void serial_outByte(HANDLE portHandle, unsigned char c) {
	unsigned char ch = c;
#ifdef _WIN32
	DWORD written = 0;
	WriteFile(portHandle, &ch, 1, &written, NULL);
#endif
#ifdef linux
	write(portHandle, &ch, 1);
#endif
	printInfo(LOG_COMM, stdout,
		"> %c 0x%02x\n", (ch < 31 ? '.' : ch), ch);
}

int serial_setDTR(HANDLE portHandle, unsigned char DTR) {
#ifdef _WIN32
	return (!EscapeCommFunction(portHandle, (DTR ? SETDTR : CLRDTR)));
#endif
#ifdef linux
	int status;
	ioctl(portHandle, TIOCMGET, &status);
	if(DTR)
		status |= TIOCM_DTR;
	else
		status &= ~TIOCM_DTR;
	return (ioctl(portHandle, TIOCMSET, &status) < 0);	
#endif
}
