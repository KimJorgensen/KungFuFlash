#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "net.h"
#include "xfer.h"
#include "xmodem.h"
#include "punter.h"

#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#else
#include <windows.h>
#include <time.h>

// from https://www.c-plusplus.net/forum/topic/109539/usleep-unter-windows
void usleep(unsigned int usec)
{
	HANDLE timer;
	LARGE_INTEGER ft;

	ft.QuadPart = -(10 * (__int64)usec);

	timer = CreateWaitableTimer(NULL, TRUE, NULL);
	SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}
#endif

char *xfer_tempdlname = "download.tmp";
char *xfer_tempulname = "upload.tmp";
char xfer_filename[256];

FILE *xfer_sendfile, *xfer_recvfile;

Direction xfer_direction;
Protocol xfer_protocol;
int xfer_cancel;
int xfer_saved_bytes;
int xfer_file_size;
unsigned char xfer_buffer[4096];
unsigned long long xfer_starttime;

//int xfer_debug = 1;

#ifndef _WIN32
struct timeval tv;

unsigned long long timer_get_ticks(void) {
	gettimeofday(&tv, NULL);

	unsigned long long milli = (unsigned long long)(tv.tv_sec) * 1000 +
		(unsigned long long)(tv.tv_usec) / 1000;

	return milli;
}
#else
struct timespec ts;

unsigned long long timer_get_ticks(void) {
	timespec_get(&ts, TIME_UTC);

	unsigned long long milli = (unsigned long long)(ts.tv_sec) * 1000 +
		(unsigned long long)(ts.tv_nsec) / 1000000;

	return milli;
}
#endif

void timer_delay(unsigned long delay) {
  unsigned long long curr=timer_get_ticks();
  while (timer_get_ticks()-curr < delay)
  {
	usleep(1);
  }
}

void xfer_send_byte(unsigned char c) {
  unsigned int t;

  net_send(c);
}


signed int xfer_recv_byte(int timeout) {
  signed int c;
  unsigned long long starttime, t;

  unsigned char onechar;

  starttime = timer_get_ticks();
  while ((c = net_get(&onechar)) == -1) {
    if (timer_get_ticks() > starttime + timeout) {
      t = timer_get_ticks() - xfer_starttime;
      //printf("xfer_recv_byte[%3d.%03d]: timeout\n", t / 1000, t % 1000);
      return(-1);
    } else {
      timer_delay(1);
      }
    }

  return(onechar);
}


signed int xfer_recv_byte_error(int timeout, int errorcnt) {
  signed int c;

  while ((c = xfer_recv_byte(timeout)) == -1 && errorcnt) {
    --errorcnt;
  }
  return(c);
}


int xfer_save_data(unsigned char *data, int length) {
  int l;
  int written = 0;

  while (written < length) {
    if ((l = fwrite(data + written, 1, length - written, xfer_recvfile))) {
      written += l;
    } else {
      return(0);
    }
  }
  xfer_saved_bytes += written;
  return(written);
}


int xfer_load_data(unsigned char *data, int length) {
  int l;
  int read = 0;

  while (read < length) {
    if ((l = fread(data + read, 1, length - read, xfer_sendfile))) {
      read += l;
    } else {
      if (ferror(xfer_sendfile)) {
	return(0);
      } else {
	return(read);
      }
    }
  }
  return(read);
}


int xfer_recv(int prot) {
  int status = 0;

  xfer_filename[0] = 0;
  xfer_cancel = 0;
  xfer_starttime = timer_get_ticks();

   if ((xfer_recvfile = fopen(xfer_tempdlname, "wb"))) {
    xfer_saved_bytes = 0;
	if (prot==0)
	{
      status = punter_recv();
	}
	if (prot==1)
	{
      status = xmodem_recv(0);
	}
	if (prot==2)
	{
      status = xmodem_recv(1);
	}
    fclose(xfer_recvfile);
    return(status);
  } else {
  printf("Coulnd't open temp file!\n");
    return(0);
  }

}


void xfer_send(char *filename) {
  char name[256];
  char *p;
  int deletetmp = 0;

  xfer_cancel = 0;
  xfer_starttime = timer_get_ticks();

  if ((xfer_sendfile = fopen(filename, "rb"))) {

    if (fseek(xfer_sendfile, 0, SEEK_END)) {
      fclose(xfer_sendfile);
      printf("Couldn't read file size!\n");
      return;
    }
    xfer_file_size = ftell(xfer_sendfile);
    fseek(xfer_sendfile, 0, SEEK_SET);

    xmodem_send(0);

    fclose(xfer_sendfile);
  } else {
    printf("Couldn't open file!\n");
  }

  if (deletetmp) {
    remove(name);
  }
}


void xfer_save_file_in_dir(char *filename) {
  FILE *from, *to;
  unsigned char buf[4096];
  int bytesleft, l;
  char name[256];

  if ((from = fopen(xfer_tempdlname, "rb")) == NULL) {
    return;
  }

  if ((to = fopen(filename, "wb")) == NULL) {
    fclose(from);
    return;
  }

  bytesleft = xfer_saved_bytes;
  while (bytesleft) {
    l = fread(buf, 1, 4096, from);
    if (ferror(from)) {
      goto done;
    }
    bytesleft -= fwrite(buf, 1, l, to);
    if (ferror(to)) {
      goto done;
    }
  }

  if (bytesleft != 0) {
    goto done;
  }

  remove(xfer_tempdlname);

  printf("Saved %-24s\n\n", filename);

 done:
  fclose(from);
  fclose(to);
}


void xfer_save_file(char *filename) {

  xfer_save_file_in_dir(filename);
}
