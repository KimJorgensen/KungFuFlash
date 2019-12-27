

.importzp   ptr1, ptr2, ptr3, ptr4
.importzp   tmp1, tmp2, tmp3, tmp4
.import     popa, popax


size_zp      = ptr1
xfer_size_zp = ptr2
p_buff_zp    = ptr3
m_size_hi    = tmp1

.include "ef3usb_macros.s"

; =============================================================================
;
; uint16_t __fastcall__ ef3usb_receive_data(void* buffer, uint16_t size);
;
; Reads exactly size bytes from USB to buffer. Do not return before the
; all data has been received. Return size.
;
; =============================================================================
.proc   _ef3usb_receive_data
.export _ef3usb_receive_data
_ef3usb_receive_data:
        jsr get_args

        lda xfer_size_zp
        ldy xfer_size_zp + 1
        jmp ef3usb_read_common
.endproc

get_args:
        sta size_zp
        sta xfer_size_zp
        stx size_zp + 1         ; Save size
        stx xfer_size_zp + 1

        jsr popax
        sta p_buff_zp
        stx p_buff_zp + 1       ; Save buffer address
        rts

; =============================================================================
;
; uint16_t __fastcall__ ef3usb_fread(void* buffer, uint16_t size);
;
; Request size bytes from the host. Read up to size bytes from USB to
; buffer. Returns the number of bytes actually read, 0 if there are no bytes
; left (EOF).
;
; =============================================================================
.proc   _ef3usb_fread
.export _ef3usb_fread
_ef3usb_fread:
        jsr get_args

        ldx size_zp
        ldy size_zp + 1
        wait_usb_tx_ok          ; request bytes (from XY)
        stx USB_DATA
        wait_usb_tx_ok
        sty USB_DATA

        ; get number of bytes actually there
        ; todo: we should check if this is more than we asked for
        wait_usb_rx_ok
        ldx USB_DATA            ; low byte of transfer size
        stx xfer_size_zp
        wait_usb_rx_ok
        ldy USB_DATA            ; high byte of transfer size
        sty xfer_size_zp + 1
        bne ef3usb_read_common
        cpx #0                  ; check for EOF
        beq end                 ; 0 bytes == EOF
        txa
.endproc

ef3usb_read_common:
        ; here: A = xfer_size low, Y = xfer_size high
        eor #$ff
        tax
        tya
        eor #$ff
        sta m_size_hi           ; calc -size - 1
        ldy #0
        jmp @incCounter         ; inc to get -size
@getBytes:
        ; xy contains number of bytes to be xfered (x = low byte)
        wait_usb_rx_ok
        lda USB_DATA
        sta (p_buff_zp), y
        iny
        bne @incCounter
        inc p_buff_zp + 1
@incCounter:
        inx
        bne @getBytes
        inc m_size_hi
        bne @getBytes
end:
        lda xfer_size_zp
        ldx xfer_size_zp + 1            ; return number of bytes transfered
        rts
