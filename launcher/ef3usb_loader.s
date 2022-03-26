;
; Copyright (c) 2019-2022 Kim Jørgensen
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

.importzp ptr1

.import init_system
.import _ef3usb_fload
.import _ef3usb_fclose

.include "ef3usb_macros.s"
EASYFLASH_CONTROL = $de02
EASYFLASH_RAM     = $df00
EASYFLASH_KILL    = $04
EASYFLASH_16K     = $07

trampoline = $0100
IMAIN      = $0302
IBASIN     = $0324

MAIN       = $a483      ; Normal BASIC warm start
BACL       = $e394      ; BASIC cold start entry point
INIT       = $e397      ; Initialize BASIC RAM
SETLFS     = $ffba      ; Set logical, first, and second address

; Align with commands.h
LOADING_OFFSET  = $80
LOADING_TEXT    = EASYFLASH_RAM + LOADING_OFFSET

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
; void ef3usb_load_and_run(void);
;
; =============================================================================
.proc   _ef3usb_load_and_run
.export _ef3usb_load_and_run
_ef3usb_load_and_run:
        sei
        ldx #$ff                        ; Reset stack
        txs
        jsr init_system

        ; set current file (emulate read from drive 8)
        lda #$01
        ldx #$08
        tay
        jsr SETLFS                       ; Set file parameters

        ; === copy to EF RAM ===
        ldx #(ef_ram_end - ef_ram_start) - 1
:
        lda ef_ram_start, x
        sta EASYFLASH_RAM, x
        dex
        bpl :-

        lda IBASIN                      ; Store old BASIN vector
        sta old_basin_vector
        lda IBASIN + 1
        sta old_basin_vector + 1

        lda #<basin_trampoline          ; Install new BASIN handler
        sta IBASIN
        lda #>basin_trampoline
        sta IBASIN + 1

        ; === Start BASIC ===
        jsr init_basic
        ldx #<MAIN                      ; Restore BASIC warm start for C64GS
        ldy #>MAIN
.export start_basic
start_basic:
        stx IMAIN                       ; Install MAIN handler
        sty IMAIN + 1

        ldx #(basic_starter_end - basic_starter) - 1
:
        lda basic_starter, x
        sta trampoline, x
        dex
        bpl :-

        lda #EASYFLASH_KILL
        jmp trampoline

.export init_basic
init_basic:
        jmp (BACL + 1)                  ; Initialize BASIC vectors
.endproc

; =============================================================================
basic_starter:
.org trampoline
        sta EASYFLASH_CONTROL
        cli
        jmp INIT                        ; Start BASIC
.reloc
basic_starter_end:

; =============================================================================
ef_ram_start:
.org EASYFLASH_RAM
start_addr:
        .byte $ff,$ff
old_basin_vector:
        .byte $ff,$ff

basin_trampoline:
        sei
        txa
        pha
        tya
        pha
        lda #EASYFLASH_16K
        sta EASYFLASH_CONTROL
        jmp ef3usb_load_prg

disable_ef_rom:
        pla
        tay
        pla
        tax
        lda #EASYFLASH_KILL
        sta EASYFLASH_CONTROL
        cli
basin_return:
        jmp $ffff                       ; Address will be replaced

.if * > LOADING_TEXT
    .error "Not enough space in EF RAM for loading text"
.endif
zp_backup:
.reloc
ef_ram_end:

; =============================================================================
ef3usb_load_prg:
        lda old_basin_vector            ; Restore old BASIN vector
        sta IBASIN
        lda old_basin_vector + 1
        sta IBASIN + 1

        ldx #$00                        ; Print loading
@load_cpy:
        lda LOADING_TEXT, x
        beq @load_end
        sta $04f0, x
        lda $286                        ; Current color
        sta $d8f0, x
        inx
        bne @load_cpy
@load_end:

        ldx #$00                        ; Put command in keyboard buffer
@cmd_cpy:
        lda run_command, x
        beq @cmd_end
        sta $0277, x
        inx
        bne @cmd_cpy
@cmd_end:
        stx $c6                         ; Length of buffer

        ldx #$19                        ; ZP size - 1 from linker config
@backup_zp:
        lda $02, x
        sta zp_backup, x
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

        ldx #$19                        ; ZP size - 1 from linker config
@restore_zp:
        lda zp_backup, x
        sta $02, x
        dex
        bpl @restore_zp

        ; start the program
        lda start_addr
        ldx start_addr + 1
        cpx #>$0801                     ; looks like BASIC?
        bne @no_basic
        cmp #<($0801 + 1)
 @no_basic:
        bcs @machine_code

        ; === start basic program ===
        lda IBASIN                      ; Call current BASIN vector
        sta basin_return + 1
        lda IBASIN + 1
        sta basin_return + 2
        jmp disable_ef_rom

        ; === start machine code ===
@machine_code:
        sta basin_return + 1
        stx basin_return + 2
        jmp disable_ef_rom

run_command:
        .byte $11, $11, "run:", $0d, $00
