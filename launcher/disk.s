;
; Copyright (c) 2019-2022 Kim Jørgensen
; Copyright (c) 2020 Sandor Vass
;
; Derived from Disk2easyflash 0.9.1 by ALeX Kazik
; and EasyFlash 3 Boot Image
; Copyright (c) 2012-2013 Thomas Giesel
;
; This software is provided 'as-is', without any express or implied
; warranty.  In no event will the authors be held liable for any damages
; arising from the use of this software.
;
; Permission is granted to anyone to use this software for any purpose,
; including commercial applications, and to alter it and redistribute it
; freely, subject to the following restrictions:
;
; 1. The origin of this software must not be misrepresented; you must not
;    claim that you wrote the original software. If you use this software
;    in a product, an acknowledgment in the product documentation would be
;    appreciated but is not required.
; 2. Altered source versions must be plainly marked as such, and must not be
;    misrepresented as being the original software.
; 3. This notice may not be removed or altered from any source distribution.
;

.import init_system
.import init_basic
.import start_basic
.include "ef3usb_macros.s"

; 6502 Instructions
BIT_INSTRUCTION = $2c

; BASIC functions
MAIN            = $a483                 ; Normal BASIC warm start

; Kernal functions
LUKING          = $f5af                 ; Print "SEARCHING"
LODING          = $f5d2                 ; Print "LOADING"
SAVING          = $f68f                 ; Print "SAVING"
BSOUT           = $ffd2                 ; Write byte to default output

; Kernal vectors
NMINV           = $0318
IOPEN           = $031a
ICLOSE          = $031c
ICHKIN          = $031e
ICKOUT          = $0320
ICLRCH          = $0322
IBASIN          = $0324
IBSOUT          = $0326
IGETIN          = $032a
ICLALL          = $032c
ILOAD           = $0330
ISAVE           = $0332

; Kernal tables
LAT             = $0259                 ; Logical file table
FAT             = $0263                 ; Device number table
SAT             = $026d                 ; Secondary address table
KEYD            = $0277                 ; Keyboard buffer

; Kernal ZP
R6510           = $01                   ; 6510 I/O port
STATUS          = $90                   ; Device status
VERCK           = $93                   ; Load/verify flag
LDTND           = $98                   ; Open file count
DFLTN           = $99                   ; Default input device
DFLTO           = $9a                   ; Default output device
SAL             = $ac                   ; Save pointer (low byte)
SAH             = $ad                   ; Save pointer (high byte)
EAL             = $ae                   ; Load/save end address (low byte)
EAH             = $af                   ; Load/save end address (high byte)
FNLEN           = $b7                   ; Length of filename
LA              = $b8                   ; Logical file
SA              = $b9                   ; Secondary address
FA              = $ba                   ; Device number
FNADR           = $bb                   ; Pointer to filename
STAL            = $c1                   ; Save start address (low byte)
STAH            = $c2                   ; Save start address (high byte)
MEMUSS          = $c3                   ; Load start address (2 bytes)
NDX             = $c6                   ; Characters in keyboard buffer
INDX            = $c8                   ; Length of line
CRSW            = $d0                   ; Input/get flag
PNTR            = $d3                   ; Cursor column
QTSW            = $d4                   ; Quotation mode flag

; Kernal error codes
ERROR_FILE_NOT_FOUND    = $04

; Kernal status codes
STATUS_END_OF_FILE      = $40
STATUS_READ_ERROR       = $42
STATUS_SAVE_FILE_EXISTS = $80
STATUS_SAVE_DISK_FULL   = $83

KFF_SAVE_RAM            = $0100
KFF_SAVE_RAM_SIZE       = 22

KFF_DATA                = $de00
KFF_COMMAND             = $de01
KFF_CONTROL             = $de02
KFF_RAM_TST             = $de03
KFF_WRITE_LPTR          = $de06
KFF_WRITE_HPTR          = $de07
KFF_RAM                 = $de08

KFF_RAM_SIZE            = $00f8
KFF_KILL                = $00
KFF_ENABLE              = $01

