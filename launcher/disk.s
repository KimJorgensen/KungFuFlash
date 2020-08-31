;
; Copyright (c) 2019-2020 Kim Jørgensen
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
.import init_system_constants_light

.include "ef3usb_macros.s"

; Kernal functions
LUKING          = $f5af                 ; Print "SEARCHING"
LODING          = $f5d2                 ; Print "LOADING"
SAVING          = $f68f                 ; Print "SAVING"
CINT	        = $ff81                 ; Initialize screen editor
RESTOR          = $ff8a                 ; Restore I/O vectors
BSOUT           = $ffd2                 ; Write byte to default output

; Kernal vectors
NMINV           = $0318
IOPEN           = $031a
ICLOSE          = $031c
ICHKIN          = $031e
ICLRCH          = $0322
IBASIN          = $0324
IGETIN          = $032a
ICLALL          = $032c
ILOAD           = $0330
ISAVE           = $0332

; Kernal tables
LAT             = $0259                 ; Logical file table
FAT             = $0263                 ; Device number table
SAT             = $026d                 ; Secondary address table

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
INDX            = $c8                   ; Length of line
CRSW            = $d0                   ; Input/get flag
PNTR            = $d3                   ; Cursor column
QTSW            = $d4                   ; Quotation mode flag

; Used ZP
ptr1            = $fb
ptr2            = $fc
tmp1            = $fd
tmp2            = $fe
tmp3            = $ff

trampoline      = $0100
ef3usb_send_receive = $0100
start_commands  = $8000

EASYFLASH_BANK          = $de00
EASYFLASH_CONTROL       = $de02
EASYFLASH_RAM           = $df00
EASYFLASH_KILL          = $04
EASYFLASH_16K           = $87

; Align with commands.h
CMD_LOAD                = $80
CMD_LOAD_NEXT_BANK      = $81
CMD_OPEN                = $82
CMD_CLOSE               = $83
CMD_TALK                = $84
CMD_UNTALK              = $85
CMD_GET_CHAR            = $86
CMD_SAVE                = $87
CMD_SAVE_BUFFER         = $88

REPLY_READ_DONE         = $80
REPLY_READ_BANK         = $81
REPLY_NOT_FOUND         = $82
REPLY_READ_ERROR        = $83
REPLY_END_OF_FILE       = $84
REPLY_NOT_SUPPORTED     = $85
REPLY_SAVE_OK           = $86
REPLY_SAVE_ERROR        = $87

; Align with commands.h (SAVE_BUFFER_OFFSET = $0100 - SAVE_BUF_SIZE)
SAVE_BUF_SIZE           = $90
SAVE_ERROR_FILE_EXISTS  = $80
SAVE_ERROR_DISK_FULL    = $83

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
        stx $8004                       ; Trash autostart (if any)

        jsr init_system_constants_light
        jsr RESTOR                      ; Restore Kernal Vectors

        lda #$00                        ; Clear start of BASIC area
        sta $0800
        sta $0801
        sta $0802

        ; === Disk API in EF RAM ===
        ldx #disk_api_resident_end - disk_api_resident
:       dex
        lda disk_api_resident,x
        sta EASYFLASH_RAM,x
        cpx #$00
        bne :-

        jsr copy_disable_ef_rom

        ; === Start BASIC ===
        ldx #basic_mount_starter_end - basic_mount_starter
:       lda basic_mount_starter,x
        sta trampoline,x
        dex
        bpl :-

        lda IOPEN                       ; Store old IOPEN vector
        sta old_open_vector
        lda IOPEN + 1
        sta old_open_vector + 1

        lda ICLOSE                      ; Store old ICLOSE vector
        sta old_close_vector
        lda ICLOSE + 1
        sta old_close_vector + 1

        lda ICHKIN                      ; Store old ICHKIN vector
        sta old_chkin_vector
        lda ICHKIN + 1
        sta old_chkin_vector + 1

        lda ICLRCH                      ; Store old ICLRCH vector
        sta old_clrch_vector
        lda ICLRCH + 1
        sta old_clrch_vector + 1

        lda IBASIN                      ; Store old BASIN vector
        sta old_basin_vector
        lda IBASIN + 1
        sta old_basin_vector + 1

        lda IGETIN                      ; Store old GETIN vector
        sta old_getin_vector
        lda IGETIN + 1
        sta old_getin_vector + 1

        lda ICLALL                      ; Store old CLALL vector
        sta old_clall_vector
        lda ICLALL + 1
        sta old_clall_vector + 1

        lda ILOAD                       ; Store old LOAD vector
        sta old_load_vector
        lda ILOAD + 1
        sta old_load_vector + 1

        lda ISAVE                       ; Store old SAVE vector
        sta old_save_vector
        lda ISAVE + 1
        sta old_save_vector + 1


        jsr install_disk_vectors

        lda #EASYFLASH_KILL
        jmp trampoline
