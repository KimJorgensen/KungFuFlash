

.importzp   ptr1, ptr2, ptr3, ptr4
.importzp   tmp1, tmp2, tmp3, tmp4
.import     popa, popax


.include "ef3usb_macros.s"

; =============================================================================
;
; void ef3usb_fclose(void);
;
; =============================================================================
.proc   _ef3usb_fclose
.export _ef3usb_fclose
_ef3usb_fclose:
        lda #0
        wait_usb_tx_ok
        sta USB_DATA
        wait_usb_tx_ok
        sta USB_DATA
        rts
.endproc
