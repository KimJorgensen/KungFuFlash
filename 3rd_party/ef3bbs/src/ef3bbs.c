#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>

int line_buf_echo = 1;

void set_line_buf_echo(int on)
{
    // Use termios to set line buffering and echo mode
    struct termios term = {};
    tcgetattr(STDIN_FILENO, &term);
    if (on)
    {
        term.c_lflag |= ICANON|ECHO;
    }
    else
    {
        term.c_lflag &= ~(ICANON|ECHO);
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
    setbuf(stdin, NULL);
    line_buf_echo = on;
}

// based on Linux (POSIX) implementation of _kbhit().
// Morgan McGuire, morgan@cs.brown.edu
int kbhit()
{
    if (line_buf_echo)
    {
        set_line_buf_echo(0);
    }

    int bytesWaiting;
    ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}

int getch()
{
    int c = getchar();

    // flush any pending input
    tcflush(STDIN_FILENO, TCIFLUSH);

    set_line_buf_echo(1);
    return c;
}

int serial;

void close_serial(void)
{
    close(serial);
    serial = -1;
}

int open_serial(const char * device)
{
    while (1)
    {
        serial = open(device, O_RDWR);
        if (serial != -1 || errno != EINTR)
        {
            break;
        }
    }

    if (serial == -1)
    {
        fprintf(stderr, "can't open serial port %s\n", device);
        return 1;
    }

    if (isatty(serial) == 0)
    {
        fprintf(stderr, "not a serial port %s\n", device);
        close_serial();
        return 1;
    }

    struct termios tio = {};
    if (tcgetattr(serial, &tio) != -1)
    {
        tio.c_iflag &= ~(IGNBRK|BRKINT|ICRNL|INLCR|     // Turn off input processing
                         IGNCR|PARMRK|INPCK|ISTRIP|
                         IXON|IXOFF|IXANY);

        tio.c_oflag &= ~OPOST;                          // Turn off output processing

        tio.c_lflag &= ~(ECHO|ECHONL|ICANON|            // Raw mode
                         IEXTEN|ISIG);

        tio.c_cflag &= ~(CSIZE|PARENB|CRTSCTS|HUPCL);   // 8n1, no hardware flow control
        tio.c_cflag |= CS8|CSTOPB|CREAD|CLOCAL;

        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 0;                            // No timeout

        if(cfsetospeed(&tio, B115200) != -1 &&          // 115200 baud
           cfsetispeed(&tio, B115200) != -1)
        {
            if(tcsetattr(serial, TCSANOW, &tio) != -1)
            {
                usleep(10000);  // Work-around for USB serial port drivers
                tcflush(serial, TCIOFLUSH);
                return 0;
            }
        }
    }

    fprintf(stderr, "can't change settings on serial port %s\n", device);
    close_serial();
    return 1;
}

int serial_read(unsigned char * buf, size_t len)
{
    int total_bytes_read = 0;
    unsigned char *buffer_location = buf;

    while (total_bytes_read < len)
    {
        int bytes_read = read(serial, buffer_location, len - total_bytes_read);
        if (bytes_read > 0)
        {
            total_bytes_read += bytes_read;
            buffer_location += bytes_read;
        }
        else if (bytes_read != -1 || errno != EINTR)
        {
            break;
        }
    }

    return total_bytes_read;
}

int serial_write(unsigned char * buf, size_t len)
{
    int total_bytes_written = 0;
    const unsigned char *buffer_location = buf;

    while (total_bytes_written < len)
    {
        int bytes_written = write(serial, buffer_location, len - total_bytes_written);
        if (bytes_written > 0)
        {
            total_bytes_written += bytes_written;
            buffer_location += bytes_written;
        }
        else if (bytes_written != -1 || errno != EINTR)
        {
            break;
        }
    }

    return total_bytes_written;
}
#else
#include <conio.h>
#include <Windows.h>

HANDLE serial;

void close_serial(void)
{
    CloseHandle(serial);
    serial = INVALID_HANDLE_VALUE;
}

int open_serial(const char * port)
{
    char filename[10];
    snprintf(filename, sizeof(filename), "\\\\.\\%s", port);

    if ((serial = CreateFile(filename, GENERIC_READ|GENERIC_WRITE,
        0,              // exclusive access
        NULL,           // no security attrs
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL)) == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "can't open serial port %s\n", port);
        return 1;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(serial, &dcb))
    {
        fprintf(stderr, "can't get settings for serial port %s\n", port);
        close_serial();
        return 1;
    }

    dcb.BaudRate            = CBR_115200;
    dcb.StopBits            = ONESTOPBIT;
    dcb.ByteSize            = 8;
    dcb.Parity              = NOPARITY;
    dcb.fParity             = 0;
    dcb.fOutxCtsFlow        = 0;
    dcb.fOutxDsrFlow        = 0;
    dcb.fDsrSensitivity     = 0;
    dcb.fTXContinueOnXoff   = TRUE;
    dcb.fOutX               = 0;
    dcb.fInX                = 0;
    dcb.fNull               = 0;
    dcb.fErrorChar          = 0;
    dcb.fAbortOnError       = 0;
    dcb.fRtsControl         = RTS_CONTROL_DISABLE;
    dcb.fDtrControl         = DTR_CONTROL_DISABLE;
    if (!SetCommState(serial, &dcb))
    {
        fprintf(stderr, "can't change settings on serial port %s\n", port);
        close_serial();
        return 1;
    }

    COMMTIMEOUTS commTimeOuts = {0};
    SetCommTimeouts(serial, &commTimeOuts);

    FlushFileBuffers(serial);
    PurgeComm(serial, PURGE_TXABORT|PURGE_RXABORT|PURGE_TXCLEAR|PURGE_RXCLEAR);

    COMSTAT comStat;
    DWORD dwErrorFlags;
    ClearCommError(serial, &dwErrorFlags, &comStat);
    return 0;
}

