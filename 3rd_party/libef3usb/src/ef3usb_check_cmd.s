

.include "ef3usb_macros.s"

.bss
cmd_buffer:
        .res 12

.code
; =============================================================================
;
; char* __fastcall__ ef3usb_check_cmd(void);
;
; Check if a command arrived from USB. The format is:
;
; Len:      012345678901
; Command: "efstart:crt#"  <= # = 0-termination
;
; Return 0 if there is no command complete. Return the 0-terminated file
; type (e.g. "crt") if it is complete.
;
; This is also used to synchronize the connection, i.e. all data is shifted
; through the buffer until a match with "efstart:???#" is found. The buffer is
; kept between calls. Yes, a simple state machine would be faster, but with
; this method I get the resulting buffer for free :)
;
; =============================================================================
.proc   _ef3usb_check_cmd
.export _ef3usb_check_cmd
_ef3usb_check_cmd:
        bit USB_STATUS
        bpl @end            ; nothing received?

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

        ; check if the buffer starts with "efstart:" and ends with "\n"
        ldx #7
@check:
        lda cmd_buffer, x
        cmp @efstart_str, x
        bne @end
        dex
        bpl @check

        lda cmd_buffer + 11
        bne @end                ; no 0-termination

        ; success, return the string behind "efstart:"
        lda #<(cmd_buffer + 8)
        ldx #>(cmd_buffer + 8)
        rts
@end:
        lda #0
        tax
        rts

@efstart_str:
        .byte   $45,$46,$53,$54,$41,$52,$54,$3A  ; "efstart:"
.endproc