.endproc

; =============================================================================
install_disk_vectors:
        lda #<open_trampoline           ; Install new OPEN handler
        sta IOPEN
        lda #>open_trampoline
        sta IOPEN + 1

        lda #<close_trampoline          ; Install new CLOSE handler
        sta ICLOSE
        lda #>close_trampoline
        sta ICLOSE + 1

        lda #<chkin_trampoline          ; Install new CHKIN handler
        sta ICHKIN
        lda #>chkin_trampoline
        sta ICHKIN + 1

        lda #<clrch_trampoline          ; Install new CLRCH handler
        sta ICLRCH
        lda #>clrch_trampoline
        sta ICLRCH + 1

        lda #<basin_trampoline          ; Install new BASIN handler
        sta IBASIN
        lda #>basin_trampoline
        sta IBASIN + 1

        lda #<getin_trampoline          ; Install new GETIN handler
        sta IGETIN
        lda #>getin_trampoline
        sta IGETIN + 1

        lda #<clall_trampoline          ; Install new CLALL handler
        sta ICLALL
        lda #>clall_trampoline
        sta ICLALL + 1

        lda #<load_trampoline           ; Install new LOAD handler
        sta ILOAD
        lda #>load_trampoline
        sta ILOAD + 1

        lda #<save_trampoline           ; Install new SAVE handler
        sta ISAVE
        lda #>save_trampoline
        sta ISAVE + 1

        rts

; =============================================================================
basic_mount_starter:
.org trampoline
        sta EASYFLASH_CONTROL
        jsr CINT                        ; Initialize screen editor
        cli
        jmp ($a000)                     ; Start BASIC
.reloc
basic_mount_starter_end:

ef3usb_send_receive_rom_addr:
; Make sure a wait_usb_tx_ok macro inserted before calling
; Not included here to keep stack usage low
.org ef3usb_send_receive
        sta USB_DATA
@ef3usb_wait4done:
        jsr @ef3usb_receive_in_stack
@ef3usb_checkdone:
        cmp #'d'
        bne @ef3usb_wait4done

        jsr @ef3usb_receive_in_stack
        cmp #'o'
        bne @ef3usb_checkdone

        jsr @ef3usb_receive_in_stack
        cmp #'n'
        bne @ef3usb_checkdone

        jsr @ef3usb_receive_in_stack
        cmp #'e'
        bne @ef3usb_checkdone

@ef3usb_receive_in_stack:
        wait_usb_rx_ok
        lda USB_DATA
        rts
.reloc
ef3usb_send_receive_rom_addr_end:

; =============================================================================
disk_api_resident:
.org EASYFLASH_RAM
zp_backup:
        .byte $ff,$ff,$ff,$ff,$ff
filename_len:
        .byte $00
; TODO: still, only 39 bytes are allocated for filenames. It should be 41 chars long.
filename:
        .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
        .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
        .byte $00,$00,$00,$00,$00,$00,$00

filename_end:

mem_config:
        .byte $00
print_text_ptr:
        .byte $00
more_to_load:
        .byte $00

old_open_vector:
        .byte $00,$00
old_close_vector:
        .byte $00,$00
old_chkin_vector:
        .byte $00,$00
old_clrch_vector:
        .byte $00,$00
old_basin_vector:
        .byte $00,$00
old_getin_vector:
        .byte $00,$00
old_clall_vector:
        .byte $00,$00
old_load_vector:
        .byte $00,$00
old_save_vector:
         .byte $00,$00

kernal_trampoline:                      ; Called by KFF routines 
        lda #EASYFLASH_KILL
        sta EASYFLASH_CONTROL
