;
; Copyright (c) 2019-2020 Kim Jørgensen
;
; Derived from EasyFlash 3 Boot Image and EasyFash 3 Menu
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
.importzp   ptr1, ptr2, ptr3, ptr4
.importzp   tmp1, tmp2, tmp3, tmp4
.import     popa, popax

.import init_system_constants_light
.import _ef3usb_fload
.import _ef3usb_fclose

.include "ef3usb_macros.s"

EASYFLASH_CONTROL = $de02
EASYFLASH_KILL    = $04
EASYFLASH_16K     = $07

trampoline = $0100
start_addr = $fb


.code
.if 1=0
hex:
        .byte $30, $31, $32, $33, $34, $35, $36, $37, $38, $39
        .byte 1, 2, 3, 4, 5, 6

dump_mem:
        ldx #0
@next:
        lda $0020,x
        lsr
        lsr
        lsr
        lsr
        tay
        lda hex,y
        sta $0400,x
        lda $0020,x
        and #$0f
        tay
        lda hex,y
        sta $0400+40,x
        inx
        cpx #32
        bne @next
        jmp *
.endif

; =============================================================================
;
; void usbtool_prg_load_and_run(void);
;
; =============================================================================
.proc   _usbtool_prg_load_and_run
.export _usbtool_prg_load_and_run
_usbtool_prg_load_and_run:
        sei
        jsr init_system_constants_light
        jsr $ff8a   ; Restore Kernal Vectors

        ; clear start of BASIC area
        lda #$00
        sta $0800
        sta $0801
        sta $0802

        ldx #$19    ; ZP size - 1 from linker config
@backup_zp:
        lda $02, x
        sta $df00, x
        dex
        bpl @backup_zp

        jsr _ef3usb_fload

        sta start_addr
        stx start_addr + 1

        ; set end addr + 1 to $2d and $ae
        clc
        adc ptr1
        sta $2d
        sta $ae
        txa
        adc ptr1 + 1
        sta $2e
        sta $af

        jsr _ef3usb_fclose

        ldx #$19    ; ZP size - 1 from linker config
@restore_zp:
        lda $df00, x
        sta $02, x
        dex
        bpl @restore_zp

        lda #$01    ; set current file (emulate read from drive 8)
        ldx #$08
        tay
        jsr $ffba   ; SETLFS - set file parameters

        ; start the program
        ; looks like BASIC?
        lda start_addr
        ldx start_addr + 1
        cmp #<$0801
        bne @no_basic
        cpx #>$0801
        bne @no_basic

        ; === start basic prg ===
        ldx #basic_starter_end - basic_starter
:
        lda basic_starter, x
        sta trampoline, x
        dex
        bpl :-
        bmi @run_it

        ; === start machine code ===
@no_basic:
        ldx #asm_starter_end - asm_starter
:
        lda asm_starter, x
        sta trampoline, x
        dex
        bpl :-

        lda start_addr
        sta trampoline_jmp_addr + 1
        lda start_addr + 1
        sta trampoline_jmp_addr + 2
@run_it:
        lda #EASYFLASH_KILL
        jmp trampoline
.endproc

; =============================================================================
basic_starter:
.org trampoline
        sta EASYFLASH_CONTROL
        jsr $ff81   ; Initialize screen editor

        ; for BASIC programs
        jsr $e453   ; Initialize Vectors
        jsr $e3bf   ; Initialize BASIC RAM

        jsr $a659   ; Set character pointer and CLR
        jmp $a7ae   ; Interpreter loop (RUN)
.reloc
basic_starter_end:

; =============================================================================
asm_starter:
.org trampoline
        sta EASYFLASH_CONTROL
        jsr $ff81   ; Initialize screen editor

        ; for BASIC programs (here too?)
        jsr $e453   ; Initialize Vectors
        jsr $e3bf   ; Initialize BASIC RAM

trampoline_jmp_addr:
        jmp $beef
.reloc
asm_starter_end:


.bss
cmd_buffer:
        .res 12
.data
; =============================================================================
;
; char* __fastcall__ ef3usb_get_cmd(bool send_fclose);
;
; This method runs from RAM to allow the cartridge to be temporary disabled
;
; =============================================================================
.proc   _ef3usb_get_cmd
.export _ef3usb_get_cmd
_ef3usb_get_cmd:
        sei
        beq ef3usb_get_cmd
; =============================================================================
;
; ef3usb_fclose
; This is a version of ef3usb_fclose that runs from RAM (DATA segment)
;
; =============================================================================
ef3usb_fclose:
        lda #0
        wait_usb_tx_ok
        sta USB_DATA
        wait_usb_tx_ok
        sta USB_DATA
; =============================================================================
;
; ef3usb_get_cmd
; This is a version of ef3usb_check_cmd that runs from RAM (DATA segment)
;
; =============================================================================
ef3usb_get_cmd:
        bit USB_STATUS
        bpl ef3usb_get_cmd      ; nothing received?

        ; move the buffer to the left
        ldx #0
@move_buf:
        lda cmd_buffer + 1, x
        sta cmd_buffer, x
        inx
        cpx #11
        bne @move_buf

        ; append new byte
        lda USB_DATA
        sta cmd_buffer + 11

        ; check if the buffer starts with "efstart:" and ends with "\0"
        ldx #7
@check:
        lda cmd_buffer, x
        cmp @efstart_str, x
        bne ef3usb_get_cmd
        dex
        bpl @check

        lda cmd_buffer + 11
        bne ef3usb_get_cmd      ; no 0-termination

        ; success, return the string behind "efstart:"
        lda #<(cmd_buffer + 8)
        ldx #>(cmd_buffer + 8)
        cli
        rts

@efstart_str:
        .byte   $45,$46,$53,$54,$41,$52,$54,$3A  ; "efstart:"
.endproc


; =============================================================================
;
; void wait_for_reset(void);
;
; Disable VIC-II output and wait for reset
;
; =============================================================================
.data
.proc   _wait_for_reset
.export _wait_for_reset
        sei
        lda #0
        sta $d011                       ; disable output (needed for FC-III)
        sta $d016                       ; 38 columns

        lda #0                          ; fclose
        wait_usb_tx_ok
        sta USB_DATA
        wait_usb_tx_ok
        sta USB_DATA

        jmp *
.endproc

; =============================================================================
;
; uint8_t is_c128(void);
;
; Return 0 if we're on a C64, other values for a C128.
;
; =============================================================================
.export _is_c128
_is_c128:
        ldx $d030
        inx
        txa
        rts
