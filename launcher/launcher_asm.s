;
; Copyright (c) 2019-2022 Kim Jørgensen
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

.importzp   ptr1
.importzp   tmp1, tmp2
.import     popax

.import     kff_send_reply

KFF_DATA    = $de00
KFF_COMMAND = $de01
KFF_RAM_TST = $de03

; Align with commands.h
CMD_SYNC        = $55
REPLY_OK        = $80

; =============================================================================
;
; void __fastcall__ kff_send_size_data(void *data, uint8_t size);
;
; =============================================================================
.proc   _kff_send_size_data
.export _kff_send_size_data
_kff_send_size_data:
        sta tmp1                ; save size

        jsr popax               ; save buffer address
        sta ptr1
        stx ptr1 + 1

        ldy #0
        ldx tmp1
        stx KFF_DATA            ; send the size
        beq @end

@loop:  lda (ptr1),y            ; send the bytes
        sta KFF_DATA
        iny
        dex
        bne @loop

@end:   rts
.endproc

; =============================================================================
;
; void __fastcall__ kff_receive_data(void *data, uint16_t size);
;
; =============================================================================
.proc   _kff_receive_data
.export _kff_receive_data
_kff_receive_data:
        sta tmp1
        stx tmp2                ; save size

        jsr popax               ; save buffer address
        sta ptr1
        stx ptr1 + 1

        ldy #0
        ldx tmp2
        beq @last

@loop1: lda KFF_DATA            ; receive a page at a time
        sta (ptr1),y
        iny
        bne @loop1
        inc ptr1 + 1
        dex
        bne @loop1

@last:  ldx tmp1
        beq @end

@loop2: lda KFF_DATA            ; receive the remaining bytes
        sta (ptr1),y
        iny
        dex
        bne @loop2

@end:   rts
.endproc

; =============================================================================
;
; uint8_t __fastcall__ kff_send_reply(uint8_t reply);
;
; =============================================================================
.proc   _kff_send_reply
.export _kff_send_reply
_kff_send_reply:
        ldx #$00                ; return value required to be 16-bit
        jmp kff_send_reply
.endproc

; =============================================================================
;
; void kff_wait_for_sync(void);
;
; This method runs from RAM to allow the cartridge to be temporary disabled
;
; =============================================================================
.data
.proc   _kff_wait_for_sync
.export _kff_wait_for_sync
_kff_wait_for_sync:
        lda #REPLY_OK                   ; Send reply
        sta KFF_COMMAND

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
        sta $b2                         ; clear pointer to datasette buffer
        sta $b3                         ; needed for Jumpman Junior
        sta $d011                       ; disable output (needed for FC-III)
        sta $d016                       ; 38 columns

        lda #REPLY_OK
        sta KFF_COMMAND                 ; send reply

        jmp *
.endproc

; =============================================================================
;
; uint8_t is_c128(void);
;
; Return 0 if we're on a C64, other values for a C128.
;
; =============================================================================
.code
.export _is_c128
_is_c128:
        ldx $d030
        inx
        txa
        rts