kernal_call:
        jsr $ffff                       ; Address will be replaced
        ldy #EASYFLASH_16K
        sty EASYFLASH_CONTROL
        rts

; Overwritten only by save, otherwise stays in EF_RAM
io_trampoline_start:
open_trampoline:                        ; Called by C64 kernal routines
        jsr store_filename_enable_ef
        jmp kff_open

close_trampoline:
        jsr enable_ef_rom
        jmp kff_close

chkin_trampoline:
        jsr enable_ef_rom
        jmp kff_chkin

clrch_trampoline:
        jsr enable_ef_rom
        jmp kff_clrch

basin_trampoline:
        jsr enable_ef_rom
        jmp kff_basin

getin_trampoline:
        jsr enable_ef_rom
        jmp kff_getin

clall_trampoline:
        jsr enable_ef_rom
        jmp kff_clall

load_trampoline:
        jsr store_filename_enable_ef
        jmp kff_load

save_trampoline:
        jsr store_filename_enable_ef
        jmp kff_save

store_filename_enable_ef:
        ldy FNLEN
        cpy #(filename_end - filename) + 1
        bcs @too_long                   ; Filename too long
        .byte $2c                       ; Dummy instruction (skip next 2 bytes)
@too_long:
        ldy #filename_end - filename    ; Limit to max length
        sty filename_len

:       lda (FNADR),y                   ; Store current file name
        sta filename,y
        dey
        bpl :-

enable_ef_rom:
        pha
        sei
        lda R6510                       ; Set memory configuration
        sta mem_config
        ora #$07
        sta R6510

        txa                             ; Save ZP
        pha
        ldx #tmp3 - ptr1
:       lda ptr1,x
        sta zp_backup,x
        dex
        bpl :-
        pla
        tax

        lda #EASYFLASH_16K
        sta EASYFLASH_CONTROL
        pla
        rts

enable_ef_rom_end:
.reloc
disk_api_resident_end:

disk_api_load_prg:
.org enable_ef_rom_end
load_prg:
        sta EASYFLASH_BANK
        ldx tmp2
        beq copy_rest

copy_256_bytes:
        lda (ptr1),y
        sta (EAL),y                     ; Store loaded byte
        iny
        bne copy_256_bytes

        inc EAH
        inc ptr2
        dex
        bne copy_256_bytes

copy_rest:
        ldx tmp1
        beq load_done

copy_bytes:
        lda (ptr1),y
        sta (EAL),y                     ; Store loaded byte
        iny
        dex
        bne copy_bytes

        clc
        lda EAL
        adc tmp1
        sta EAL
        bcc load_done
        inc EAH

load_done:
        ldx EAL
        ldy EAH
        clc
        lda #$00                        ; Set EF bank
        sta EASYFLASH_BANK
        lda more_to_load
        beq all_done
        jmp load_next_bank

all_done:
        rts
.reloc
disk_api_load_prg_end:

disk_api_disable_ef_rom:
; Overwritten only by load and save, otherwise stays in EF_RAM
.org enable_ef_rom_end
disable_ef_rom:
        php                             ; Restore ZP
        pha
        txa
        pha
        ldx #tmp3 - ptr1
:       lda zp_backup,x
        sta ptr1,x
        dex
        bpl :-
        pla
        tax

        lda #EASYFLASH_KILL
        sta EASYFLASH_CONTROL

        lda mem_config                        ; Restore mem config
        sta R6510
        pla
        plp
        cli
return_inst:
        rts                         ; Replaced with jmp or rts
       .byte $00,$00          
.reloc
disk_api_disable_ef_rom_end:

disk_api_save_prg:
; This area is copied over the io_trampoline code to have bigger 
; IO buffer for data transfer between C64 and KFF card.
.org io_trampoline_start
fill_save_buffer:

        lda #EASYFLASH_KILL                   ; Area under $8000-$9fff will be RAM
        sta EASYFLASH_CONTROL
        lda mem_config                        ; Restore mem config
        sta R6510

:       dey
        lda (SAL),y
        sta (ptr1),y
        cpy #$00
        bne :-

        lda #EASYFLASH_16K
        sta EASYFLASH_CONTROL
        lda mem_config
        ora #$07
        sta R6510
        rts

