#include <string.h>
#include <stdio.h>
#include "xfer.h"


void punter_fail(char *message) {
  printf("punter_fail: %s\n", message);
}


void punter_retry(char *message) {
  printf("punter_retry: %s\n", message);
}


void punter_send_string(char *s) {
  printf("punter_send_string: %s\n", s);
  while (*s) {
    xfer_send_byte(*s++);
  }
}


int punter_recv_string(char *sendstring, char *recvstring) {
  int c, bytecnt;
  int errorcnt;

  printf("punter_recv_string: sending \"%s\"\n", sendstring);
  memset(recvstring, 0, 4);
  punter_send_string(sendstring);
  bytecnt = 0;
  errorcnt = 10;
  while (bytecnt != 3) {
    while ((c = xfer_recv_byte(1000)) < 0 && errorcnt-- && (xfer_cancel == 0)) {
      printf("punter_recv_string: timeout %d\n", errorcnt);
      punter_send_string(sendstring);
      printf("punter_recv_string: sending \"%s\"\n", sendstring);
      bytecnt = 0;
    }
    if (errorcnt && (xfer_cancel == 0)) {
      recvstring[bytecnt++] = c;
    } else {
      return(0);
    }
  }
  recvstring[4] = 0;
  printf("punter_recv_string: received \"%s\"\n", recvstring);

  return(3);
}


int punter_handshake(char *sendstring, char *waitstring) {
  char p[4];
  int l = 0;
  int errorcnt = 10;

  while (errorcnt--) {
    l = punter_recv_string(sendstring, p);
    if (l == 3) {
      if (strcmp(waitstring, p) == 0) {
	printf("punter_handshake: got \"%s\", done\n", waitstring);
	return(1);
      } else if (strcmp(sendstring, p) == 0) {
	printf("punter_handshake: got echoed \"%s\", failed\n", sendstring);
	return(0);
      } else {
	if (strcmp("ACK", waitstring) == 0) {
	  if (strcmp("CKA", p) == 0) {
	    xfer_recv_byte(100);
	    xfer_recv_byte(100);
	    return(1);
	  }
	  if (strcmp("KAC", p) == 0) {
	    xfer_recv_byte(100);
	    return(1);
	  }
	}
      }
    }
  }
  printf("punter_handshake: failed\n");
  return(0);
}


int punter_checksum(int len) {
  unsigned short cksum = 0;
  unsigned short clc = 0;
  unsigned char *data = xfer_buffer + 4;

  len -= 4;
  while (len--) {
    cksum += *data;
    clc ^= *data++;
    clc = (clc<<1) | (clc>>15);
  }
  if (cksum == (xfer_buffer[0] | (xfer_buffer[1]<<8))) {
    if (clc == (xfer_buffer[2] | (xfer_buffer[3]<<8))) {
      return(1);
    }
  }
  printf("punter_checksum: cksum = %04x (%04x)\n", cksum, xfer_buffer[0] | (xfer_buffer[1]<<8));
  printf("punter_checksum:   clc = %04x (%04x)\n", clc, xfer_buffer[2] | (xfer_buffer[3]<<8));
  return(0);
}


unsigned short punter_next_blocknum(void) {
  return(xfer_buffer[5] | (xfer_buffer[6]<<8));
}