int serial_read(unsigned char * buf, size_t len)
{
    int total_bytes_read = 0;
    unsigned char *buffer_location = buf;

    while (total_bytes_read < len)
    {
        DWORD bytes_read;
        if (ReadFile(serial, buffer_location, len - total_bytes_read, &bytes_read, NULL))
        {
            total_bytes_read += bytes_read;
            buffer_location += bytes_read;
        }
        else
        {
            break;
        }
    }

    return total_bytes_read;
}

int serial_write(unsigned char * buf, size_t len)
{
    int total_bytes_written = 0;
    const unsigned char *buffer_location = buf;

    while (total_bytes_written < len)
    {
        DWORD bytes_written;
        if (WriteFile(serial, buffer_location, len - total_bytes_written, &bytes_written, NULL))
        {
            total_bytes_written += bytes_written;
            buffer_location += bytes_written;
        }
        else
        {
            break;
        }
    }

    return total_bytes_written;
}
#endif

#include "net.h"
#include "xfer.h"

  unsigned char * bufread;
  unsigned char * bufcmd;
  int written;

void print_net_status(int code, char *message)
{
}
unsigned char * bufstart;
FILE * fp;

void startprg()
{
    // SEND PRG TO EF3 MENU

    size_t  size;
    size_t total = 0;
    uint8_t       buffer[65536];

    int waiting;

    bufstart = (unsigned char *) malloc(30);
    bufread = (unsigned char *) malloc(30);

    printf("\nStarting ef3bbs.prg on c64\n");
    do
    {
        waiting = 0;
        sprintf(bufstart,"EFSTART:PRG%c",0);
//			printf("Handshake: EFSTART:PRG\n");

        written = serial_write(bufstart, 12);

        written = serial_read(bufread, 5);

        if (written != 5)
        {
            printf("Error 3 on the C64 side ... exiting...\n");
            goto exitpoint;
        }

//			printf("Response: [%c%c%c%c]\n",bufread[0],bufread[1],bufread[2],bufread[3]);

        if (bufread[0] == 'W')
        {
            waiting = 1;
//				printf("Waiting...\n");
        }
        else if (bufread[0] != 'L')
        {
            printf("Error on the C64 side... exiting...\n");
            goto exitpoint;
        }
    }
    while (waiting);

    printf("\n - Sending PRG file. Bytes sent: ");

    fp = fopen("ef3bbs.prg", "rb");

    serial_read(bufread, 2);		// get how many bytes to send

    written = bufread[0] + bufread[1]*256;

    do
    {
        size = fread(buffer, 1, written, fp);

        total += size;

        bufstart[0] = (unsigned char) (size & 0xff);
        bufstart[1] = (unsigned char) (size >> 8);

        if (serial_write(bufstart, 2) != 2)
        {
            printf("\nERROR 1!\n");
            break;
        }
        if (serial_write(buffer, size) != size)
        {
            printf("\nERROR 2!\n");
            break;
        }
        printf("%6ld%c%c%c%c%c%c",total,8,8,8,8,8,8);

    }
    while (size);

    printf("\n\nDONE!\n\n");
    exitpoint:

    fclose(fp);
}