.reloc
disk_api_save_prg_end:


; =============================================================================

.proc copy_load_prg
copy_load_prg:
        php     ; store flags
        pha     ; store a
        txa     ; store x
        pha
        ldx #disk_api_load_prg_end - disk_api_load_prg
:       dex
        lda disk_api_load_prg,x
        sta load_prg,x
        cpx #$00
        bne :-
        pla     ; pop x from stack
        tax
        pla     ; pop a
        plp     ; pop flags
        rts
.endproc

.proc copy_save_prg
copy_save_prg:
        ldx #disk_api_save_prg_end - disk_api_save_prg
:       dex
        lda disk_api_save_prg,x
        sta fill_save_buffer,x
        cpx #$00
        bne :-
        rts
.endproc

.proc copy_disable_ef_rom
copy_disable_ef_rom:
        php     ; flags
        pha     ; a
        txa     ; x
        pha
        ldx #disk_api_disable_ef_rom_end - disk_api_disable_ef_rom
:       dex
        lda disk_api_disable_ef_rom,x
        sta disable_ef_rom,x
        cpx #$00
        bne :-
        pla     ; x
        tax
        pla     ; a
        plp     ; flags
        rts
.endproc

.proc copy_trampoline_code
copy_trampoline_code:
        php     ; flags
        pha     ; a
        txa     ; x
        pha
        ldx #enable_ef_rom_end - io_trampoline_start
:       dex
        lda disk_api_resident + (io_trampoline_start - EASYFLASH_RAM),x
        sta io_trampoline_start,x
        cpx #$00
        bne :-
        pla     ; x
        tax
        pla     ; a
        plp     ; flags
        rts
.endproc

.proc copy_ef3usb_send_receive_code
copy_ef3usb_send_receive_code:
        ldx #ef3usb_send_receive_rom_addr_end-ef3usb_send_receive_rom_addr
:       dex        
        lda ef3usb_send_receive_rom_addr,X
        sta ef3usb_send_receive,X
        cpx #$00
        bne :-
        rts
.endproc

.proc kff_open
kff_open:
	lda FA
        cmp #$08
        beq @kff_device                 ; Device 8

@normal_open:
        lda #$4c                        ; Setup jmp to normal OPEN
        sta return_inst
        lda old_open_vector
        sta return_inst + 1
        lda old_open_vector + 1
        sta return_inst + 2
        jmp disable_ef_rom

@kff_device:
        sta tmp1                        ; Save device number

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
        bcs @open_done                  ; Open error

        tya
        sta FAT,x                       ; Update table with real device number

        lda #CMD_OPEN                   ; Send command
        jsr kff_send_command
        lda SA                          ; Send secondary address (channel)
        jsr ef3usb_send_byte
        jsr send_filename

        jsr ef3usb_receive_byte         ; Get reply
        cmp #REPLY_READ_DONE
        beq @open_ok
        cmp #REPLY_END_OF_FILE
        beq @end_of_file

        lda #$04                        ; File not found error
        sec
        bcs @open_done

@end_of_file:
        lda #$40                        ; Set status to end of file
        sta STATUS
@open_ok:
        lda #$00
        clc
@open_done:
        ldy #$60
        sty return_inst
        ldy FA
        jmp disable_ef_rom

.endproc

; =============================================================================
.proc send_filename
send_filename:
        lda filename_len
        jsr ef3usb_send_byte
        tax
        beq @done

        lda #<filename
        sta ptr1
        lda #>filename
        sta ptr1 + 1
        jmp ef3usb_send_bytes
@done:
        rts
.endproc

; =============================================================================
.proc kff_close
kff_close:
        pha                             ; Find logical file (in A)
        ldx LDTND
:       dex
        bmi @normal_close               ; Not found
        cmp LAT,x
        bne :-

        lda FAT,x                       ; Lookup device
        cmp #$08
        bne @normal_close               ; Not device 8

        lda SAT,x                       ; Lookup secondary address
        sta SA
        lda #$00                        ; Clear secondary address so that CLOSE
        sta SAT,x                       ; doesn't send commands to the serial bus

        lda #CMD_CLOSE                  ; Send command
        jsr kff_send_command
        lda SA                          ; Send secondary address (channel)
        jsr ef3usb_send_byte
        jsr ef3usb_receive_byte         ; Get reply (ignored)

