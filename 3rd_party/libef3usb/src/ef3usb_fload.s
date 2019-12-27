

.importzp   ptr1, ptr2, ptr3, ptr4
.importzp   tmp1, tmp2, tmp3, tmp4
.import     popa, popax


size_zp      = ptr1
xfer_size_zp = ptr2
p_buff_zp    = ptr3
start_addr   = ptr4

.include "ef3usb_macros.s"

; =============================================================================
;
; unsigned void* ef3usb_fload(void);
;
; Load a file from USB to its start address (taken from the first two bytes).
; Return its start address. ptr1 contains the size on return (without start
; address).
;
; =============================================================================
.proc   _ef3usb_fload
.export _ef3usb_fload
_ef3usb_fload:
        php
        sei

        lda #$ff                ; request 64k data
        wait_usb_tx_ok          ; request bytes (from XY)
        sta USB_DATA
        wait_usb_tx_ok
        sta USB_DATA

        ; get number of bytes actually there
        wait_usb_rx_ok
        lda USB_DATA            ; low byte of transfer size
        sec
        sbc #2                  ; minus start address
        tax
        stx size_zp
        wait_usb_rx_ok
        ldy USB_DATA            ; high byte of transfer size
        bcs :+                  ; from sbc
        dey
:
        sty size_zp + 1

        bne @loadCont
        cpx #0                  ; check for EOF
        beq @end                ; 0 bytes == EOF
@loadCont:
        txa
        eor #$ff
        tax
        tya
        eor #$ff
        sta xfer_size_zp + 1    ; calc -size - 1

        txa
        clc
        adc #1                  ; calc -size
        sta xfer_size_zp
        lda xfer_size_zp + 1
        adc #0
        sta xfer_size_zp + 1
        beq @end                ; file too short?

        wait_usb_rx_ok          ; read start address
        lda USB_DATA
        sta p_buff_zp
        sta start_addr
        wait_usb_rx_ok
        lda USB_DATA
        sta p_buff_zp + 1
        sta start_addr + 1

        lda $01
        sta tmp1

        ldy #0
@getBytes:
        wait_usb_rx_ok
        lda USB_DATA
        ldx #$33                ; hide I/O
        stx $01
        sta (p_buff_zp), y
        ldx #$37                ; show I/O
        stx $01
        iny
        bne @incCounter
        inc p_buff_zp + 1
@incCounter:
        inc xfer_size_zp
        bne @getBytes
        inc xfer_size_zp + 1
        bne @getBytes
@end:
        lda tmp1
        sta $01
        plp
        lda start_addr
        ldx start_addr + 1
        rts
.endproc
