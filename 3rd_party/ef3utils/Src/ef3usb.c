/*
 * Modified version of Easy Flash III USB Transfer Utilities v1.93 by Tom-Cat
 * Uses serial port instead of ftdi
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

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
    if(tcgetattr(serial, &tio) != -1)
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

int d81 = 0;

int nrTracks=40;

static int sectorsPerTrack[]={ 21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
                                                           19,19,19,19,19,19,19,
                                                           18,18,18,18,18,18,
                                                           17,17,17,17,17,
                                                           17,17,17,17,17 };

static int sectorsPerTrack71[]={ 21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
                                                           19,19,19,19,19,19,19,
                                                           18,18,18,18,18,18,
                                                           17,17,17,17,17,
                                21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
                                                           19,19,19,19,19,19,19,
                                                           18,18,18,18,18,18,
                                                           17,17,17,17,17
                                                            };

char* filetypes[]={"DEL\0","SEQ\0","PRG\0","USR\0","REL\0","CBM\0"};

unsigned char c64filename[23];

int linearSector(int track, int sector)
{
int i;

        int* spt = sectorsPerTrack;
        if (nrTracks == 70)
        {
                spt = sectorsPerTrack71;
        }

        if ((track<1) || (track>nrTracks))
        {
                fprintf(stderr, " - Illegal track %d\n", track);
//		exit(-1);
                return -1;

        }

        if (!d81)
        {
                if ((sector<0) || (sector>=spt[track-1]))
                {
                        fprintf(stderr, " - Illegal sector %d for track %d (max is %d)\n", sector, track, sectorsPerTrack[track-1]);
//        		exit(-1);
                        return -1;
                }
        }
        else if (sector>39)
        {
                fprintf(stderr, " - Illegal sector %d (max is %d)\n", sector, 39);
//		exit(-1);
                return -1;
        }

        if (d81)
        {
                return (track-1)*40 + sector;
        }

        int linearSector=0;
        for (i=0;i<track-1;i++)
                linearSector+=spt[i];
        linearSector+=sector;

        return linearSector;
}

int off(int track, int sector)
{
        int tr = linearSector(track, sector);
        if (tr != -1)
        {
                return tr*256;
        }
        return -1;
}

int usednum;
unsigned char usedtr[1024];
unsigned char usedse[1024];
unsigned char * buf;
unsigned char * buf2;
unsigned char * filebuf;
unsigned char checksum;
unsigned char * bufread;
unsigned char * bufstart;
int written;
int fileptr;
unsigned char ft;
int entoff;

int sendfile()
{
        int i;

        printf(" - Len: %6d bytes. Sending file. Bytes left:", fileptr);

        bufstart[0] = ft;        // filetype
        written = serial_write(bufstart, 1);

        // filename
        written = serial_write(buf+entoff+0x05, 16);

        // filelen in 3 bytes
        bufstart[0] = (unsigned char) (fileptr&0xff);        // len lo
        bufstart[1] = (unsigned char) ((fileptr>>8)&0xff);   // len hi
        bufstart[2] = (unsigned char) ((fileptr>>16)&0xff);  // len hi hi
        written = serial_write(bufstart, 3);

        // calc checksum
        checksum = 0;
        for (i=0; i < fileptr; i++)
        {
              checksum = checksum + filebuf[i];
        }
        bufstart[0] = checksum;
        written = serial_write(bufstart, 1);

        // data
        for (i=0; i < fileptr; i++)
        {
                serial_write(filebuf+i,1);
                printf("%6d%c%c%c%c%c%c",fileptr-i,8,8,8,8,8,8);
        }
        printf("%c%c%c%c%c%c%c%c%c%c%cDone.                    \n",8,8,8,8,8,8,8,8,8,8,8);
        //written = serial_write(filebuf, fileptr);

        // get ack
        written = serial_read(bufread, 1);
        if (bufread[0] != 0xff)
        {
                printf("Error on the C64 side ... exiting...\n");
                return 1;
        }
        return 0;
}

void sendindicator(int format, int track, int sector)
{
        // send the position of the '*' on the c64 screen
        int add;
        if (format == 80)
        {
                // d81
                add = 0x0400 + (3+(sector/2))*40 + ((track-1)/2);
                unsigned char vis;
                if (track%2)
                {
                        // 1,2
                        if (sector%2)
                        {
                                vis = 0x32;
                        }
                        else
                        {
                                vis = 0x31;
                        }
                }
                else
                {
                        // 3,*
                        if (sector%2)
                        {
                                vis = 0x2a;
                        }
                        else
                        {
                                vis = 0x33;
                        }
                }
                bufstart[2] = vis;     // 1,2,3,*
        }
        else
        {
                // d64
                add = 0x0400 + (3+sector)*40 + (track-1);
                bufstart[2] = 0x2a;     // '*'
        }
        bufstart[0] = (unsigned char) (add&0xff);
        bufstart[1] = (unsigned char) ((add>>8)&0xff);
        written = serial_write(bufstart,3);        // send over the coordinates and character
}

void printusage(const char * executable)
{
   printf("Usage: %s port command [file] [options]\n", executable);
   printf(" e[xecute]  file.prg|p00                   - execute prg on c64\n");
   printf(" c[opy]     file.prg|p00|d64|d81|d71       - copy files to c64\n");
   printf(" x[fer]     [p00]                          - copy files from c64\n");
   printf(" w[rite]    file.d64|d81 [verify] [kernal] - write image on c64\n");
   printf(" r[ead]     file.d64|d81 [40] [kernal]     - read image from c64\n");
   printf(" d[ir]      file.d64|d81|d71               - display dir of file and check it\n");
   printf(" f[ormat]   [40]                           - turbo format 1541 floppy\n");
   printf(" t[apwrite] file.tap                       - write tap file to tape\n");
   printf(" m[aketap]  file.tap                       - read tap file from tape\n");
   printf("----------- the following are to be used in EF3 menu mode: \n");
   printf(" b[urn]     file.crt                       - flash crt file to the ef3\n");
   printf(" s[end]     [file.prg]                     - send file.prg to EF3 menu\n");
   printf("                                             if no file then send ef3usb.prg\n");
   printf(" 0[test]                                   - test the usb connection\n");
#ifndef _WIN32
   printf("Example: %s /dev/ttyACM0 s\n", executable);
#else
   printf("Example: %s COM4 s\n", executable);
#endif
   exit(1);
}

void startcommand(unsigned char cmd)
{
  // SYNC BYTES
  bufstart[0] = 0xB3;
  bufstart[1] = 0x68;
  bufstart[2] = 0x92;

  bufstart[3] = cmd;

  written = serial_write(bufstart, 4);         // send sync & command

  written = serial_read(bufstart, 1);          // get back ACK
}

int main(int argc, char *argv[])
{
  char *fname = NULL;
  FILE *fp;
  int read;

  int i;
  int len;
  int batch;
  int mode = 0;
  int mode40 = 0;
  int command = -1;

  char p00start[8] = "C64File\0";

  printf("EF3 USB Client v1.93 - Kung Fu Flash version\n");
  if (argc < 3)
  {
        printusage(argv[0]);
  }

  if (open_serial(argv[1])) return 1;
  int verify = 0;

  printf("\n");
  switch(argv[2][0])
  {
        case 'e': command = 0;
                  printf(" - EXECUTE FILE\n");
                  if (argc != 4)
                  {
                        printusage(argv[0]);
                  }
                  fname = argv[3];
                  break;
        case 'c': command = 1;
                  printf(" - COPY FILE(S) TO C64\n");
                  if (argc != 4)
                  {
                        printusage(argv[0]);
                  }
                  fname = argv[3];
                  break;
        case 'w': command = 2;
                  printf(" - WRITE IMAGE\n");
                  if (argc == 6)
                  {
                        mode = 1;
                        verify = 1;
                  }
                  else if (argc == 5)
                  {
                        if (argv[4][0] == 'k')
                        {
                                mode = 1;
                        }
                        else
                        {
                                verify = 1;
                        }
                  }
                  fname = argv[3];
                  break;
        case 'r': command = 3;
                  printf(" - READ IMAGE\n");
                  if (argc == 6)
                  {
                        mode = 1;
                        mode40 = 1;
                  }
                  else if (argc == 5)
                  {
                        if (argv[4][0] == 'k')
                        {
                                mode = 1;
                        }
                        else
                        {
                                mode40 = 1;
                        }
                  }
                  fname = argv[3];
                  break;
        case 'd': command = 1;
                  printf(" - DIR\n");
                  if (argc != 4)
                  {
                        printusage(argv[0]);
                  }
                  fname = argv[3];
                  mode = 1;
                  break;
        case 'f': command = 6;
                  printf(" - TURBO FORMAT\n");
                  if (argc == 4)
                  {
                        if (argv[3][0] != '4')
                        {
                                printusage(argv[0]);
                        }
                        mode40 = 1;
                  }
                  break;
        case '0': command = 5;
                  printf(" - TEST USB CONNECTION\n");
                  break;
        case 'x': command = 7;
                  printf(" - COPY FILE(S) FROM C64\n");
                  if (argc == 4)
                  {
                        mode = 1;
                  }
                  break;
        case 't': command = 8;
                  printf(" - WRITE TAP FILE\n");
                  if (argc != 4)
                  {
                        printusage(argv[0]);
                  }
                  fname = argv[3];
                  break;
        case 'm': command = 11;
                  printf(" - READ TAP FILE\n");
                  if (argc != 4)
                  {
                        printusage(argv[0]);
                  }
                  fname = argv[3];
                  break;
        case 'b': command = 10;
                  printf(" - FLASH CRT FILE\n");
                  if (argc != 4)
                  {
                        printusage(argv[0]);
                  }
                  fname = argv[3];
                  break;
        case 's': command = 9;
                  printf(" - SEND PRG TO EF3 MENU\n");
                  if (argc == 4)
                  {
                        fname = argv[3];
                  }
                  else
                  {
                        fname = "ef3usb.prg";
                  }
                  break;
        default: printusage(argv[0]);
  }
  printf("\n");

  buf = (unsigned char *) malloc(0x1000000);
  filebuf = (unsigned char *) malloc(0x1000000);
  bufread = (unsigned char *) malloc(30);
  bufstart = (unsigned char *) malloc(30);

  if (fname != NULL && (command != 3 && command != 11))
  {
        if (!(fp = fopen(fname, "rb")))
        {
                fprintf(stderr, "can't open %s\n", fname);
                return 0;
        }
        read = fread (buf, 1, 0x1000000, fp);
        fclose(fp);
  }

  if (command == 7)
  {
        // COPY FILES FROM C64
        startcommand(0x07);

        printf("- Waiting for files from C64...\n");

        written = serial_read(bufstart,1);                         // get status

        int fnlen;
        while (bufstart[0] == 0)
        {
                for (i=0; i < 19; i++)
                {
                        c64filename[i]=0;
                }
                written = serial_read(bufstart,1);                         // get file length
                fnlen = bufstart[0];
                written = serial_read(c64filename, fnlen);           // get filename

                printf("Transfering %-17s  Bytes: ", c64filename);

                int ok = 0;
                int len=0;
                while (!ok)
                {
                        written = serial_read(buf+len,1);                 // get byte
                        len++;
                        printf("%6d%c%c%c%c%c%c",len,8,8,8,8,8,8);
                        written = serial_read(bufstart, 1);          // status
                        ok = bufstart[0];
                }
                printf("\n");

                // fix filename
                for(i=0; i < fnlen; i++)
                {
                        if (c64filename[i]>=0x20 && c64filename[i]<=0x5f)
                        {
                                bufstart[i]=c64filename[i];
                        }
                        else
                        {
                                bufstart[i]=0x5f;
                        }
                }

                bufstart[fnlen]='.';
                bufstart[fnlen+1]='P';
                bufstart[fnlen+4]=0;
                if (mode == 0)
                {
                        // PRG
                        bufstart[fnlen+2]='R';
                        bufstart[fnlen+3]='G';
                }
                else
                {
                        // P00
                        bufstart[fnlen+2]='0';
                        bufstart[fnlen+3]='0';
                }

                if (!(fp = fopen(bufstart, "wb")))
                {
                        printf("Can't open %s for writing !\n", bufstart);
                }
                else
                {
                        if (mode == 1)
                        {
                                fwrite(p00start, 1 , 8, fp);
                                fwrite(c64filename, 1, 18, fp);
                        }
                        fwrite (buf, 1, len, fp);
                        fclose(fp);
                }

                written = serial_read(bufstart,1);                         // get status
        }
        printf("- End of transfer\n");
  }
  else if (command == 6)
  {
        // TURBO FORMAT DISK
        startcommand(0x06);

        bufstart[0] = 35;                   // normal mode
        if (mode40 == 1)
        {
                bufstart[0] = 40;
        }
        written = serial_write(bufstart,1);
        printf("Formating disk with %d tracks...\n", bufstart[0]);
        written = serial_read(bufstart,1);
  }
  else if (command == 2)
  {
        // WRITE IMAGE

          int format = 0;

          if (read == 174848 || read == 175531)
          {
                format = 35;
          }
          else if (read == 196608 || read == 197376)
          {
                format = 40;
          }
          else if (read == 819200 || read == 822400)
          {
                format = 80;
          }

          if (format == 40 && mode == 1)
          {
                printf("40 tracks D64 is not supported in Kernal mode!\n");
                goto exitpoint;
          }

          if (format == 0)
          {
                printf("Unsupported Image format!\n");
                goto exitpoint;
          }
          else
          {
                if (format == 35 || format == 40)
                {
                        printf (" - .D64 format %d tracks.\n\n", format);
                }
                else
                {
                        printf (" - .D81 format %d tracks.\n\n", format);
                        mode = 1;
                }
          }

        int track;
        int sector;
        int pos = 0;

        printf("Transferring image to C64. ");

        if (mode == 1)
        {
                  // KERNAL WRITE

                  startcommand(0x02);

                  bufstart[0] = 0x00;                   // kernal write
                  written = serial_write(bufstart,1);

                  if (verify)
                  {
                          bufstart[0] = 3;
                  }
                  else
                  {
                          bufstart[0] = 0;              // verify retries #
                  }
                  written = serial_write(bufstart,1);

                  for (track=1; track < format+1; track++)
                  {
                        int numsec;
                        if (format != 80)
                        {
                                numsec = sectorsPerTrack[track-1];
                        }
                        else
                        {
                                numsec = 40;
                        }
                        for (sector=0; sector < numsec; sector++)
                        {
                                printf("T:%2d S:%2d%c%c%c%c%c%c%c%c%c",track,sector,8,8,8,8,8,8,8,8,8);
                                fflush(stdout);

                                bufstart[0] = 0xff;                     // indicate that we are transferring a sector
                                written = serial_write(bufstart, 1);

                                sprintf(bufstart,"%2d %2d",track,sector);
                                written = serial_write(bufstart, 5);       // send over the string containing track & sector

                                // calc checksum
                                checksum = 0;
                                for (i=0; i < 256; i++)
                                {
                                        checksum = checksum + buf[pos+i];
                                }
                                bufstart[0] = checksum;
                                written = serial_write(bufstart, 1);

                                written = serial_write(buf+pos, 256);      // send the sector over
                                pos += 256;

                                sendindicator(format, track, sector);

                                // get ack
                                written = serial_read(bufread, 1);
                                if (bufread[0] != 0xff)
                                {
                                        printf("Error on the C64 side ... exiting...\n");
                                        goto exitpoint;
                                }
                        }
                  }
                  bufstart[0] = 0;        // end of transfer
                  written = serial_write(bufstart, 1);
                  printf("\nDone\n");

        }
        else
        {
                // TURBO WRITE

                  startcommand(0x02);

                  bufstart[0] = 0xff;                   // turbo write
                  written = serial_write(bufstart,1);

                  if (verify)
                  {
                          bufstart[0] = 3;
                  }
                  else
                  {
                          bufstart[0] = 0;              // verify retries #
                  }
                  written = serial_write(bufstart,1);

                  for (track=1; track < format+1; track++)
                  {
                        int numsec;
                        if (format != 80)
                        {
                                numsec = sectorsPerTrack[track-1];
                        }
                        else
                        {
                                numsec = 40;
                        }
                        for (sector=numsec-1; sector >=0 ; sector--)
                        {
                                printf("T:%2d S:%2d%c%c%c%c%c%c%c%c%c",track,sector,8,8,8,8,8,8,8,8,8);

                                bufstart[0] = 0xff;                     // indicate that we are transferring a sector
                                written = serial_write(bufstart, 1);

                                written = serial_write(buf + off(track,sector), 256);      // send the sector over

                                bufstart[0] = (unsigned char) track;
                                bufstart[1] = (unsigned char) sector;
                                written = serial_write(bufstart, 2);       // send over the string containing track & sector

                                sendindicator(format, track, sector);

                                // get ack
                                written = serial_read(bufread, 1);
                                if (bufread[0] != 0xff)
                                {
                                        printf("Error on the C64 side ... exiting...\n");
                                        goto exitpoint;
                                }
                        }
                  }
                  bufstart[0] = 0;        // end of transfer
                  written = serial_write(bufstart, 1);
                  printf("\nDone\n");

        }
  }
  else if (command == 3)
  {
        // READ IMAGE

        int fnamelen = strlen(fname);
        int format = 0;
        if (fnamelen < 4 || fname[fnamelen-4]!='.')
        {
              format = 0;
        }
        else if (fname[fnamelen-1]=='1' && fname[fnamelen-2]=='8')
        {
                mode =1;
                format = 80;
        }
        else if (fname[fnamelen-1]=='4' && fname[fnamelen-2]=='6')
        {
                if (mode40 == 1)
                {
                        format = 40;
                }
                else
                {
                        format = 35;
                }
        }
        if (format == 0 || (format == 80 && mode == 0))
        {
                printf("%d Unsupported format or mode!\n", format);
                goto exitpoint;
        }

        if (mode == 1 && format == 40)
        {
                printf("40 tracks D64 is not supported in Kernal mode!\n");
                goto exitpoint;
        }

        if (format == 35 || format == 40)
        {
                printf (" - .D64 format %d tracks.\n\n", format);
        }
        else
        {
                printf (" - .D81 format %d tracks.\n\n", format);
        }

        int track;
        int sector;
        int pos = 0;

        printf("Transferring image from C64. ");

        if (mode == 1)
        {
                  // KERNAL READ

                  startcommand(0x03);

                  bufstart[0] = 0x00;                   // kernal read
                  written = serial_write(bufstart,1);

                  for (track=1; track < format+1; track++)
                  {
                        int numsec;
                        if (format != 80)
                        {
                                numsec = sectorsPerTrack[track-1];
                        }
                        else
                        {
                                numsec = 40;
                        }
                        for (sector=0; sector < numsec; sector++)
                        {
                                printf("T:%2d S:%2d%c%c%c%c%c%c%c%c%c",track,sector,8,8,8,8,8,8,8,8,8);

                                bufstart[0] = 0xff;                     // indicate that we are transferring a sector
                                written = serial_write(bufstart, 1);

                                sprintf(bufstart,"%2d %2d",track,sector);
                                written = serial_write(bufstart, 5);       // send over the string containing track & sector

                                written = serial_read(buf+pos, 256);      // send the sector over

                                // calc checksum
                                checksum = 0;
                                for (i=0; i < 256; i++)
                                {
                                        checksum = checksum + buf[pos+i];
                                }
                                bufstart[0] = checksum;
                                written = serial_write(bufstart, 1);     // send our checksum over

                                pos += 256;

                                sendindicator(format, track, sector);

                                // get ack
                                written = serial_read(bufread, 1);
                                if (bufread[0] != 0xff)
                                {
                                        printf("Error on the C64 side ... exiting...\n");
                                        goto exitpoint;
                                }
                        }
                  }
                  bufstart[0] = 0;        // end of transfer
                  written = serial_write(bufstart, 1);
                  printf("\nDone... Writing image with %d bytes length.\n", pos);


                   if (!(fp = fopen(fname, "wb"))) {
                    fprintf(stderr, "can't open %s\n", fname);
                    return 0;
                  }

                  written = fwrite (buf, 1, pos, fp);
                  fclose(fp);

        }
        else
        {
                // TURBO READ

                  startcommand(0x03);

                  bufstart[0] = 0xff;                   // turbo read
                  written = serial_write(bufstart,1);
                  int size = 0;

                  for (track=1; track < format+1; track++)
                  {
                        int numsec;
                        if (format != 80)
                        {
                                numsec = sectorsPerTrack[track-1];
                        }
                        else
                        {
                                numsec = 40;
                        }
                        for (sector=numsec-1; sector >=0; sector--)
                        {
                                printf("T:%2d S:%2d%c%c%c%c%c%c%c%c%c",track,sector,8,8,8,8,8,8,8,8,8);

                                bufstart[0] = 0xff;                     // indicate that we are transferring a sector
                                written = serial_write(bufstart, 1);

                                bufstart[0] = (unsigned char) track;
                                bufstart[1] = (unsigned char) sector;
                                written = serial_write(bufstart, 2);       // send over the string containing track & sector

                                written = serial_read(buf + off(track,sector), 256);      // send the sector over

                                size = size+256;

                                sendindicator(format, track, sector);

                                // get ack
                                written = serial_read(bufread, 1);
                                if (bufread[0] != 0xff)
                                {
                                        printf("Error on the C64 side ... exiting...\n");
                                        goto exitpoint;
                                }
                        }
                  }
                  bufstart[0] = 0;        // end of transfer
                  written = serial_write(bufstart, 1);
                  printf("\nDone... Writing image with %d bytes length.\n", size);

                   if (!(fp = fopen(fname, "wb"))) {
                    fprintf(stderr, "can't open %s\n", fname);
                    return 0;
                  }

                  written = fwrite (buf, 1, size, fp);
                  fclose(fp);

        }

  }
  else if (command == 5)
  {
        // TEST

          buf2 = (unsigned char *) malloc(0x10);
          printf("Started Testing... (make sure all 4 jumpers are in DATA position!)\n");

          startcommand(0x05);

          int j;

          written = serial_read(buf, 1);

          for (j=0; j < 64; j++)
          {
                  for (i=0; i < 256; i++)
                  {
                        buf[0] = (unsigned char) i;
                        written = serial_write(buf, 1);
                        if (written != 1)
                        {
                                printf("ERROR - could not write all test bytes to the device!\n");
                                return 1;
                        }
                        written = serial_read(buf2, 1);
                        if (written != 1)
                        {
                                printf("ERROR - could not read back all test bytes to the device!\n");
                                return 1;
                        }
                        if (buf2[0] != (unsigned char) i)
                             {
                                printf("ERROR - Read byte ($%02X) does not match written byte ($%02X) !\n", buf2[0], i);
                                return 1;
                        }
                  }
          }

          free(buf2);
          printf("Testing was Succesfull ! Restart C64...\n");
     }
  else if (command == 0)
  {
        // EXECUTE

          // P00 ?
          int fnamelen = strlen(fname);
          if ((fname[fnamelen-3]=='P' || fname[fnamelen-3]=='p') && fname[fnamelen-2]=='0')
          {
            read = read-0x1a;    // new filelength
            for (i=0; i <read; i++)      // copy file to filebuf
            {
                buf[i] = buf[0x1A + i];
            }
          }

          buf2 = (unsigned char *) malloc(((int)read)*2);

          checksum = 0;

          for (i=0; i < read; i++)
          {
                buf2[i*2] = buf[i] & 0xF2;
                buf2[i*2+1] = ((unsigned char) (buf[i] << 4)) & 0xF2;
                if (i>1)
                {
                        checksum = checksum + buf[i];
                }
          }

          startcommand(0x00);

          bufstart[0] = 0xFF;        // Check for 4 or 8 bit !
          bufstart[1] = 0x00;

          written = serial_write(bufstart, 2);

          if (written != 2)
          {
                printf("ERROR - could not initiate transfer!\n");
                return 1;
          }

          written = serial_read(bufread, 1);

          if (written != 1)
          {
                printf("ERROR - could not read back status byte!\n");
                return 1;
          }

          bufread[0] = bufread[0] & 0xF0;       // just top 4 bits

          if (bufread[0] == 0x10)
          {
                // 8 bit transfer

                printf("Starting 8-bit transfer...\n");

                bufstart[0] = checksum;                                    // checksum

                written = serial_write(bufstart, 1);

                bufstart[0] = (unsigned char) (read&0xff);                 // len low
                bufstart[1] = (unsigned char) ((read>>8)&0xff);            // len high

                written = serial_write(bufstart, 2);

                written = serial_write(buf, read);

                if (written == -1)
                {
                      printf("ERROR - Could not transfer data !\n");
                      return 1;
                }
                printf("Wrote %d bytes\n", written);
          }
          else
          {
                // 4 bit transfer

                printf("Starting 4-bit transfer...\n");

                bufstart[0] = checksum;                                    // checksum
                bufstart[1] = (unsigned char) (checksum<<4);

                written = serial_write(bufstart, 2);

                bufstart[0] = (unsigned char) (read&0xff);                 // len low
                bufstart[1] = (unsigned char) ((read&0xff)<<4);
                bufstart[2] = (unsigned char) ((read>>8)&0xff);            // len high
                bufstart[3] = (unsigned char) (((read>>8)&0xff)<<4);

                written = serial_write(bufstart, 4);

                written = serial_write(buf2, read*2);

                if (written == -1)
                {
                      printf("ERROR - Could not transfer data !\n");
                      return 1;
                }
                printf("Wrote %d half-bytes\n", written);
        }
        free(buf2);
  }
  else if (command == 8)
  {
        // TAP WRITER

          startcommand(0x08);

        size_t size;
        size_t total = 0;
        uint8_t buffer[256];

        printf(" - Sending TAP file. Bytes sent: ");


        fp = fopen(fname, "rb");

        do
        {
            size = fread(buffer, 1, sizeof(buffer), fp);
            if (size)
            {
                total += size;
                if (serial_write(buffer, size) != size)
                {
                    printf("\nERROR!\n");
                    break;
                }
                printf("%7ld%c%c%c%c%c%c%c",total,8,8,8,8,8,8,8);
            }
        }
        while (size);

        printf("\n\nDONE!\n");

        fclose(fp);
  }
  else if (command == 11)
  {
        // TAP READER

        startcommand(0x09);

        size_t  size;
        size_t total = 0;
        uint8_t       buffer[256];

                size_t bufsize = 40000000;

                sprintf(buffer, "C64-TAPE-RAW%c", 0x01);

            unsigned char * buf = (unsigned char *) malloc(bufsize);

        printf(" - Press PLAY on tape to start reading!");

                int notfinished = 1;
        do
        {
            written = serial_read(buf+total, 1);

                        if (written == 1)
                        {
                                total++;
//				printf("%7ld%c%c%c%c%c%c%c",total,8,8,8,8,8,8,8);
                        }
                        if (total > 6)	// check if we pressed run/stop
                        {
                                if (buf[total-5] == 0 &&
                                    buf[total-4] == 1 &&
                                    buf[total-3] == 2 &&
                                    buf[total-2] == 3 &&
                                    buf[total-1] == 4)
                                        {
                                                notfinished = 0;
                                        }
                        }
                        if (total == 1)
                        {
                                printf("\n\n - Reading TAP file... Please STOP the tape to finish!");
                        }
        }
        while (notfinished);

        printf("\n\nDONE!\n");

        if (!(fp = fopen(fname, "wb")))
        {
                fprintf(stderr, "can't open %s\n", fname);
                return 0;
        }

        buffer[0x0d] = buffer[0x0e] = 0x00;
        buffer[0x10] = total&0xff;
        buffer[0x11] = (total>>8)&0xff;
        buffer[0x12] = (total>>16)&0xff;

        written = fwrite (buffer, 1, 0x14, fp);		// write header

        size_t i;
        for (i=0; i < total; i++)
        {
                unsigned char b = buf[i];
                if (b != 0)
                {
                        float tmp = ((float) b) * 1.35;
                        if (tmp > 255.0)
                        {
                                b = 0;
                        }
                        else
                        {
                                b = (unsigned char) tmp;
                        }
                        // Handle $00 pause here !
                        buf[i] = b;
                }
        }

        i = 0;
        int numzero = 0;
        int start = 0;
        int num = 0;
        int totalsize = 0;
        while (i < total)
        {
                if (buf[i] != 0)
                {
                        i++;
                        num++;
                }
                else
                {
                        written = fwrite (buf + start, 1, num, fp);
                        totalsize += num;
                        numzero = 0;
                        while (i < total && buf[i] == 0)
                        {
                                i++;
                                numzero++;
                        }
                        start = i;
                        num = 0;
                        numzero = numzero * 2900;	// actual cycles
                        if (numzero > 0xffffff)
                        {
                                numzero = 0xffffff;
                        }


                        buffer[0x00] = 0x00;
                        buffer[0x01] = numzero&0xff;
                        buffer[0x02] = (numzero>>8)&0xff;
                        buffer[0x03] = (numzero>>16)&0xff;
                        written = fwrite (buffer, 1, 4, fp);		// write pause
                        totalsize += 4;
                }
        }
        if (num > 0)		// finish it off
        {
                written = fwrite (buf + start, 1, num, fp);
                totalsize += num;
        }

        fseek(fp, 0x10, SEEK_SET);

        buffer[0x00] = totalsize&0xff;
        buffer[0x01] = (totalsize>>8)&0xff;
        buffer[0x02] = (totalsize>>16)&0xff;
        buffer[0x03] = (totalsize>>24)&0xff;

        written = fwrite (buffer, 1, 4, fp);		// write length

//		written = fwrite (buf, 1, total, fp);

        fclose(fp);

        free(buf);

  }
  else if (command == 9)
  {
        // SEND PRG TO EF3 MENU

        size_t size;
        size_t total = 0;
        uint8_t buffer[65536];

        int waiting;

        do
        {
                waiting = 0;
                sprintf(bufstart,"EFSTART:PRG%c",0);

                printf("Handshake: EFSTART:PRG\n");
                written = serial_write(bufstart, 12);

                written = serial_read(bufread, 5);

                if (written != 5)
                {
                        printf("Error 3 on the C64 side ... exiting...\n");
                        goto exitpoint;
                }

                printf("Response: [%c%c%c%c]\n",bufread[0],bufread[1],bufread[2],bufread[3]);

                if (bufread[0] == 'W')
                {
                        waiting = 1;
                        printf("Waiting...\n");
                }
                else if (bufread[0] != 'L')
                {
                        printf("Error on the C64 side... exiting...\n");
                        goto exitpoint;
                }
        }
        while (waiting);

        printf("\n - Sending PRG file. Bytes sent: ");

        fp = fopen(fname, "rb");

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

        printf("\n\nDONE!\n");

        fclose(fp);
  }
  else if (command == 10)
  {
        // SEND CRT TO EF3 MENU

        size_t  size;
        size_t total = 0;
        uint8_t       buffer[65536];

        int waiting;

        do
        {
                waiting = 0;
                sprintf(bufstart,"EFSTART:CRT%c",0);

                printf("Handshake: EFSTART:CRT\n");
                written = serial_write(bufstart, 12);

                written = serial_read(bufread, 5);

                if (written != 5)
                {
                                printf("Error 3 on the C64 side ... exiting...\n");
                                goto exitpoint;
                }

                printf("Response: [%c%c%c%c]\n",bufread[0],bufread[1],bufread[2],bufread[3]);

                if (bufread[0] == 'W')
                {
                        waiting = 1;
                        printf("Waiting...\n");
                }
                else if (bufread[0] != 'L')
                {
                                printf("Error on the C64 side... exiting...\n");
                                goto exitpoint;
                }
        }
        while (waiting);

        printf("\n - Sending CRT file. Bytes sent: ");

                fp = fopen(fname, "rb");

                do
        {
                        serial_read(bufread, 2);		// get how many bytes to send

                        written = bufread[0] + bufread[1]*256;

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

        printf("\n\nDONE!\n");

        fclose(fp);
  }
  else if (command == 1)
  {
        // COPY

          unsigned char nexttr = 18;
          unsigned char nextse = 1;

          int prgmode = 0;

          int fnamelen = strlen(fname);
          if (fnamelen < 5 || fname[fnamelen-4]!='.')
          {
                prgmode = 1;
          }
          else if (fname[fnamelen-1]=='1' && fname[fnamelen-2]=='8')
          {
                d81 = 1;
                nexttr = 40;
                nextse = 03;
                nrTracks = 80;
          }
          else if (fname[fnamelen-1]=='1' && fname[fnamelen-2]=='7')
          {
                nrTracks = 70;
          }
          else if ((fname[fnamelen-3]=='P' || fname[fnamelen-3]=='p') &&
                   (fname[fnamelen-2]=='R' || fname[fnamelen-2]=='r') &&
                   (fname[fnamelen-1]=='G' || fname[fnamelen-1]=='g'))
          {
                prgmode = 1;
          }
          else if ((fname[fnamelen-3]=='P' || fname[fnamelen-3]=='p') &&
                    fname[fnamelen-2]=='0')
          {
                prgmode = 2;
          }

          if (mode == 0)
          {
                  startcommand(0x01);
          }

          if (prgmode != 0)
          {
                if (mode != 0)
                {
                        printf("DIR only supported on disk images!\n");
                        goto exitpoint;
                }
                ft = 0x02;      // PRG filetype

                //buf+entoff+0x05 = filename

                if (prgmode == 2)
                {
                        // P00
                        entoff = 0x08 - 0x05; //filename
                        for (i=0; i < 16; i++)
                        {
                                if (buf[entoff+0x05+i] == 0)
                                {
                                        buf[entoff+0x05+i] = 0xA0;      // fix filename
                                }
                        }
                        fileptr = read-0x1a;    // filelength
                        for (i=0; i <fileptr; i++)      // copy file to filebuf
                        {
                                filebuf[i] = buf[0x1A + i];
                        }
                }
                else
                {
                        // PRG
                        fileptr = read;
                        entoff = fileptr;
                        int stop = 0;
                        // check filename start
                        int fnamelen = strlen(fname);
                        int start = fnamelen-1;
                        while (start > 0)
                        {
                                if (fname[start] == '\\' || fname[start] == '/')
                                {
                                        start = start + 1;
                                        break;
                                }
                                start = start-1;
                        }

                        for (i=start+0; i < start+16; i++)
                        {
                                if (i>=strlen(fname) || stop)
                                {
                                        buf[entoff+0x05+(i-start)]=0xA0;            // pad
                                }
                                else
                                {
                                        if (fname[i] == '.' && i==strlen(fname)-4)
                                        {
                                                stop = 1;
                                                buf[entoff+0x05+(i-start)]=0xA0;            // pad
                                        }
                                        else if (fname[i] < 0x60)
                                        {
                                                buf[entoff+0x05+(i-start)] = fname[i];
                                        }
                                        else
                                        {
                                                buf[entoff+0x05+(i-start)] = fname[i]-0x20;
                                        }
                                }
                        }
                        for (i=0; i <fileptr; i++)      // copy file to filebuf
                        {
                                filebuf[i] = buf[i];
                        }
                }
                for (i=0; i < 16; i++)
                {
                        c64filename[i] = buf[entoff+0x05+i];
                        if (c64filename[i] == 0xA0)
                        {
                                c64filename[i] = ' ';
                        }
                }
                c64filename[i] = 0;

                printf(" PRG %s %3d", c64filename, fileptr/256);

                if (sendfile())
                {
                        goto exitpoint;
                }
          }
          else
          {
                  int tt = 0x90;
                  if (d81 == 1)
                  {
                        tt = 0x04;
                  }
                  for (i=0; i < 23; i++)
                  {
                          c64filename[i] = buf[off(nexttr,0)+tt+i];
                          if (c64filename[i] == 0xA0)
                          {
                                c64filename[i] = ' ';
                          }
                  }
                  c64filename[16] = '"';
                  c64filename[17] = ',';
                  printf("    \"%s\n",c64filename);

                  while (nexttr != 0)
                  {
                          int diroff = off(nexttr,nextse);
                          if (diroff == -1)
                          {
                                goto exitpoint;
                          }
                          nexttr = buf[diroff+0];
                          nextse = buf[diroff+1];
                //          printf("lin: $%x next tr %d se %d\n", diroff, nexttr, nextse);

                          int pos = 0;
                          while (pos < 8)
                          {
                                entoff = diroff + pos*0x20;
                                ft = buf[entoff+0x02];
                                int locked = 0;
                                int splat = 0;
                                if (ft&0x40)
                                {
                                        locked = 1;
                                }
                                if (ft&0x80)
                                {
                                        splat = 1;
                                }
                                ft = ft & 0x07;
                                unsigned char lc = ' ';
                                if (locked)
                                {
                                        lc = '>';
                                }
                                else if (splat)
                                {
                                        lc = ' ';
                                }
                                unsigned char filetr = buf[entoff+0x03];
                                unsigned char filese = buf[entoff+0x04];
                                int sizese = buf[entoff+0x1e] + buf[entoff+0x1f]*256;

                                for (i=0; i < 16; i++)
                                {
                                        c64filename[i] = buf[entoff+0x05+i];
                //                        printf("%d\n", c64filename[i]);
                                        if (c64filename[i] == 0xA0)
                                        {
                                                c64filename[i] = ' ';
                                        }
                                }
                                c64filename[i] = 0;

                                if (!(ft == 00 && !locked && !splat))
                                {
                                        printf("%c%s %s %3d", lc, filetypes[ft], c64filename, sizese);
                                        int valid = 1;
                                        if (!d81 && filetr == 18 && (filese == 0 || filese == 1))
                                        {
                                                valid = 0;
                                        }
                                        if (valid && (ft == 0x02 || ft == 0x01))  // PRG or SEQ
                                        {
                                                usednum = 0;
                                                int ok = 1;
                                                int corrupted = 0;
                                                unsigned char currtr = filetr;
                                                unsigned char currse = filese;
                                                fileptr = 0;
                                                while (ok)
                                                {
                                                        for (i=0; i < usednum; i++)
                                                        {
                                                                if (usedtr[usednum] == filetr && usedse[usednum] == filese)
                                                                {
                                                                        corrupted = 1;
                                                                        ok = 0;
                                                                        break;
                                                                }
                                                        }

                                                        if (!corrupted)
                                                        {
                                                                usedtr[usednum] = filetr;
                                                                usedse[usednum] = filese;
                                                                usednum++;

                                                                int cs = off(filetr, filese);
                                                                if (cs == -1)
                                                                {
                                                                        corrupted = 2;
                                                                        ok = 0;
                                                                }

                                                                // next file pointer
                                                                filetr = buf[cs+0x00];
                                                                filese = buf[cs+0x01];

                                                                int secsize = 254;

                                                                if (filetr == 0x00)     // last sector
                                                                {
                                                                        secsize = filese-1;
                                                                        ok = 0;         // exit
                                                                }
                                                                for (i=0; i < secsize; i++)
                                                                {
                                                                        filebuf[fileptr] = buf[cs+0x02+i];
                                                                        fileptr++;
                                                                }
                                                         }
                                                }
                                                if (corrupted)
                                                {
                                                        if (corrupted == 1)
                                                        {
                                                                printf(" - File Loop detected - corrupted file!\n");
                                                        }
                                                }
                                                else
                                                {
                                                        if (mode == 0)
                                                        {
                                                                if (sendfile())
                                                                {
                                                                        goto exitpoint;
                                                                }

                                                                //FILE * fp1 = fopen (c64filename, "wb");
                                                                //fwrite(filebuf, 1, fileptr, fp1);
                                                                //fclose(fp1);

                                                        }
                                                        else
                                                        {
                                                                printf("\n");
                                                        }
                                                }
                                        }
                                        else
                                        {
                                                if (mode != 1)
                                                {
                                                        printf(" - skipping\n");
                                                }
                                                else
                                                {
                                                        printf("\n");
                                                }
                                        }
                                }

                                pos++;
                          }

                  }
          }
          if (mode == 0)
          {
                   bufstart[0] = 0;        // end of transfer
                   written = serial_write(bufstart, 1);
          }

  // END OF COPY
  }

exitpoint:
  free(buf);
  free(filebuf);
  free(bufstart);
  free(bufread);

  close_serial();
}