@normal_close:
        lda #$4c                        ; Setup jmp to old CLOSE
        sta return_inst
        lda old_close_vector
        sta return_inst + 1
        lda old_close_vector + 1
        sta return_inst + 2

        pla
        jmp disable_ef_rom
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
        cmp #$08
        bne @normal_chkin               ; Not device 8

        sta FA                          ; Store device
        sta DFLTN
        lda SAT,x                       ; Lookup secondary address
        sta SA

        lda #CMD_TALK                   ; Send command
        jsr kff_send_command
        lda SA                          ; Send secondary address (channel)
        jsr ef3usb_send_byte
        jsr ef3usb_receive_byte         ; Get reply (ignored)

        lda #$00                        ; Clear device status
        sta STATUS
        clc                             ; OK
        stx tmp1
        ldx #$60                        ; Setup rts from CHKIN
        stx return_inst
        ldx tmp1
        jmp disable_ef_rom

@normal_chkin:
        lda #$4c                        ; Setup jmp to old CHKIN
        sta return_inst
        lda old_chkin_vector
        sta return_inst + 1
        lda old_chkin_vector + 1
        sta return_inst + 2

        ldx tmp1
        jmp disable_ef_rom
.endproc

; =============================================================================
.proc kff_clrch
kff_clrch:
        lda #$00                        ; Keyboard channel

        ldx #$08
        cpx DFLTO                       ; Check output channel
        bne @check_input                ; Not device 8
        sta DFLTO                       ; Don't send commands to the serial bus

@check_input:
        cpx DFLTN                       ; Check input channel
        bne @normal_clrch               ; Not device 8
        sta DFLTN                       ; Don't send commands to the serial bus

        sty tmp1
        lda #CMD_UNTALK                 ; Send command
        jsr kff_send_command
        jsr ef3usb_receive_byte         ; Get reply (ignored)
        ldy tmp1

@normal_clrch:
        lda #$4c                        ; Setup jmp to old CLRCH
        sta return_inst
        lda old_clrch_vector
        sta return_inst + 1
        lda old_clrch_vector + 1
        sta return_inst + 2

        jmp disable_ef_rom
.endproc

; =============================================================================
.proc kff_basin
kff_basin:
        lda DFLTN
        beq keyboard                    ; Is keyboard

        cmp #$08
        bne normal_basin                ; Not device 8

do_kff_basin:
        stx tmp1
        sty tmp2
        ldx #$60                        ; set RTS at the end of disable_ef_rom
        stx return_inst
        lda #CMD_GET_CHAR               ; Send command
        jsr kff_send_command
        ldx tmp1
        ldy tmp2
        jsr ef3usb_receive_byte         ; Get reply
        cmp #REPLY_READ_DONE
        beq @read_ok
        cmp #REPLY_END_OF_FILE
        beq @read_end

        lda #$10                        ; Set device status to read error occurred
        sta STATUS
        lda #$00
        clc
        jmp disable_ef_rom

@read_end:
        lda #$40                        ; Set status to end of file
        .byte $2c                       ; Skip next 2 bytes
@read_ok:
        lda #$00                        ; Clear status
        sta STATUS
        jsr ef3usb_receive_byte         ; Get data
        clc
        jmp disable_ef_rom

keyboard:
        lda CRSW
        bne normal_basin                ; Still reading characters

        sty tmp1
        ldy print_text_ptr
        lda start_commands,y
        beq @normal_basin_y             ; No more lines to print

@next_char:
        jsr BSOUT                       ; Print char
        inc INDX                        ; End of line++
        iny
        beq @end_of_line
        lda start_commands,y
        bne @next_char

@end_of_line:
        sta $0292                       ; Insertion before current line
        sta PNTR                        ; Cursor at column 0
        sta QTSW                        ; Editor not in quote mode
        inc CRSW                        ; End of line (return)
        iny
        sty print_text_ptr

@normal_basin_y:
        ldy tmp1
normal_basin:
        lda #$4c                        ; Setup jmp to old BASIN
        sta return_inst
        lda old_basin_vector
        sta return_inst + 1
        lda old_basin_vector + 1
        sta return_inst + 2

        jmp disable_ef_rom
