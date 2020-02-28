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

; Used ZP
ptr1    = $fb
ptr2    = $fc
tmp1    = $fd
tmp2    = $fe
tmp3    = $ff

EASYFLASH_BANK    = $de00
EASYFLASH_CONTROL = $de02
EASYFLASH_KILL    = $04
EASYFLASH_16K     = $87

trampoline      = $0100
ef_ram          = $df00
start_commands  = $8000

IBASIN          = $0324
ILOAD           = $0330

; Align with commands.h
CMD_LOAD                = $80
CMD_LOAD_NEXT_BANK      = $81
REPLY_READ_DONE         = $80
REPLY_READ_BANK         = $81

; =============================================================================
;
; void disk_mount_and_load(void);
;
; =============================================================================
.proc   _disk_mount_and_load
.export _disk_mount_and_load
_disk_mount_and_load:
        sei
        ldx #$ff                ; Reset stack
        txs
        stx $8004               ; Trash autostart (if any)

        jsr init_system_constants_light
        jsr $ff8a               ; Restore Kernal Vectors

        lda #$00                ; Clear start of BASIC area
        sta $0800
        sta $0801
        sta $0802

        ; === Disk API in EF RAM ===
        ldx #disk_api_end - disk_api
:       dex
        lda disk_api,x
        sta ef_ram,x
        cpx #$00
        bne :-

        ; === Start BASIC ===
        ldx #basic_mount_starter_end - basic_mount_starter
:       lda basic_mount_starter,x
        sta trampoline,x
        dex
        bpl :-

        lda IBASIN              ; Store old BASIN vector
        sta old_basin_vector
        lda IBASIN + 1
        sta old_basin_vector + 1

        lda ILOAD               ; Store old LOAD vector
        sta old_load_vector
        lda ILOAD + 1
        sta old_load_vector + 1

        lda start_commands
        beq @no_commands
        lda #<basin_trampoline  ; Install new BASIN handler
        sta IBASIN
        lda #>basin_trampoline
        sta IBASIN + 1
@no_commands:

        lda #<load_trampoline   ; Install new LOAD handler
        sta ILOAD
        lda #>load_trampoline
        sta ILOAD + 1

        lda #EASYFLASH_KILL
        jmp trampoline
.endproc

; =============================================================================
basic_mount_starter:
.org trampoline
        sta EASYFLASH_CONTROL
        jsr $ff81               ; Initialize screen editor
        cli
        jmp ($a000)             ; Start BASIC
.reloc
basic_mount_starter_end:

; =============================================================================
.proc print_commands
print_commands:
        lda #$4c                ; Setup jmp to old BASIN
        sta return_inst
        lda old_basin_vector
        sta return_inst + 1
        lda old_basin_vector + 1
        sta return_inst + 2

        lda $99
        bne @normal_basin       ; Not keyboard
        lda $d0
        bne @normal_basin       ; Still reading characters

        sty tmp1
        ldy print_text_ptr
@next_char:
        lda start_commands,y
        beq @end_of_line

        jsr $ffd2               ; Print char
        inc $c8                 ; End of line++
        iny
        bne @next_char

@end_of_line:
        sta $0292               ; Insertion before current line
        sta $d3                 ; Cursor at column 0
        sta $d4                 ; Editor in not in quote mode
        inc $d0                 ; End of line (return)
        iny
        sty print_text_ptr
        lda start_commands,y
        bne @normal_basin_y

        lda old_basin_vector    ; Restore BASIN vector
        sta IBASIN
        lda old_basin_vector + 1
        sta IBASIN + 1

@normal_basin_y:
        ldy tmp1
@normal_basin:
        jmp disable_ef_rom
.endproc

; =============================================================================
disk_api:
.org ef_ram
load_trampoline:
        jsr store_filename_enable_ef
        jmp kff_load

basin_trampoline:
        jsr enable_ef_rom
        jmp print_commands

old_basin_vector:
        .byte $00,$00
old_load_vector:
        .byte $00,$00
print_text_ptr:
        .byte $00
filename:
        .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

store_filename_enable_ef:
        ldy #$0f	        ; Store current file name
:       lda ($bb),y
        sta filename,y
        dey
        bpl :-

enable_ef_rom:
        pha
        sei
        lda $01                 ; Set memory configuration
        sta mem_config + 1
        ora #$07
        sta $01

        txa                     ; Save ZP
        pha
        ldx #$04