; Align with commands.h
CMD_NO_DRIVE            = $10
CMD_DISK_ERROR          = $11
CMD_NOT_FOUND           = $12
CMD_END_OF_FILE         = $13
CMD_WAIT_SYNC           = $50
CMD_SYNC                = $55

REPLY_OK                = $80
REPLY_LOAD              = $90
REPLY_SAVE              = $91
REPLY_OPEN              = $92
REPLY_CLOSE             = $93
REPLY_TALK              = $94
REPLY_UNTALK            = $95
REPLY_SEND_BYTE         = $96
REPLY_LISTEN            = $97
REPLY_UNLISTEN          = $98
REPLY_RECEIVE_BYTE      = $99

; =============================================================================
VECTOR_PAGE     = IOPEN & $ff00
OLD_VECTOR_PAGE = old_open_vector & $ff00
NEW_VECTOR_PAGE = >open_trampoline

vectors:
        .lobytes IOPEN, ICLOSE, ICHKIN
        .lobytes ICKOUT, ICLRCH, IBASIN
        .lobytes IBSOUT, IGETIN, ICLALL
        .lobytes ILOAD, ISAVE
vectors_end:

VECTORS_SIZE = vectors_end - vectors

old_vectors:
        .lobytes old_open_vector, old_close_vector, old_chkin_vector
        .lobytes old_ckout_vector, old_clrch_vector, old_basin_vector
        .lobytes old_bsout_vector, old_getin_vector, old_clall_vector
        .lobytes old_load_vector, old_save_vector

new_vectors:
        .lobytes open_trampoline, close_trampoline, chkin_trampoline
        .lobytes ckout_trampoline, clrch_trampoline, basin_trampoline
        .lobytes bsout_trampoline, getin_trampoline, clall_trampoline
        .lobytes load_trampoline, save_trampoline

; =============================================================================
;
; void disk_mount_and_load(void);
;
; =============================================================================
.proc   _disk_mount_and_load
.export _disk_mount_and_load
_disk_mount_and_load:
        sei
        ldx #$ff                        ; Reset stack
        txs
        jsr init_system

        ; === Disk API in KFF RAM ===
        ldx #disk_api_end - disk_api
:       lda disk_api - 1, x
        sta KFF_RAM - 1, x
        dex
        bne :-

        lda KFF_DATA                    ; First byte is device number
        sta kff_device_number

        ldy #VECTORS_SIZE - 1           ; Store old vectors
:       ldx vectors, y
        lda VECTOR_PAGE, x
        ldx old_vectors, y
        sta OLD_VECTOR_PAGE, x

        ldx vectors, y
        lda VECTOR_PAGE + 1, x
        ldx old_vectors, y
        sta OLD_VECTOR_PAGE + 1, x
        dey
        bpl :-

        ; === Start BASIC ===
        jsr init_basic
        ldx #<main_trampoline           ; New MAIN handler
        ldy #>main_trampoline
        jmp start_basic
.endproc

; -----------------------------------------------------------------------------
install_disk_vectors:
        ldy #VECTORS_SIZE - 1           ; Install new handlers

:       ldx old_vectors, y              ; Check if vector low byte has changed
        lda OLD_VECTOR_PAGE, x
        ldx vectors, y
        cmp VECTOR_PAGE, x
        bne @skip_vector

        ldx old_vectors, y              ; Check if vector high byte has changed
        lda OLD_VECTOR_PAGE + 1, x
        ldx vectors, y
        cmp VECTOR_PAGE + 1, X
        bne @skip_vector

        lda new_vectors, y              ; Install new vector
        sta VECTOR_PAGE, x
        lda #NEW_VECTOR_PAGE
        sta VECTOR_PAGE + 1, x
@skip_vector:
        dey
        bpl :-
        rts

; =============================================================================
disk_api:
.org KFF_RAM
tmp1:
        .byte $ff
tmp2:
        .byte $ff
kff_device_number:
        .byte $08

; -----------------------------------------------------------------------------
; Called by C64 kernal routines
; -----------------------------------------------------------------------------
main_trampoline:
        jsr enable_kff_rom
        jmp kff_main

