#include <string.h>
#include <stdio.h>
#include "xfer.h"


#define XM_SOH 0x01
#define XM_STX 0x02
#define XM_EOT 0x04
#define XM_ACK 0x06
#define XM_NAK 0x15
#define XM_CAN 0x18
#define XM_PAD 0x1a
#define XM_C   0x43

static unsigned short crctab[] = {
    0x0000,  0x1021,  0x2042,  0x3063,  0x4084,  0x50a5,  0x60c6,  0x70e7,
    0x8108,  0x9129,  0xa14a,  0xb16b,  0xc18c,  0xd1ad,  0xe1ce,  0xf1ef,
    0x1231,  0x0210,  0x3273,  0x2252,  0x52b5,  0x4294,  0x72f7,  0x62d6,
    0x9339,  0x8318,  0xb37b,  0xa35a,  0xd3bd,  0xc39c,  0xf3ff,  0xe3de,
    0x2462,  0x3443,  0x0420,  0x1401,  0x64e6,  0x74c7,  0x44a4,  0x5485,
    0xa56a,  0xb54b,  0x8528,  0x9509,  0xe5ee,  0xf5cf,  0xc5ac,  0xd58d,
    0x3653,  0x2672,  0x1611,  0x0630,  0x76d7,  0x66f6,  0x5695,  0x46b4,
    0xb75b,  0xa77a,  0x9719,  0x8738,  0xf7df,  0xe7fe,  0xd79d,  0xc7bc,
    0x48c4,  0x58e5,  0x6886,  0x78a7,  0x0840,  0x1861,  0x2802,  0x3823,
    0xc9cc,  0xd9ed,  0xe98e,  0xf9af,  0x8948,  0x9969,  0xa90a,  0xb92b,
    0x5af5,  0x4ad4,  0x7ab7,  0x6a96,  0x1a71,  0x0a50,  0x3a33,  0x2a12,
    0xdbfd,  0xcbdc,  0xfbbf,  0xeb9e,  0x9b79,  0x8b58,  0xbb3b,  0xab1a,
    0x6ca6,  0x7c87,  0x4ce4,  0x5cc5,  0x2c22,  0x3c03,  0x0c60,  0x1c41,
    0xedae,  0xfd8f,  0xcdec,  0xddcd,  0xad2a,  0xbd0b,  0x8d68,  0x9d49,
    0x7e97,  0x6eb6,  0x5ed5,  0x4ef4,  0x3e13,  0x2e32,  0x1e51,  0x0e70,
    0xff9f,  0xefbe,  0xdfdd,  0xcffc,  0xbf1b,  0xaf3a,  0x9f59,  0x8f78,
    0x9188,  0x81a9,  0xb1ca,  0xa1eb,  0xd10c,  0xc12d,  0xf14e,  0xe16f,
    0x1080,  0x00a1,  0x30c2,  0x20e3,  0x5004,  0x4025,  0x7046,  0x6067,
    0x83b9,  0x9398,  0xa3fb,  0xb3da,  0xc33d,  0xd31c,  0xe37f,  0xf35e,
    0x02b1,  0x1290,  0x22f3,  0x32d2,  0x4235,  0x5214,  0x6277,  0x7256,
    0xb5ea,  0xa5cb,  0x95a8,  0x8589,  0xf56e,  0xe54f,  0xd52c,  0xc50d,
    0x34e2,  0x24c3,  0x14a0,  0x0481,  0x7466,  0x6447,  0x5424,  0x4405,
    0xa7db,  0xb7fa,  0x8799,  0x97b8,  0xe75f,  0xf77e,  0xc71d,  0xd73c,
    0x26d3,  0x36f2,  0x0691,  0x16b0,  0x6657,  0x7676,  0x4615,  0x5634,
    0xd94c,  0xc96d,  0xf90e,  0xe92f,  0x99c8,  0x89e9,  0xb98a,  0xa9ab,
    0x5844,  0x4865,  0x7806,  0x6827,  0x18c0,  0x08e1,  0x3882,  0x28a3,
    0xcb7d,  0xdb5c,  0xeb3f,  0xfb1e,  0x8bf9,  0x9bd8,  0xabbb,  0xbb9a,
    0x4a75,  0x5a54,  0x6a37,  0x7a16,  0x0af1,  0x1ad0,  0x2ab3,  0x3a92,
    0xfd2e,  0xed0f,  0xdd6c,  0xcd4d,  0xbdaa,  0xad8b,  0x9de8,  0x8dc9,
    0x7c26,  0x6c07,  0x5c64,  0x4c45,  0x3ca2,  0x2c83,  0x1ce0,  0x0cc1,
    0xef1f,  0xff3e,  0xcf5d,  0xdf7c,  0xaf9b,  0xbfba,  0x8fd9,  0x9ff8,
    0x6e17,  0x7e36,  0x4e55,  0x5e74,  0x2e93,  0x3eb2,  0x0ed1,  0x1ef0,
};


