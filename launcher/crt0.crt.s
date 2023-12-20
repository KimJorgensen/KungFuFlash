;
; Copyright (c) 2019-2023 Kim Jørgensen
;
; Derived from EasyFlash 3 Boot Image
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
; Startup code for cc65
; No IRQ support at the moment
;

.export _exit
.export __STARTUP__ : absolute = 1      ; Mark as startup
.export init_system

.import _main

.import initlib, donelib, copydata
.import zerobss
.import BSOUT
.import __RAM_START__, __RAM_SIZE__     ; Linker generated

.include "zeropage.inc"
.include "c64.inc"

CINT    = $ff81     ; Initialize screen editor
IOINIT  = $ff84     ; Initialize I/O devices
RESTOR  = $ff8a     ; Restore I/O vectors

EASYFLASH_BANK    = $DE00
EASYFLASH_CONTROL = $DE02
EASYFLASH_LED     = $80
EASYFLASH_16K     = $07
EASYFLASH_KILL    = $04

; ------------------------------------------------------------------------
; Actual code

.code
init_system:
        lda #$00        ; Clear start of BASIC area
        sta $0800
        sta $0801
        sta $0802
        sta $8004       ; Trash autostart (if any)

init_system_no_clear:
        jsr IOINIT      ; Initialize I/O
        jsr init_memory ; faster replacement for $ff87
        jsr RESTOR      ; Restore Kernal Vectors
        jmp CINT        ; Initialize screen editor

cold_start:
reset:
        ; same init stuff the kernel calls after reset
        ldx #$00
        stx $d016       ; 38 columns
        jsr init_system_no_clear

        ; Switch to second charset
        lda #$0e
        jsr BSOUT

        jsr zerobss
        jsr copydata

        ; and here
        ; Set argument stack ptr
        lda #<(__RAM_START__ + __RAM_SIZE__)
        sta sp
        lda #>(__RAM_START__ + __RAM_SIZE__)
        sta sp + 1

        jsr initlib
        cli
        jsr _main

_exit:
        jsr donelib
exit:
        jmp (reset_vector) ; reset, mhhh

; ------------------------------------------------------------------------
; faster replacement for $ff87
init_memory:
        ; from KERNAL @ FD50:
        lda #$00
        tay
:
        sta $0002,y
        sta $0200,y
        sta $0300,y
        iny
        bne :-
        ldx #$3c
        ldy #$03
        stx $b2         ; pointer to datasette buffer
        sty $b3
        tay

        ; result from loop KERNAL @ FD6C:
        lda #$a0
        sta $c2
        sta $0284       ; pointer to end of BASIC area

        ; from KERNAL @ FD90:
        lda #$08
        sta $0282       ; pointer to beginning of BASIC area

        lda #$04
        sta $0288       ; high byte of screen memory address
        rts

; ------------------------------------------------------------------------
; EAPI for Kung Fu Flash

        .segment "EAPI"
.proc eapi
.include "eapi-kung_fu_flash.s"
.endproc

; ------------------------------------------------------------------------
; This code is executed in Ultimax mode. It is called directly from the
; reset vector and must do some basic hardware initializations.
; It also contains trampoline code which will switch to 16k cartridge mode
; and call the normal startup code.
;
        .segment "ULTIMAX"
.proc ultimax_reset
ultimax_reset:
        ; === the reset vector points here ===
        sei
        ldx #$ff
        txs
        cld

        ; enable VIC (e.g. RAM refresh)
        lda #$08
        sta $d016

        ; write to RAM to make sure it starts up correctly (=> RAM datasheets)
        lda #$00
wait:
        sta $0100, x
        dex
        bne wait

        ; copy the final start-up code to RAM (bottom of CPU stack)
        ldx #(trampoline_end - trampoline) - 1
l1:
        lda trampoline, x
        sta $0100, x
        dex
        bpl l1

        lda #EASYFLASH_16K + EASYFLASH_LED
        jmp $0100

trampoline:
        ; === this code is copied to the stack area, does some inits ===
        ; === starts the main application                            ===
        sta EASYFLASH_CONTROL
        jmp cold_start
trampoline_end:
.endproc

; ------------------------------------------------------------------------

        .segment "ULTIMAX"
.proc dummy_vector
dummy_vector:
        dec $d020
        jmp dummy_vector
.endproc

        .segment "VECTORS"
.word   dummy_vector    ; NMI vector
reset_vector:
.word   ultimax_reset   ; Reset vector
.word   dummy_vector    ; IRQ/BRK vector