normal_main_disable:
        jsr disable_kff_rom
normal_main:
        jmp MAIN                        ; Hardcoded for C64GS

; -----------------------------------------------------------------------------
open_trampoline:
	lda FA
        cmp kff_device_number
        bne normal_open                 ; Not KFF device

        lda SA                          ; Send secondary address (channel)
        sta KFF_DATA

        jsr store_filename_enable
        jmp kff_open

normal_open:
        old_open_vector = * + 1
        jmp $ffff

; -----------------------------------------------------------------------------
close_trampoline:
        sta tmp1
        jsr enable_kff_rom
        jmp kff_close

normal_close_disable:
        jsr disable_kff_rom
normal_close:
        old_close_vector = * + 1
        jmp $ffff

; -----------------------------------------------------------------------------
chkin_trampoline:
        jsr enable_kff_rom
        jmp kff_chkin

normal_chkin_disable:
        jsr disable_kff_rom
normal_chkin:
        old_chkin_vector = * + 1
        jmp $ffff

; -----------------------------------------------------------------------------
ckout_trampoline:
        jsr enable_kff_rom
        jmp kff_ckout

normal_ckout_disable:
        jsr disable_kff_rom
normal_ckout:
        old_ckout_vector = * + 1
        jmp $ffff

; -----------------------------------------------------------------------------
clrch_trampoline:
        jsr enable_kff_rom
        jmp kff_clrch

normal_clrch_disable:
        jsr disable_kff_rom
normal_clrch:
        old_clrch_vector = * + 1
        jmp $ffff

; -----------------------------------------------------------------------------
basin_trampoline:
        lda DFLTN
        cmp kff_device_number
        normal_basin_offset = * + 1
        bne do_kff_basin                ; will be replaced with normal_basin

do_kff_basin:
        jsr enable_kff_rom
        kff_basin_addr = * + 1
        jmp kff_basin_command           ; will be replaced with kff_basin

normal_basin_disable:
        jsr disable_kff_rom
normal_basin:
        old_basin_vector = * + 1
        jmp $ffff

; -----------------------------------------------------------------------------
bsout_trampoline:
        pha
        lda DFLTO
        cmp kff_device_number
        bne normal_bsout                ; Not KFF device

        jsr enable_kff_rom
        jmp kff_bsout

normal_bsout:
        pla
        old_bsout_vector = * + 1
        jmp $ffff

; -----------------------------------------------------------------------------
getin_trampoline:
        lda DFLTN
        cmp kff_device_number
        beq do_kff_basin                ; KFF device

normal_getin:
        old_getin_vector = * + 1
        jmp $ffff

; -----------------------------------------------------------------------------
clall_trampoline:
        jsr enable_kff_rom
        jmp kff_clall

old_clall_vector:
        .word $ffff

; -----------------------------------------------------------------------------
load_trampoline:
        sta VERCK
        lda FA
        cmp kff_device_number
        bne normal_load

        jsr store_filename_enable
        jmp kff_load

normal_load_disable:
        jsr disable_kff_rom
normal_load:
        lda VERCK
        old_load_vector = * + 1
        jmp $ffff

; -----------------------------------------------------------------------------
save_trampoline:
        lda FA
        cmp kff_device_number
        bne normal_save

        jsr store_filename_enable
        jmp kff_save

normal_save_disable:
        jsr disable_kff_rom
normal_save:
        old_save_vector = * + 1
        jmp $ffff

; -----------------------------------------------------------------------------
store_filename_enable:
        ldx FNLEN
        stx KFF_DATA                    ; Send filename Length
        beq enable_kff_rom

        ldy #$00
:       lda (FNADR),y                   ; Send filename
        sta KFF_DATA
        iny
        dex
        bne :-
; -----------------------------------------------------------------------------
enable_kff_rom:
        sei
        lda R6510                       ; Set memory configuration
        sta mem_config + 1
        ora #$07                        ; Kernal, BASIC, and I/O area visible
        sta R6510

        lda #KFF_ENABLE
        sta KFF_CONTROL
        rts