unsigned short crc16_calc(unsigned char *buffer, int len) {
  unsigned short crc = 0;

  while (len-- > 0) {
    crc = (crc<<8) ^ crctab[(crc>>8) ^ (*buffer++)];
  }
  return (crc);
}


int crc_init(void) {
  return(0);
}

void xmodem_fail(char *message) {
  printf("xmodem_fail: %s\n", message);
  while (xfer_recv_byte(1000) >= 0);
  xfer_send_byte(XM_CAN);
  xfer_send_byte(XM_CAN);
}


void xmodem_retry(char *message) {
  printf("xmodem_retry: %s\n", message);
  while (xfer_recv_byte(1000) >= 0);
  xfer_send_byte(XM_NAK);
}


int xmodem_checksum(int blocksize, int usecrc) {
  int i;
  unsigned short remotecrc;
  unsigned char checksum = 0;
  unsigned short crc = 0;

  if (blocksize == 128 && usecrc == 0) {
    for (i = 0; i < 128; ++i) {
      checksum += xfer_buffer[i];
    }
    //printf("xfer_checksum: %02x (%02x)\n\n", checksum, xfer_buffer[i]);
    if (checksum == xfer_buffer[i]) {
      return(1);
    } else {
      return(0);
    }
  } else if (blocksize == 128 && usecrc) {
    crc = crc16_calc(xfer_buffer, 128);
    remotecrc = (xfer_buffer[128]<<8) | xfer_buffer[129];
    //printf("xfer_crc16: %04x (%04x)\n\n", crc, remotecrc);
    if (crc == remotecrc) {
      return(1);
    } else {
      return(0);
    }
  } else if (blocksize == 1024 && usecrc) {
    crc = crc16_calc(xfer_buffer, 1024);
    remotecrc = (xfer_buffer[1024]<<8) | xfer_buffer[1025];
    //printf("xfer_crc16: %04x (%04x)\n\n", crc, remotecrc);
    if (crc == remotecrc) {
      return(1);
    } else {
      return(0);
    }
  } else {
    return(0);
  }
}


static unsigned char xmodem_buffer[1024];
static unsigned int xmodem_buffer_len = 0;

int xmodem_save_data(unsigned char *data, int length) {
  if (data) {
    if (xmodem_buffer_len) {
      // flush previous block
      if (xfer_save_data(xmodem_buffer, xmodem_buffer_len) != xmodem_buffer_len) {
	return(0); // ouch
      }
    }
    // buffer new block
    memcpy(xmodem_buffer, data, length);
    xmodem_buffer_len = length;
  } else { // strip padding and flush
    if (xmodem_buffer_len) {
      while (xmodem_buffer_len && xmodem_buffer[xmodem_buffer_len - 1] == XM_PAD) {
	--xmodem_buffer_len;
      }
      if (xmodem_buffer_len) {
	if (xfer_save_data(xmodem_buffer, xmodem_buffer_len) != xmodem_buffer_len) {
	  return(0); // ouch
	}
      }
    }
  }
  return(length);
}


