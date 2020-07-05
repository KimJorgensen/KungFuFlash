;**************************************************************************
;*
;* FILE  format2_runtime.i
;* Copyright (c) 2010 Daniel Kahlin <daniel@kahlin.net>
;* Written by Daniel Kahlin <daniel@kahlin.net>
;*
;* DESCRIPTION
;*   1540/41/70/71 Fast formatter MK II.
;*
;******

FORMAT2_BASE	=	$c000

fmt2_format	=	FORMAT2_BASE+$00
fmt2_fetch_scan	=	FORMAT2_BASE+$03


FMT2_LAST_STATUS	=	0
FMT2_LAST_TRACK		=	1
FMT2_LAST_ZONE		=	2
FMT2_SCAN_ISGAP_TAB	=	3+4*0
FMT2_SCAN_CAPL_TAB	=	3+4*1
FMT2_SCAN_CAPH_TAB	=	3+4*2
FMT2_SCAN_GAPMAX_TAB	=	3+4*3
FMT2_SCAN_GAPMIN_TAB	=	3+4*4
FMT2_SCAN_BUF_LEN	=	3+4*5

; eof