int main(int argc, char *argv[])
{
    printf("EasyFlash3 BBS 1.0 by Tom-Cat of Nostalgia - Kung Fu Flash version\n\n");

    if (argc != 2)
    {
        printf("Usage: %s port\n", argv[0]);
#ifndef _WIN32
        printf("Example: %s /dev/ttyACM0\n", argv[0]);
#else
        printf("Example: %s COM4\n", argv[0]);
#endif
        return 1;
    }

    if (open_serial(argv[1])) return 1;

    startprg();

    bufread = (unsigned char *) malloc(30);
    bufcmd = (unsigned char *) malloc(1024);
    unsigned char * buff = (char *) malloc(4096);

    char *cfg_host;
    unsigned int cfg_port;

    unsigned char onechar = 0xaa;

    int i;
    char seq = 0x01;

    // do handshake , send cc  get back 1, 2, 3
    onechar = 0xcc;
    written = serial_write(&onechar,1);
    seq = 0x01;
    while (seq < 0x04)
    {
        written = serial_read(&onechar,1);
        if (seq == onechar)
        {
            seq++;
        }
    }

    signed int count = 1;
    int ch;

    int disco = 1;
    int in_menu = 1;

    int debug = 1;

    while (1)
    {
        // check keyboard
        if (kbhit() != 0)
        {
            int ch = getch();
            if (ch=='p' || ch=='x' || ch=='1')
            {
                int status = 0;
                if (ch=='p')
                {
                    printf("\n\nInitializing Punter Download...\n");
                    status=xfer_recv(0);
                }
                if (ch=='x')
                {
                    printf("\n\nInitializing Xmodem Download...\n");
                    status=xfer_recv(1);
                }
                if (ch=='1')
                {
                    printf("\n\nInitializing Xmodem 1k Download...\n");
                    status=xfer_recv(2);
                }
                if (status == 1)
                {
                    char filename2[256];
                    printf("\nEnter Filename: ");
                    scanf("%s", filename2);
                    xfer_save_file(filename2);
                }
            }
            if (ch=='u')
            {
                char filename3[256];
                printf("\nEnter Filename: ");
                scanf("%s", filename3);

                printf("\n\nInitializing Xmodem Upload...\n");
                xfer_send(filename3);
            }
        }

        if (!disco && !in_menu)
        {
            count = net_get(&onechar);
        }
        else
        {
            count = 0;
        }

        if (count > 0)
        {
            // Send to ef3
            written = serial_write(&onechar,1);

            if (onechar == 0)
            {
                written = serial_write(&onechar,1);
            }
        }
        else
        {
            // send 0 to ef3
            onechar = 0;
            written = serial_write(&onechar,1);
            onechar = 0xff;
            written = serial_write(&onechar,1);

            // get keypress from ef3, only if we didn't get anything from telnet
            written = serial_read(bufread,1);
            ch = bufread[0];							// ch = pressed key
            if (ch == 0xff)		// command mode
            {
                written = serial_read(bufread,1);
                ch = bufread[0];							// ch = pressed key
                if (ch == 0xff)		//signal to connect to server !!!
                {
                    if (disco == 0)
                    {
                        printf("Restarting...\n\n");
                        net_disconnect();
                    }

                    // get the telnet address and port from c64
                    written = serial_read(bufcmd, 34);

                    for (i=0; i < 34; i++)
                    {
                        if (bufcmd[i]==0x20)
                        {
                            bufcmd[i]=0;	//terminate
                        }
                    }
                    //printf("add: %s|\n", bufcmd);
                    written = serial_read(bufread, 5);
                    bufread[5]=0;
                    //printf("port: %s|\n", bufcmd);
                    cfg_port = atoi(bufread);
                    cfg_host = bufcmd;



                    printf("\nConnecting to: %s %d\n", cfg_host, cfg_port);

                    if (net_connect(cfg_host, cfg_port, &print_net_status))
                    {
                        printf("CONNECT FAILED\n\n");

                        onechar = 0xcc;
                        written = serial_write(&onechar,1);

                        printf("Restarting...\n\n");

                        net_disconnect();

                        disco = 1;
                        in_menu = 1;
                    }
                    else
                    {
                        onechar = 0xee;
                        written = serial_write(&onechar,1);

                        printf("\nConnected... press 'X' to recieve a file with Xmodem\n");
                          printf("                   '1' to recieve a file with Xmodem 1k\n");
                          printf("                   'U' to send a file with Xmodem\n\n");

                          disco = 0;
                          in_menu = 0;
                    }
                }
                else if (ch == 0x00)	//menu waiting
                {
                    in_menu = 1;
                }
                else if (ch == 0x01)	// menu save
                {
                    printf("Saving Configuration....\n");

                    written = serial_read(buff, 2);
                    int size = buff[0]+(buff[1]*256);

    //					printf("Size: %d\n", size);

                    written = serial_read(buff, size+1);

    //					printf("Written: %d\n", written);

                    if (!(fp = fopen("ef3bbs.cfg", "wb")))
                    {
                        printf("Can't open ef3irc.cfg for writing !\n");
                    }
                    else
                    {
                        fwrite(buff, 1 , size+1, fp);
                        fclose(fp);
                    }
                }
                else if (ch == 0x02)	// menu load
                {
                    printf("Loading Configuration....\n");

                    written = serial_read(buff, 2);
                    int size = buff[0]+(buff[1]*256);

    //					printf("Size: %d\n", size);

                    if (!(fp = fopen("ef3bbs.cfg", "rb")))
                    {
    //						printf("Can't open ef3irc.cfg for reading !\n");
                        buff[0]=0xff;	// signal file not found
                        written = serial_write(buff, 1);
                    }
                    else
                    {
                        buff[0]=0x00;	// signal file found
                        written = serial_write(buff, 1);
                        fread(buff, 1 , size+1, fp);
                        fclose(fp);
                        written = serial_write(buff, size+1);
    //						printf("Written: %d\n", written);
                    }
                }
                else if (ch == 0x03)	//menu exit
                {
                    in_menu = 0;
                    if (debug)
                    {
                        printf("menu exit\n");
                    }
                }
            }
        }
        if (count == -1)
        {
            if (ch != 0)
            {
                net_send(ch);
//				printf("sent to telnet: %d\n", ch);
            }
        }
        if (count == -2 && disco == 0)
        {
            sprintf(bufread,"%cno carrier%c",13,13);
            written = serial_write(bufread,12);
            printf("\n\n+++NO CARRIER\n");
            net_disconnect();
            disco = 1;
        }
    }

    free(bufread);
  free(bufcmd);

  close_serial();
}
