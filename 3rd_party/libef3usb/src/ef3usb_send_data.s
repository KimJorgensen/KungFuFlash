

.importzp   ptr1, ptr2, ptr3, ptr4
.importzp   tmp1, tmp2, tmp3, tmp4
.import     popa, popax

.include "ef3usb_macros.s"

size_zp      = ptr1
xfer_size_zp = ptr2
p_buff_zp    = ptr3
m_size_hi    = tmp1


; =============================================================================
;
; void __fastcall__ ef3usb_send_data(const void* data, uint16_t len);
;
; =============================================================================
.proc   _ef3usb_send_data
.export _ef3usb_send_data
_ef3usb_send_data:
        sta size_zp
        stx size_zp + 1         ; Save size

        jsr popax
        sta p_buff_zp
        stx p_buff_zp + 1       ; Save buffer address

        lda size_zp
        eor #$ff
        tax
        lda size_zp + 1
        eor #$ff
        sta m_size_hi           ; calc -size - 1
        ldy #0
        jmp @incCounter         ; inc to get -size

@byteLoop:
        lda (p_buff_zp), y

        ; xy contains number of bytes to be xfered (x = low byte)
        wait_usb_tx_ok
        sta USB_DATA

        iny
        bne @incCounter
        inc p_buff_zp + 1
@incCounter:
        inx
        bne @byteLoop
        inc m_size_hi
        bne @byteLoop

        rts
.endproc
