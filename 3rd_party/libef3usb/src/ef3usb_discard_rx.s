

.importzp   ptr1, ptr2, ptr3, ptr4
.importzp   tmp1, tmp2, tmp3, tmp4
.import     popa, popax


.include "ef3usb_macros.s"


; =============================================================================
;
; Discard all data from the USB RX buffer.
;
; void __fastcall__ ef3usb_discard_rx(void)
;
; in:
;       -
; out:
;       -
;
.export _ef3usb_discard_rx
_ef3usb_discard_rx:
        ldx #0
:
        lda USB_DATA
        dex
        bne :-
        rts