; -----------------------------------------------------------------------------
disable_kff_rom:
        pha
        lda #KFF_KILL
        sta KFF_CONTROL
mem_config:
        lda #$ff                        ; Restore mem config
        sta R6510
        pla
        cli
        rts

; -----------------------------------------------------------------------------
kernal_trampoline:                      ; Called by KFF routines
        lda #KFF_KILL
        sta KFF_CONTROL
kernal_call:
        jsr $ffff                       ; Address will be replaced
        sei
        ldy #KFF_ENABLE
        sty KFF_CONTROL
        rts

; -----------------------------------------------------------------------------
.if * > KFF_RAM + KFF_RAM_SIZE
        .error "Not enough space for disk_api"
.endif
.reloc
disk_api_end:

; =============================================================================
.proc kff_main
kff_main:
        jsr install_disk_vectors
        jmp normal_main_disable
.endproc

; =============================================================================
.proc kff_open
kff_open:
        lda FA                          ; Save device number
        sta tmp1
	lda #$00                        ; Set device to keyboard so that OPEN
	sta FA                          ; doesn't send commands to the serial bus

        sta STATUS                      ; Clear device status

        lda old_open_vector             ; Call old OPEN
        sta kernal_call + 1
        lda old_open_vector + 1
        sta kernal_call + 2
        jsr kernal_trampoline
        ldy tmp1                        ; Restore device number
	sty FA
        bcc @old_open_ok

        ldy #$00
        sty KFF_WRITE_LPTR              ; Clear KFF write buffer (filename)
        sty KFF_WRITE_HPTR
        bcs @open_done                  ; Open error

@old_open_ok:
        tya
        sta FAT,x                       ; Update table with real device number

        lda #REPLY_OPEN                 ; Send reply
        jsr kff_send_reply
        beq @open_ok
        cmp #CMD_WAIT_SYNC
        beq @wait_for_sync
        cmp #CMD_END_OF_FILE
        beq @end_of_file

@file_not_found:
        lda #ERROR_FILE_NOT_FOUND      ; File not found error
        sec
        bcs @open_done

@end_of_file:
        lda #STATUS_END_OF_FILE         ; Set status to end of file
        sta STATUS
@open_ok:
        lda #$00
        clc
@open_done:
        jmp disable_kff_rom

@wait_for_sync:
        jsr kff_save_ram_wait_sync
        beq @open_ok
        jmp @file_not_found
.endproc

; =============================================================================
.proc kff_close
kff_close:
        lda tmp1

        ldx LDTND                       ; Find logical file (in A)
:       dex
        bmi @normal_close               ; Not found
        cmp LAT,x
        bne :-

        lda FAT,x                       ; Lookup device
        cmp kff_device_number
        bne @normal_close               ; Not KFF device

        lda SAT,x                       ; Lookup secondary address
        sta SA
        ora #$e0                        ; Modify secondary address so that CLOSE
        sta SAT,x                       ; doesn't send commands to the serial bus

        lda SA                          ; Send secondary address (channel)
        sta KFF_DATA

        lda #REPLY_CLOSE                ; Send reply
        jsr kff_send_reply
        beq @normal_close

@wait_for_sync:
        jsr kff_save_ram_wait_sync
        bne @wait_for_sync

@normal_close:
        lda tmp1
        jmp normal_close_disable
.endproc

; =============================================================================
.proc kff_chkin
kff_chkin:
        stx tmp1
        txa                             ; Find logical file (in X)
        ldx LDTND
:       dex
        bmi @normal_chkin               ; Not found
        cmp LAT,x
        bne :-
        sta LA                          ; Store logical file

        lda FAT,x                       ; Lookup device
        cmp kff_device_number
        bne @normal_chkin               ; Not KFF device

        sta DFLTN                       ; Set input device
        sta FA                          ; Store device
        lda SAT,x                       ; Lookup secondary address
        sta SA
        sta KFF_DATA                    ; Send secondary address (channel)

        lda #REPLY_TALK                 ; Send reply
        jsr kff_send_reply

        lda #$00                        ; Clear device status
        sta STATUS
        clc                             ; OK
        jmp disable_kff_rom