int xmodem_recv(int usecrc) {
  int c;
  int blocknum, bufctr, errorcnt, blocksize;
  int nexthandshake;

  xmodem_buffer_len = 0;

  blocknum = 1;
  if (usecrc) {
    nexthandshake = XM_C;
  } else {
    nexthandshake = XM_NAK;
  }
  xfer_send_byte(nexthandshake);

  printf("Starting...\n");

  for (;;) {

    if (xfer_cancel) {
      xmodem_fail("\nCancelling...");
      return(0);
    }

    errorcnt = 0;
    while ((c = xfer_recv_byte(10000)) == -1 && errorcnt < 10) {
      printf("\nTimeout...\n");
      xfer_send_byte(nexthandshake);
      ++errorcnt;
    }
    nexthandshake = XM_NAK;
    switch (c) {
    case -2:
      // connection closed
      xmodem_fail("\nDisconnected!");
      return(0);
      break;
    case -1:
      // timeout
      xmodem_fail("\nTimeout!");
      return(0);
      break;
    case XM_CAN:
      xmodem_fail("\nRemote cancel!");
      return(0);
      break;
    case XM_STX:
      //printf("xfer_recv_xmodem: receiving 1k block\n");
      blocksize = 1026;
      goto getblock;
    case XM_SOH:
      //printf("xfer_recv_xmodem: receiving 128 byte block\n");
      if (usecrc) {
	blocksize = 130;
      } else {
	blocksize = 129;
      }
    getblock:
      // new block
      c = xfer_recv_byte_error(1000, 10);
      if (c != (blocknum & 255) && c != ((blocknum - 1) & 255)) {
	printf("\nxfer_recv_xmodem: got block %d, expected %d or %d\n", c, blocknum & 255, (blocknum - 1) & 255);
	xmodem_retry("\nWrong block, retrying");
	return(0);
      }
      if (255 - xfer_recv_byte_error(1000, 10) != c) {
	xmodem_retry("\nBlock mismatch, retrying");
      }
      if (c == ((blocknum - 1) & 255)) {
	--blocknum;
      }
      printf(".");
      fflush(stdout);
      //xfer_debug = 0;
      for (bufctr = 0; bufctr < blocksize && c >= 0; ++bufctr) {
	c = xfer_recv_byte_error(1000, 5);
	xfer_buffer[bufctr] = c;
	//printf("xfer_recv_xmodem: received byte %d\n", bufctr);
      }
      //xfer_debug = 1;
      if (c < 0) {
	printf("\nbufctr = %d, c = %d\n", bufctr, c);
	// lost sync
	xmodem_retry("\nLost sync, retrying...");
      } else {
	if (xmodem_checksum(blocksize & 0xfff0, usecrc)) {
	  if (blocknum == 0) {
	    printf("\nnot saving block 0\n");
	  } else {
	    if (xmodem_save_data(xfer_buffer, blocksize & 0xfff0)) {
	      //printf("Downloading...\n");
	    } else {
	      xmodem_fail("\nWrite error!");
	    }
	  }
	  xfer_send_byte(XM_ACK);
	  nexthandshake = XM_ACK;
	  ++blocknum;
	  //puts("");
	} else {
	  // checksum failed
	  xmodem_retry("\nChecksum failed, retrying...");
	}
      }
      break;
    case XM_EOT:
      // end of transmission
      xmodem_save_data(NULL, 0);
      xfer_send_byte(XM_ACK);
      printf("\nTransfer complete\n");
      return(1);
      break;
    default:
      // purge and cancel
      xmodem_fail("Wtf!?");
      return(0);
      break;
    }

  }
}


int xmodem_load_block(int blocksize) {
  int l;

  l = xfer_load_data(xfer_buffer, blocksize);
  if (l == 0) {
    return(0);
  }
  if (l < blocksize) {
    memset(xfer_buffer + l, XM_PAD, blocksize - l);
  }
  return(blocksize);
}