.endproc

; =============================================================================
.proc kff_getin
kff_getin:
        lda DFLTN
        cmp #$08
        bne @normal_getin               ; Not device 8

@kff_device:
        jmp kff_basin::do_kff_basin

@normal_getin:
        lda #$4c                        ; Setup jmp to old GETIN
        sta return_inst
        lda old_getin_vector
        sta return_inst + 1
        lda old_getin_vector + 1
        sta return_inst + 2

        jmp disable_ef_rom
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
        lda VERCK                       ; TODO FIXME: verify with KFF storage if possible
        bne @normal_load                ; Verify operation (not load)

        lda FA
        cmp #$08
        beq @kff_device                 ; Device 8

@normal_load:
        lda #$4c                        ; Setup jmp to normal load
        sta return_inst
        lda old_load_vector
        sta return_inst + 1
        lda old_load_vector + 1
        sta return_inst + 2
        lda VERCK
        ldx MEMUSS
        ldy MEMUSS + 1
        jmp disable_ef_rom

@kff_device:
        jsr search_for_file
        bcs @normal_load                ; Not found

        jsr copy_load_prg

        lda #$00                        ; Clear device status
        sta STATUS

        lda #<LUKING                   ; Setup call to print "Searching for"
        sta kernal_call + 1
        lda #>LUKING
        sta kernal_call + 2
        jsr kernal_trampoline

        lda #<LODING                    ; Setup call to print "Loading"
        sta kernal_call + 1
        lda #>LODING
        sta kernal_call + 2
        jsr kernal_trampoline

        jsr ef3usb_receive_2_bytes      ; Get PRG size
        sta tmp1                        ; actually min(bank#1 size, remaining bytes to load)
        stx tmp2

        jsr ef3usb_receive_2_bytes      ; Get load address
        sta EAL                         ; Set start address
        stx EAH

        lda SA
        bne start_load                  ; Secondary address set

        lda MEMUSS                      ; Set start address
        sta EAL
        lda MEMUSS + 1
        sta EAH

start_load:
        jsr init_load_params
        jsr load_prg
        jsr copy_disable_ef_rom
        jmp disable_ef_rom

init_load_params:
        lda #$00                        ; ptr = Start of EF flash
        sta ptr1
        lda #$80
        sta ptr2

        ldy #$00                        ; Index
        lda #$01                        ; EF Bank
        rts

search_for_file:
        ldy FNLEN
        beq file_not_found              ; Filename length = 0

        lda #CMD_LOAD                   ; Send command
        jsr kff_send_command
        jsr send_filename

get_reply:
        jsr ef3usb_receive_byte         ; Get reply
        ldx #$00
        cmp #REPLY_READ_DONE
        beq @file_found
        cmp #REPLY_READ_BANK
        bne file_not_found

        inx
@file_found:
        stx more_to_load
        clc
        rts

file_not_found:
        sec
        rts

load_next_bank:
        lda #CMD_LOAD_NEXT_BANK         ; Send command
        jsr kff_send_command
        jsr get_reply
        bcs @read_error

        jsr ef3usb_receive_2_bytes      ; Get bank size
        sta tmp1
        stx tmp2
        jsr init_load_params
        jmp load_prg                    ; let's not pollute the stack...

@read_error:
        lda #$10                        ; Set device status to read error occurred
        sta STATUS
        jsr copy_disable_ef_rom
        jmp disable_ef_rom
.endproc

; "Export" function
load_next_bank = kff_load::load_next_bank

; =============================================================================
.proc kff_save
kff_save:
        lda FA
        cmp #$08
        beq @kff_device                 ; Device 8

@normal_save:
        lda #$4c                        ; Setup jmp to normal save
        sta return_inst
        lda old_save_vector
        sta return_inst + 1
        lda old_save_vector + 1
        sta return_inst + 2
        jmp disable_ef_rom

@kff_device:
        lda #<SAVING                   ; Setup call to print "Saving <filename>"
        sta kernal_call + 1
        lda #>SAVING
        sta kernal_call + 2
        jsr kernal_trampoline

        jsr copy_ef3usb_send_receive_code
        jsr copy_save_prg

        lda #CMD_SAVE                   ; Send command
        jsr kff_send_command
        jsr send_filename


; calculate file size - number of bytes to send
        sec
        lda EAL                 ; EAL:EAH is the first address after the program (points after the prg)                 
        sbc STAL                ; STAL:STAH is the first valid address
        sta tmp1
        lda EAH
        sbc STAH
        sta tmp2

        lda STAL                        ; initializing SAL:SAH
        sta SAL
        jsr ef3usb_send_byte            ; send low byte of start address

        lda STAH
        sta SAH
        jsr @ef3usb_send_receive
        ldx #SAVE_ERROR_FILE_EXISTS     ; Most probably the file already exists (or directory is full)
        cmp #REPLY_SAVE_OK
        bne @save_done

        lda #CMD_SAVE_BUFFER            ; Here starts the save buffer cycle
        jsr kff_send_command

        lda #($0100 - SAVE_BUF_SIZE)    ; set ptr to EF RAM
        sta ptr1
        lda #>EASYFLASH_RAM
        sta ptr2

; tmp1:tmp2 -> remaining size of file to save (here: size of file)
; ptr1:ptr2 -> start address of outbut buff in EF RAM
; SAL:SAH   -> running address of file to save (here: start address of file) 
@save_start:
        jsr @compare_buffer_and_remaining_size   ; c set if remaining size >= SAVE_BUF_SIZE
        bcc @set_remaining_size
        lda #SAVE_BUF_SIZE
        .byte $2c                       ; BIT
@set_remaining_size:
        lda tmp1                        ; valid bytes in buffer
        tax                             ; save A in X
        tay
        beq @save_cycle_done            ; let's not copy $100 bytes...
        jsr fill_save_buffer            ; y->input, num of bytes to copy to save buffer
        txa                             ; let's restore A from X
@save_cycle_done:
        sta tmp3                        ; store last num of sent bytes (0 if we're done)
        jsr @ef3usb_send_receive
        ldx #SAVE_ERROR_DISK_FULL       ; exit code - disk full
        cmp tmp3
        bne @save_done                  ; if any problem during save -> exit loop
        tax                  
        cmp #$00                        ; was it the last save cycle? (exit code is 0x00)
        beq @save_done

        sec                             ; substract sent bytes from remaining size to save
        lda tmp1
        sbc tmp3
        sta tmp1
        lda tmp2
        sbc #$00
        sta tmp2

        clc                             ; increase source address by sent bytes
        lda tmp3
        adc SAL                         
        sta SAL
        lda SAH
        adc #$00
        sta SAH

        jmp @save_start

@save_done:
        stx STATUS                      ; x contains the exit code - (ST)
        jsr copy_trampoline_code
        jsr copy_disable_ef_rom
        clc
        jmp disable_ef_rom

@compare_buffer_and_remaining_size:
        lda tmp2
        cmp #$01
        bcs @compare_done       ; C if size > SAVE_BUF_SIZE
        lda tmp1
        cmp #SAVE_BUF_SIZE      ; C if size >= SAVE_BUF_SIZE
@compare_done:
        rts

@ef3usb_send_receive:
        wait_usb_tx_ok
        jmp ef3usb_send_receive

.endproc

; =============================================================================
kff_send_command:
        sta tmp3
        lda #<cmd_header
        sta ptr1
        lda #>cmd_header
        sta ptr1 + 1
        ldx #$04
        jsr ef3usb_send_bytes
        lda tmp3
        jmp ef3usb_send_byte
cmd_header:
        .byte "kff:"

; =============================================================================
.proc ef3usb_send_bytes ; Note: Cannot send 0 bytes
ef3usb_send_bytes:
        ldy #$00
@next_byte:
        lda (ptr1),y
        wait_usb_tx_ok
        sta USB_DATA
        iny
        dex
        bne @next_byte
        rts
.endproc

; =============================================================================
ef3usb_send_byte:
        wait_usb_tx_ok
        sta USB_DATA
        rts

; =============================================================================
ef3usb_receive_2_bytes:
        wait_usb_rx_ok
        lda USB_DATA
        wait_usb_rx_ok
        ldx USB_DATA
        rts

; =============================================================================
ef3usb_receive_byte:
        wait_usb_rx_ok
        lda USB_DATA
        rts