@normal_chkin:
        ldx tmp1
        jmp normal_chkin_disable
.endproc

; =============================================================================
.proc kff_ckout
kff_ckout:
        stx tmp1
        txa                             ; Find logical file (in X)
        ldx LDTND
:       dex
        bmi @normal_ckout               ; Not found
        cmp LAT,x
        bne :-
        sta LA                          ; Store logical file

        lda FAT,x                       ; Lookup device
        cmp kff_device_number
        bne @normal_ckout               ; Not KFF device

        sta DFLTO                       ; Set output device
        sta FA                          ; Store device
        lda SAT,x                       ; Lookup secondary address
        sta SA
        sta KFF_DATA                    ; Send secondary address (channel)

        lda #REPLY_LISTEN               ; Send reply
        jsr kff_send_reply

        lda #$00                        ; Clear device status
        sta STATUS
        clc                             ; OK
        jmp disable_kff_rom

@normal_ckout:
        ldx tmp1
        jmp normal_ckout_disable
.endproc

; =============================================================================
.proc kff_clrch
kff_clrch:
        lda #$00                        ; Keyboard channel
        ldx kff_device_number
        cpx DFLTO                       ; Check output channel
        bne @check_input                ; Not KFF device
        sta DFLTO                       ; Don't send commands to the serial bus

        lda #REPLY_UNLISTEN             ; Send reply
        jsr kff_send_reply
        cmp #CMD_WAIT_SYNC
        bne @check_input_a

        sty tmp1
        jsr kff_save_ram_wait_sync
        ldy tmp1

@check_input_a:
        lda #$00                        ; Keyboard channel
@check_input:
        cpx DFLTN                       ; Check input channel
        bne @normal_clrch               ; Not KFF device
        sta DFLTN                       ; Don't send commands to the serial bus

        lda #REPLY_UNTALK               ; Send reply
        jsr kff_send_reply

@normal_clrch:
        jmp normal_clrch_disable
.endproc

; =============================================================================
.proc kff_basin_command
kff_basin_command:
        lda DFLTN
        bne @not_keyboard

        lda CRSW
        bne @normal_basin_disable       ; Still reading characters

        lda KFF_DATA
        beq @normal_basin               ; No start commands

@next_char:                             ; Print first command
        jsr BSOUT                       ; Print char
        inc INDX                        ; End of line++
        beq @normal_basin
        lda KFF_DATA
        bne @next_char

@end_of_line:
        sta $0292                       ; Insertion before current line
        sta PNTR                        ; Cursor at column 0
        sta QTSW                        ; Editor not in quote mode
        inc CRSW                        ; End of line (return)

        stx tmp1
        ldx #$00                        ; Put next command in keyboard buffer
@cmd_cpy:
        lda KFF_DATA
        beq @cmd_end
        sta KEYD, x
        inx
        bne @cmd_cpy
@cmd_end:
        stx NDX                         ; Length of buffer
        ldx tmp1

@normal_basin:
        lda #normal_basin - normal_basin_offset - 1
        sta normal_basin_offset         ; Enable KFF device check

        lda #<kff_basin                 ; Do not call kff_basin_command again
        sta kff_basin_addr
        lda #>kff_basin
        sta kff_basin_addr + 1

@normal_basin_disable:
        jmp normal_basin_disable

@not_keyboard:
        cmp kff_device_number
        bne @normal_basin_disable       ; Not KFF device
.endproc

; -----------------------------------------------------------------------------
.proc kff_basin
kff_basin:
        lda STATUS
        bne @status_not_ok

        lda #REPLY_SEND_BYTE            ; Send reply
        jsr kff_send_reply
        bne @check_read_error           ; Check command

@read_byte:
        lda KFF_DATA                    ; Get data
@read_done:
        clc
        jmp disable_kff_rom