void xmodem_send_block(unsigned char blocknum, int blocksize, int usecrc) {
  int i;
  unsigned char cksum;
  unsigned short crc;
  //int debug;

  printf(".");
  fflush(stdout);

  //puts("xmodem_send_block: sending block header");
  xfer_send_byte(blocksize == 128 ? XM_SOH : XM_STX);
  xfer_send_byte(blocknum & 0xff);
  xfer_send_byte(0xff ^ (blocknum & 0xff));

  //puts("xmodem_send_block: sending data");
  //debug = xfer_debug;
  //xfer_debug = 0;
  for (i = 0; i < blocksize; ++i) {
    xfer_send_byte(xfer_buffer[i]);
  }
  //xfer_debug = debug;

  if (usecrc) {
    crc = crc16_calc(xfer_buffer, blocksize);
    //puts("xmodem_send_block: sending crc");
    xfer_send_byte(crc>>8);
    xfer_send_byte(crc & 0xff);
  } else {
    cksum = 0;
    for (i = 0; i < 128; ++i) {
      cksum += xfer_buffer[i];
    }
    //puts("xmodem_send_block: sending checksum");
    xfer_send_byte(cksum);
  }
  //puts("");
}


int xmodem_send(int send1k) {
  int c, usecrc, blocknum, bytesleft, sentbytes, blocksize;

  //puts("xmodem_send: waiting for start code");

  printf("Starting...\n");

  c = 0;
  while (c != XM_C && c != XM_NAK) {
    c = xfer_recv_byte(1000);
    if (xfer_cancel) {
      printf("Canceled\n");
      return(0);
    }
  }

  if (c == XM_C) {
    //puts("xmodem_send: using CRC");
    usecrc = 1;
  } else {
    //puts("xmodem_send: using checksum");
    usecrc = 0;
  }

  /*
  menu_update_xfer_progress("Sending header...", 0, xfer_file_size);
  gfx_vbl();
  memset(xfer_buffer, 'X', 128);
  xmodem_send_block(0, 128, usecrc);
  */

  bytesleft = xfer_file_size;
  sentbytes = 0;
  blocknum = 1;
  c = XM_NAK;

  printf("Uploading...\n");

  if (bytesleft > 896 && send1k && usecrc) {
    blocksize = 1024;
  } else {
    blocksize = 128;
  }
  if (xmodem_load_block(blocksize) == 0) {
    printf("Read error\n");
    return(0);
  }
  xmodem_send_block(blocknum, blocksize, usecrc);

  while (bytesleft > 0) {
    c = xfer_recv_byte(10000);
    switch (c) {

    case -2:
      xmodem_fail("\nDisconnected!");
      return(0);
      break;

    case XM_CAN:
      //puts("xmodem_send: cancel");
      printf("\nCanceled by remote!\n");
      return(0);
      break;

    case XM_ACK:
      //puts("xmodem_send: ack");
      if (blocknum != 0) {
	sentbytes += blocksize;
	bytesleft -= blocksize;
      }
      ++blocknum;
      //printf("Uploading...\n");
      if (bytesleft > 896 && send1k && usecrc) {
	blocksize = 1024;
      } else {
	blocksize = 128;
      }
      if (bytesleft > 0) {
	if (xmodem_load_block(blocksize) == 0) {
	  printf("\nRead error\n");
	  return(0);
	}
	xmodem_send_block(blocknum, blocksize, usecrc);
      }
      break;

    case XM_NAK:
      //puts("xmodem_send: nak");
      printf("\nResending...\n");
      xmodem_send_block(blocknum, blocksize, usecrc);
      break;

    default:
      break;
    }
  }

  //puts("xmodem_send: file sent");

  printf("\nFinishing...\n");

  xfer_send_byte(XM_EOT);
  if (xfer_recv_byte(10000) != XM_ACK) {
    xfer_send_byte(XM_EOT);
  }

  printf("Transfer complete\n");
  return(1);
}