signed int punter_recv_block(int len) {
  signed int c;
  int bytecnt;
  int errorcnt = 10;

 restart:
  printf("punter_recv_block: receiving %d byte block\n", len);
  punter_send_string("S/B");
  bytecnt = 0;
  while (bytecnt < len) {
    if ((c = xfer_recv_byte_error(500, 10)) < 0) {
      if (bytecnt == 3) {
	if (strncmp("S/B", xfer_buffer, 3) == 0) {
	  printf("Transfer canceled by remote\n");
	  return(-1);
	}
    }
    printf("Block timed out, retrying\n");
      if (punter_handshake("BAD", "ACK")) {
	if (errorcnt--) {
	  goto restart;
	} else {
	  printf("Block timed out\n");
	  return(-1);
	}
      } else {
	printf("punter_recv_block: bad handshake timeout\n");
	return(-1);
      }
    }
    printf("punter_recv_block: received byte %3d: %02x\n", bytecnt, c);
    xfer_buffer[bytecnt++] = c;
    if (bytecnt == 4) {
      if (strncmp("ACK", xfer_buffer, 3) == 0) {
		printf("Lost sync, retrying...\n");
		if (xfer_buffer[3] == 'A') {
	  goto restart;
	} else {
	  printf("punter_recv_block: skipping late ack\n");
	  xfer_buffer[0] = xfer_buffer[3];
	  bytecnt = 1;
	}
      }
    }
    if (bytecnt == 8) {
      if (strncmp("ACKACK", xfer_buffer + 2, 6) == 0) {
	printf("punter_recv_block: lost sync, restarting block\n");
	goto restart;
      }
      if (strncmp("CKACKA", xfer_buffer + 2, 6) == 0) {
	printf("punter_recv_block: lost sync, restarting block\n");
	goto restart;
      }
      if (strncmp("KACKAC", xfer_buffer + 2, 6) == 0) {
	printf("punter_recv_block: lost sync, restarting block\n");
	goto restart;
      }
    }
  }
  if (punter_checksum(bytecnt)) {
    if (punter_handshake("GOO", "ACK") == 0) {
      printf("punter_recv_block: goo handshake timeout\n");
      return(-1);
    }
    printf("Downloading...\n");
    if (len <= 8) {
      printf("punter_recv_block: short block, returning %d\n", xfer_buffer[4]);
      return(xfer_buffer[4]);
    }
    if (xfer_save_data(xfer_buffer + 7, len - 7)) {
      printf("punter_recv_block: returning %d\n", xfer_buffer[4]);
      return(xfer_buffer[4]);
    } else {
      punter_fail("Write error!");
      return(-1);
    }
  } else {
    printf("punter_recv_block: checksum failed\n");
    if (punter_handshake("BAD", "ACK")) {
      if (errorcnt--) {
	goto restart;
      } else {
	printf("Checksum failed\n");
	return(-1);
      }
    } else {
      printf("punter_recv_block: bad handshake timeout\n");
      return(-1);
    }
  }
}


void punter_countdown(int num) {
  char s[24];

  printf(s, "Starting in %d", num);
}


int punter_recv(void) {
  signed int nextblocksize;

  printf("Starting...\n");

  if (punter_handshake("GOO", "ACK")) {
    punter_countdown(5);
  } else {
    punter_fail("Timed out");
    return(0);
  }

  nextblocksize = punter_recv_block(8);
  if (nextblocksize < 0) {
    punter_fail("Timed out on filetype block");
    return(0);
  }

  if (punter_handshake("GOO", "ACK")) {
    punter_countdown(4);
  } else {
    punter_fail("Handshake timeout");
    return(0);
  }

  if (punter_handshake("S/B", "SYN")) {
    punter_countdown(3);
  } else {
    punter_fail("Handshake timeout");
    return(0);
  }

  if (punter_handshake("SYN", "S/B")) {
    punter_countdown(2);
  } else {
    punter_fail("Handshake timeout");
    return(0);
  }

  if (punter_handshake("GOO", "ACK")) {
    punter_countdown(1);
  } else {
    punter_fail("Handshake timeout");
    return(0);
  }

  nextblocksize = punter_recv_block(7);
  if (nextblocksize < 0) {
    // error
    punter_fail("file start timeout");
    return(0);
  }

  while (punter_next_blocknum() < 0xff00 && nextblocksize >= 7) {
    nextblocksize = punter_recv_block(nextblocksize);
  }
  if (nextblocksize < 0) {
    //punter_fail("Block timeout");
    return(0);
  }


  if (punter_handshake("S/B", "SYN")) {
    punter_handshake("SYN", "S/B");
    printf("Finished\n");
	} else {
   printf("Done, but handshake timed out\n");
   }
  return(1);
}