@check_read_error:
        cmp #CMD_END_OF_FILE
        bne @read_error

        lda #STATUS_END_OF_FILE         ; Set status to end of file
        sta STATUS
        bne @read_byte

@read_error:
        lda #STATUS_READ_ERROR          ; Set device status to read error occurred
        sta STATUS
        lda #$00
        beq @read_done

@status_not_ok:
        lda #$0d
        bne @read_done                  ; Status not OK - return CR
.endproc

; =============================================================================
.proc kff_bsout
kff_bsout:
        pla
        sta tmp1
        sta KFF_DATA                    ; Send data

        lda #REPLY_RECEIVE_BYTE         ; Send reply
        jsr kff_send_reply
        beq @write_ok                   ; Check command
        cmp #CMD_WAIT_SYNC
        bne @write_error

        sty tmp2
        jsr kff_save_ram_wait_sync
        ldy tmp2
        jmp @write_ok

@write_error:
        lda #STATUS_SAVE_FILE_EXISTS    ; file already exists
        .byte BIT_INSTRUCTION           ; Skip next 2 bytes
@write_ok:
        lda #$00                        ; Clear status
        sta STATUS
        lda tmp1
        clc
        jmp disable_kff_rom
.endproc

; =============================================================================
.proc kff_clall
kff_clall:
        lda #$00                        ; Clear open file count
        sta LDTND

        jmp kff_clrch
.endproc

; =============================================================================
.proc kff_load
kff_load:
        lda FNLEN
        beq @no_filename                ; No filename, load will report error

        lda VERCK
        bne @not_found                  ; TODO: Support verify operation

        lda #REPLY_LOAD                 ; Send reply
        jsr kff_send_reply
        beq @load_ok

        cmp #CMD_NO_DRIVE
        beq @no_drive                   ; Try serial device (if any)

@not_found:
        lda #$00
        sta KFF_WRITE_LPTR              ; Clear KFF write buffer (filename)
        sta KFF_WRITE_HPTR
        jsr @print_searching

        lda #STATUS_READ_ERROR
        sta STATUS
        lda #ERROR_FILE_NOT_FOUND       ; File not found error
        sec
        jmp disable_kff_rom

@no_filename:
        sta KFF_WRITE_LPTR              ; Clear KFF write buffer (filename)
        sta KFF_WRITE_HPTR
@no_drive:
        jmp normal_load_disable

@print_searching:
        lda #<LUKING                   ; Setup call to print "Searching for"
        sta kernal_call + 1
        lda #>LUKING
        sta kernal_call + 2
        jmp kernal_trampoline

@load_ok:
        lda #STATUS_END_OF_FILE         ; Clear device status
        sta STATUS
        jsr @print_searching

        lda #<LODING                    ; Setup call to print "Loading"
        sta kernal_call + 1
        lda #>LODING
        sta kernal_call + 2
        jsr kernal_trampoline

        lda KFF_DATA                    ; Get PRG size
        sta tmp1
        lda KFF_DATA
        sta tmp2

        lda KFF_DATA                    ; Get load address
        sta EAL
        lda KFF_DATA
        sta EAH

        lda SA
        bne @load_start                 ; Secondary address set

        lda MEMUSS                      ; Set start address
        sta EAL
        lda MEMUSS + 1
        sta EAH

@load_start:
        ldy #$00
        ldx tmp2
        beq @load_rest
        bne @new_page

@load_256_bytes:                        ; Receive a page
        lda KFF_DATA
        sta (EAL),y
        iny
        lda KFF_DATA
        sta (EAL),y
        iny
        bne @load_256_bytes
@end_page:
        inc EAH
        dex
        beq @load_rest
@new_page:
        lda EAH                         ; Check for load to I/O area
        cmp #$cf
        bcc @load_256_bytes
        cmp #$e0
        bcs @load_256_bytes

        stx tmp2
        ldx #$33
@load_256_bytes_io:
        lda KFF_DATA                    ; Receive a page
        stx R6510                       ; Hide I/O area
        sta (EAL),y
        lda #$37
        sta R6510                       ; Show I/O area
        iny
        bne @load_256_bytes_io

        ldx tmp2
        bne @end_page