:       lda $fb,x
        sta zp_backup,x
        dex
        bpl :-
        pla
        tax

        lda #EASYFLASH_16K
        sta EASYFLASH_CONTROL
        pla
        rts

kernal_trampoline:
        lda #EASYFLASH_KILL
        sta EASYFLASH_CONTROL
kernal_call:
        jsr $ffff               ; Address will be replaced
        lda #EASYFLASH_16K
        sta EASYFLASH_CONTROL
        rts

load_prg:
        sta EASYFLASH_BANK
        ldx tmp2
        beq copy_rest

copy_256_bytes:
        lda (ptr1),y
        sta ($ae),y             ; Store loaded byte
        iny
        bne copy_256_bytes

        inc $af
        inc ptr2
        dex
        bne copy_256_bytes

copy_rest:
        ldx tmp1
        beq load_done

copy_bytes:
        lda (ptr1),y
        sta ($ae),y             ; Store loaded byte
        iny
        dex
        bne copy_bytes

        clc
        lda $ae
        adc tmp1
        sta $ae
        bcc load_done
        inc $af

load_done:
        ldx $ae
        ldy $af
        clc
        lda #$00                ; Set EF bank
        sta EASYFLASH_BANK
        lda more_to_load
        beq all_done
        jmp load_next_bank

all_done:
        lda #$60                ; Setup rts
        sta return_inst

disable_ef_rom:
        php                     ; Restore ZP
        pha
        txa
        pha
        ldx #$04
:       lda zp_backup,x
        sta $fb,x
        dex
        bpl :-
        pla
        tax

        lda #EASYFLASH_KILL
        sta EASYFLASH_CONTROL
mem_config:
        lda #$00                ; Restore mem config
        sta $01
        pla
        plp
        cli
return_inst:
        .byte $00,$00,$00       ; Replaced with jmp or rts
more_to_load:
        .byte $00
zp_backup:
        .byte $ff,$ff,$ff,$ff,$ff
.reloc
disk_api_end:

; =============================================================================
.proc kff_load
kff_load:
        lda $93
        bne @normal_load                ; Verify operation (not load)

        lda $ba
        cmp #$08
        beq @device_ok                  ; Device 8

@normal_load:
        lda #$4c                        ; Setup jmp to normal load
        sta return_inst
        lda old_load_vector
        sta return_inst + 1
        lda old_load_vector + 1
        sta return_inst + 2
        lda $93
        ldx $c3
        ldy $c4
        jmp disable_ef_rom

@device_ok:
        lda #$00                        ; Clear device status
        sta $90

        jsr search_for_file
        bcs @normal_load                ; Not found

        lda #$af                        ; Setup call to $f5af
        sta kernal_call + 1
        lda #$f5
        sta kernal_call + 2
        jsr kernal_trampoline           ; Print "Searching for"

        lda #$d2                        ; Setup call to $f5d2
        sta kernal_call + 1
        lda #$f5
        sta kernal_call + 2
        jsr kernal_trampoline           ; Print "Loading"

        jsr ef3usb_receive_2_bytes      ; Get PRG size
        sta tmp1
        stx tmp2

        jsr ef3usb_receive_2_bytes      ; Get load address
        sta $ae                         ; Set start address
        stx $af

        lda $b9
        bne start_load                  ; Secondary address set

        lda $c3                         ; Set start address
        sta $ae
        lda $c4
        sta $af

start_load:
        lda #$00                        ; ptr = Start of EF flash
        sta ptr1
        lda #$80
        sta ptr2

        ldy #$00                        ; Index
        lda #$01                        ; EF Bank
        jmp load_prg

search_for_file:
        ldy $b7
        beq file_not_found              ; Filename length = 0

        cpy #$11
        bcs @too_long                   ; Filename length > 16
        .byte $2c                       ; Dummy bit instruction
@too_long:
        ldy #$10                        ; Limit to 16
        sty tmp1

        lda #CMD_LOAD                   ; Send command
        jsr kff_send_command

        lda tmp1                        ; Send filename
        jsr ef3usb_send_byte
        tax
        lda #<filename
        sta ptr1
        lda #>filename
        sta ptr1 + 1
        jsr ef3usb_send_bytes

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
        jmp start_load

@read_error:
        lda #$10                        ; Set device status to VERIFY error occurred
        sta $90
        jmp all_done
.endproc

; "Export" function
load_next_bank = kff_load::load_next_bank

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