@load_rest:
        ldx tmp1
        beq @load_end
        ldx #$33
@load_bytes:
        lda KFF_DATA                    ; Receive the remaining bytes
        stx R6510                       ; Hide I/O area
        sta (EAL),y
        lda #$37
        sta R6510                       ; Show I/O area
        iny
        dec tmp1
        bne @load_bytes
@load_end:
        sty tmp1
        jsr install_disk_vectors        ; Reinstall vectors if changed

        lda tmp1
        clc                             ; Adjust end address
        adc EAL
        sta EAL
        tax
        lda EAH
        adc #$00
        sta EAH
        tay

        clc
        lda $dc0d                       ; Clear timer interrupt flags
        jmp disable_kff_rom
.endproc

; =============================================================================
.proc kff_save
kff_save:
        lda FNLEN
        bne @send_reply                 ; Has filename

        sta KFF_WRITE_LPTR              ; Clear KFF write buffer (filename)
        sta KFF_WRITE_HPTR
        jmp normal_save_disable         ; No filename, save will report error

@send_reply:
        jsr kff_save_ram                ; Send KFF_SAVE_RAM for later retrieval
        lda #REPLY_SAVE                 ; Send reply
        jsr kff_send_reply
        beq @save_ok

        cmp #CMD_NO_DRIVE
        beq @no_drive                   ; Try serial device (if any)

@exists:
        jsr @print_saving

        lda #STATUS_SAVE_FILE_EXISTS    ; file already exists (or dir is full)
        sta STATUS
        clc
        jmp disable_kff_rom

@no_drive:
        jmp normal_save_disable

@print_saving:
        lda #<SAVING                    ; Setup call to print "Saving"
        sta kernal_call + 1
        lda #>SAVING
        sta kernal_call + 2
        jmp kernal_trampoline

@save_ok:
        jsr @print_saving

        sec                             ; Calculate file size
        lda EAL                         ; EAL:EAH is the end address
        sbc STAL                        ; STAL:STAH is the start address
        sta tmp1
        sta KFF_DATA                    ; Send low byte of file size

        lda EAH
        sbc STAH
        sta tmp2
        sta KFF_DATA                    ; Send high byte of file size

        lda STAL                        ; Initializing SAL:SAH
        sta SAL
        sta KFF_DATA                    ; Send low byte of start address

        lda STAH
        sta SAH
        sta KFF_DATA                    ; Send high byte of start address

        ; tmp1:tmp2 -> remaining size of file to save
        ; SAL:SAH   -> running address of file to save

@save_start:
        ldy #$00
        ldx tmp2
        beq @save_rest
        bne @new_page

@save_256_bytes:                        ; Send a page of data
        lda (SAL),y
        sta KFF_DATA
        iny
        bne @save_256_bytes
@end_page:
        inc SAH
        dex
        beq @save_rest
@new_page:
        lda SAH                         ; Check for save from ROM area
        cmp #$7f
        bcc @save_256_bytes
        stx tmp2

        ldy #KFF_SAVE_PAGES_SIZE - 1    ; Copy save page prg to KFF_SAVE_RAM
:       lda kff_save_pages_prg, y
        sta KFF_SAVE_RAM, y
        dey
        bpl :-

        ldy #$00
        ldx #$37                        ; Setup for show ROM and I/O
        jsr save_pages

@save_rest:
        ldx tmp1
        beq @save_end

        ldy #KFF_SAVE_BYTES_SIZE - 1    ; Copy save bytes prg to KFF_SAVE_RAM
:       lda kff_save_bytes_prg, y
        sta KFF_SAVE_RAM, y
        dey
        bpl :-

        ldy #$00
        ldx #$37                        ; Setup for show ROM and I/O
        jsr save_bytes

@save_end:
        tya
        clc                             ; Adjust end address
        adc SAL
        sta SAL
        lda SAH
        adc #$00
        sta SAH

        jsr kff_wait_for_sync
        beq @save_no_error

        lda #STATUS_SAVE_DISK_FULL      ; Set status
        .byte BIT_INSTRUCTION           ; Skip next 2 bytes
@save_no_error:
        lda #$00
        sta STATUS
        clc
        lda $dc0d                       ; Clear timer interrupt flags
        jmp disable_kff_rom
.endproc

; =============================================================================
.proc kff_send_reply
.export kff_send_reply
kff_send_reply:
        sta KFF_COMMAND                 ; Send reply

@wait:  cmp KFF_COMMAND
        beq @wait                       ; Wait for next command

        lda KFF_COMMAND                 ; Get command
        rts
.endproc

; =============================================================================
.proc kff_save_ram_wait_sync
kff_save_ram_wait_sync:
        jsr kff_save_ram
        jmp kff_wait_for_sync
.endproc

; =============================================================================
.proc kff_save_ram
kff_save_ram:
        ldy #KFF_SAVE_RAM_SIZE          ; Send KFF_SAVE_RAM
        sty KFF_DATA
        dey
:       lda KFF_SAVE_RAM, y
        sta KFF_DATA
        dey
        bpl :-
        rts
.endproc

; =============================================================================
.proc kff_wait_for_sync
kff_wait_for_sync:
        ldy #KFF_SYNC_RAM_SIZE - 1      ; Copy sync prg to KFF_SAVE_RAM
:       lda kff_sync_prg, y
        sta KFF_SAVE_RAM, y
        dey
        bpl :-

        lda #REPLY_OK
        jsr wait_for_sync
        clc

        ldy KFF_DATA                    ; Restore KFF_SAVE_RAM
        dey
:       lda KFF_DATA
        sta KFF_SAVE_RAM, y
        dey
        bpl :-

        lda #REPLY_OK                   ; Get result
        jmp kff_send_reply
.endproc

; =============================================================================
kff_sync_prg:
.org KFF_SAVE_RAM
.proc wait_for_sync
wait_for_sync:
        sta KFF_COMMAND                 ; Send reply

@sync:  lda #CMD_SYNC                   ; Wait for sync command
@wait:  cmp KFF_COMMAND
        bne @wait

@next:  sta KFF_RAM_TST                 ; Check that KFF RAM is available
        cmp KFF_RAM_TST
        bne @sync
        asl
        bne @next
        rts
.endproc
.reloc
kff_sync_prg_end:

KFF_SYNC_RAM_SIZE = kff_sync_prg_end - kff_sync_prg

.if KFF_SYNC_RAM_SIZE > KFF_SAVE_RAM_SIZE
    .error "Not enough space for kff_sync_prg"
.endif

; -----------------------------------------------------------------------------
kff_save_pages_prg:
.org KFF_SAVE_RAM
.proc save_pages                        ; Send a page of data
        lda #$35
        sta R6510                       ; Hide ROM and I/O
        lda (SAL),y
        stx R6510                       ; Show ROM and I/O
        sta KFF_DATA
        iny
        bne save_pages

        inc SAH
        dec tmp2
        bne save_pages
        rts
.endproc
.reloc
kff_save_pages_prg_end:

KFF_SAVE_PAGES_SIZE = kff_save_pages_prg_end - kff_save_pages_prg

.if KFF_SAVE_PAGES_SIZE > KFF_SAVE_RAM_SIZE
    .error "Not enough space for kff_save_pages_prg"
.endif

; -----------------------------------------------------------------------------
kff_save_bytes_prg:
.org KFF_SAVE_RAM
.proc save_bytes                        ; Send the remaining bytes
        lda #$35
        sta R6510                       ; Hide ROM and I/O
        lda (SAL),y
        stx R6510                       ; Show ROM and I/O
        sta KFF_DATA
        iny
        dec tmp1
        bne save_bytes
        rts
.endproc
.reloc
kff_save_bytes_prg_end:

KFF_SAVE_BYTES_SIZE = kff_save_bytes_prg_end - kff_save_bytes_prg

.if KFF_SAVE_BYTES_SIZE > KFF_SAVE_RAM_SIZE
    .error "Not enough space for kff_save_bytes_prg"
.endif
