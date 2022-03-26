;
; Copyright (c) 2019-2022 Kim Jørgensen
;
; Derived from EasySDK
; (c) 2009-2010 Thomas 'skoe' Giesel
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

.include "ef3usb_macros.s"

; Align with commands.h
CMD_EAPI_INIT           = $01
CMD_WRITE_FLASH         = $02
CMD_ERASE_SECTOR        = $03

REPLY_WRITE_WAIT        = $01
REPLY_WRITE_ERROR       = $02

; error codes
EAPI_ERR_RAM            = 1
EAPI_ERR_ROML           = 2
EAPI_ERR_ROMH           = 3
EAPI_ERR_ROML_PROTECTED = 4
EAPI_ERR_ROMH_PROTECTED = 5

; There's a pointer to our code base
EAPI_ZP_INIT_CODE_BASE   = $4b

; hardware dependend values
KFF_NUM_BANKS      = 64
KFF_MFR_ID         = $ff
KFF_DEV_ID         = $ff

EAPI_RAM_CODE      = $df80
EAPI_RAM_SIZE      = 124

; I/O address used to select the bank
EASYFLASH_IO_BANK    = $de00

EAPICodeBase:
        .byte $65, $61, $70, $69        ; signature "EAPI"

        .byte "KungFuFlash 1.2"
        .byte 0                         ; 16 bytes, must be 0-terminated

; =============================================================================
;
; EAPIInit: User API: To be called with JSR <load_address> + 20
;
; Read Manufacturer ID and Device ID from the flash chip(s) and check if this
; chip is supported by this driver. Prepare our private RAM for the other
; functions of the driver.
; When this function returns, EasyFlash will be configured to bank in the ROM
; area at $8000..$bfff.
;
; This function calls SEI, it restores all Flags except C before it returns.
; Do not call it with D-flag set. $01 must enable both ROM areas.
;
; parameters:
;       -
; return:
;       C   set: Flash chip not supported by this driver
;           clear: Flash chip supported by this driver
;       If C is clear:
;       A   Device ID
;       X   Manufacturer ID
;       Y   Number of physical banks (>= 64) or
;           number of slots (< 64) with 64 banks each
;       If C is set:
;       A   Error reason
; changes:
;       all registers are changed
;
; =============================================================================
EAPIInit:
        php
        sei
        ; backup ZP space
        lda EAPI_ZP_INIT_CODE_BASE
        pha
        lda EAPI_ZP_INIT_CODE_BASE + 1
        pha

        ; find out our memory address
        lda #$60        ; rts
        sta EAPI_ZP_INIT_CODE_BASE
        jsr EAPI_ZP_INIT_CODE_BASE
initCodeBase = * - 1
        tsx
        lda $100, x
        sta EAPI_ZP_INIT_CODE_BASE + 1
        dex
        lda $100, x
        sta EAPI_ZP_INIT_CODE_BASE
        clc
        bcc initContinue

RAMCode:
; This code will be copied to EasyFlash RAM at EAPI_RAM_CODE
.org EAPI_RAM_CODE
RAMContentBegin:
; =============================================================================
; JUMP TABLE (will be updated to be correct)
; =============================================================================
jmpTable:
        jmp EAPIWriteFlash - initCodeBase
        jmp EAPIEraseSector - initCodeBase
        jmp EAPISetBank - initCodeBase
        jmp EAPIGetBank - initCodeBase
        jmp EAPISetPtr - initCodeBase
        jmp EAPISetLen - initCodeBase
        jmp EAPIReadFlashInc - initCodeBase
        jmp EAPIWriteFlashInc - initCodeBase
        jmp EAPISetSlot - initCodeBase
        jmp EAPIGetSlot - initCodeBase
jmpTableEnd:

; =============================================================================
;
; Internal function
;
; Send command to Kung Fu Flash
;
; =============================================================================
kff_send_command:
        lda #'k'
        jsr ef3usb_send_byte
        lda #'f'
        jsr ef3usb_send_byte
        lda #'f'
        jsr ef3usb_send_byte
        lda #':'
        jsr ef3usb_send_byte
        txa
        jsr ef3usb_send_byte
        lda EAPI_WRITE_ADDR_LO
        jsr ef3usb_send_byte
        lda EAPI_WRITE_ADDR_HI
        jsr ef3usb_send_byte
        lda EAPI_WRITE_VAL
        jsr ef3usb_send_byte
        ; get reply
        jmp ef3usb_receive_byte

; =============================================================================
;
; Internal function
;
; Send byte to Kung Fu Flash via USB registers
;
; =============================================================================
ef3usb_send_byte:
        wait_usb_tx_ok
ef3usb_send_byte_no_wait:
        sta USB_DATA
        rts

; =============================================================================
;
; Internal function
;
; Send byte to Kung Fu Flash via USB registers
;
; return:
;       C   set: Timeout
;           clear: Okay
; =============================================================================
ef3usb_send_byte_timed:
        ldy #$0
:
        dey
        beq timeout
        bit USB_STATUS
        bvc :-
        clc
        bcc ef3usb_send_byte_no_wait

timeout:
        sec
        rts

; =============================================================================
;
; Internal function
;
; Receive byte from Kung Fu Flash via USB registers
;
; =============================================================================
ef3usb_receive_byte:
        wait_usb_rx_ok
        ldx USB_DATA
        rts

; =============================================================================
;
; Internal function
;
; Read a byte from the inc-address
;
; =============================================================================
readByteForInc:
EAPI_INC_ADDR_LO = * + 1
EAPI_INC_ADDR_HI = * + 2
            lda $ffff
            rts

; =============================================================================
; Variables
; =============================================================================

EAPI_TMP_VAL1           = * + 0
EAPI_TMP_VAL2           = * + 1
EAPI_SHADOW_BANK        = * + 2 ; copy of current bank number set by the user
EAPI_INC_TYPE           = * + 3 ; type used for EAPIReadFlashInc/EAPIWriteFlashInc
EAPI_LENGTH_LO          = * + 4
EAPI_LENGTH_MED         = * + 5
EAPI_LENGTH_HI          = * + 6
EAPI_WRITE_VAL          = * + 7
EAPI_WRITE_ADDR_LO      = * + 8
EAPI_WRITE_ADDR_HI      = * + 9
; =============================================================================
RAMContentEnd           = * + 10
.reloc
RAMCodeEnd:

.if RAMContentEnd - RAMContentBegin > EAPI_RAM_SIZE
    .error "Code too large"
.endif

.if * - initCodeBase > 256
    .error "RAMCode not addressable trough (initCodeBase),y"
.endif

initContinue:
        ; *** copy some code to EasyFlash private RAM ***
        ; length of data to be copied
        ldx #RAMCodeEnd - RAMCode - 1
        ; offset behind initCodeBase of last byte to be copied
        ldy #RAMCodeEnd - initCodeBase - 1
cidCopyCode:
        lda (EAPI_ZP_INIT_CODE_BASE),y
        sta EAPI_RAM_CODE, x
        cmp EAPI_RAM_CODE, x    ; check if there's really RAM at this address
        bne ciRamError
        dey
        dex
        bpl cidCopyCode

        ; *** calculate jump table ***
        ldx #0
cidFillJMP:
        inx
        clc
        lda jmpTable, x
        adc EAPI_ZP_INIT_CODE_BASE
        sta jmpTable, x
        inx
        lda jmpTable, x
        adc EAPI_ZP_INIT_CODE_BASE + 1
        sta jmpTable, x
        inx
        cpx #jmpTableEnd - jmpTable
        bne cidFillJMP
        clc
        bcc ciNoRamError
ciRamError:
        lda #EAPI_ERR_RAM
        sta EAPI_WRITE_ADDR_LO
        sec                     ; error
ciNoRamError:
        ; restore the caller's ZP state
        pla
        sta EAPI_ZP_INIT_CODE_BASE + 1
        pla
        sta EAPI_ZP_INIT_CODE_BASE
        bcs returnOnly          ; branch on error from above

        ; send init command to KFF
        lda #'k'
        jsr ef3usb_send_byte_timed
        bcs kffNotFound
        lda #'f'
        jsr ef3usb_send_byte_timed
        bcs kffNotFound
        lda #'f'
        jsr ef3usb_send_byte_timed
        bcs kffNotFound
        lda #':'
        jsr ef3usb_send_byte_timed
        bcs kffNotFound
        lda #CMD_EAPI_INIT
        jsr ef3usb_send_byte_timed
        bcs kffNotFound

        ; check reply from KFF
        ldy #$0
:       dey
        beq kffNotFound
        bit USB_STATUS
        bpl :-
        ldx USB_DATA
        bne kffNotFound

        ; Device ID
        ldx #KFF_DEV_ID
        stx EAPI_WRITE_ADDR_LO

        ; everything okay
        clc
        bcc returnOnly

kffNotFound:
        lda #EAPI_ERR_ROML
        sta EAPI_WRITE_ADDR_LO  ; error code in A
        sec

returnOnly:                     ; C indicates error
        lda EAPI_WRITE_ADDR_LO  ; device or error code in A
        bcs returnCSet
        ldx #KFF_MFR_ID         ; manufacturer in X
        ldy #KFF_NUM_BANKS      ; number of banks in Y

        plp
        clc                     ; do this after plp :)
        rts
returnCSet:
        plp
        sec                     ; do this after plp :)
        rts

; =============================================================================
;
; EAPIWriteFlash: User API: To be called with JSR jmpTable + 0 = $df80
;
; Write a byte to the given address. The address must be as seen in Ultimax
; mode, i.e. do not use the base addresses $8000 or $a000 but $8000 or $e000.
;
; When writing to flash memory only bits containing a '1' can be changed to
; contain a '0'. Trying to change memory bits from '0' to '1' will result in
; an error. You must erase a memory block to get '1' bits.
;
; This function uses SEI, it restores all flags except C before it returns.
; Do not call it with D-flag set. $01 must enable both ROM areas.
; It can only be used after having called EAPIInit.
;
; parameters:
;       A   value
;       XY  address (X = low), $8xxx/$9xxx or $Exxx/$Fxxx
;
; return:
;       C   set: Error
;           clear: Okay
; changes:
;       Z,N <- value
;
; =============================================================================
EAPIWriteFlash:
        sta EAPI_WRITE_VAL
        stx EAPI_WRITE_ADDR_LO
        sty EAPI_WRITE_ADDR_HI
        php
        sei

        ldx #CMD_WRITE_FLASH
        clc
        bcc sendCommand

; =============================================================================
;
; EAPIEraseSector: User API: To be called with JSR jmpTable + 3 = $df83
;
; Erase the sector at the given address. The bank number currently set and the
; address together must point to the first byte of a 64 kByte sector.
;
; When erasing a sector, all bits of the 64 KiB area will be set to '1'.
; This means that 8 banks with 8 KiB each will be erased, all of them either
; in the LOROM chip when $8000 is used or in the HIROM chip when $e000 is
; used.
;
; This function uses SEI, it restores all flags except C before it returns.
; Do not call it with D-flag set. $01 must enable the affected ROM area.
; It can only be used after having called EAPIInit.
;
; parameters:
;       A   bank
;       Y   base address (high byte), $80 for LOROM, $a0 or $e0 for HIROM
;
; return:
;       C   set: Error
;           clear: Okay
;
; change:
;       Z,N <- bank
;
; =============================================================================
EAPIEraseSector:
        sta EAPI_WRITE_VAL      ; used for bank number here
        stx EAPI_WRITE_ADDR_LO  ; backup of X only, no parameter
        sty EAPI_WRITE_ADDR_HI
        php
        sei

        ldx #CMD_ERASE_SECTOR

; =============================================================================
;
; Command stuff
;
; =============================================================================
sendCommand:
        lda EAPI_SHADOW_BANK
        sta EASYFLASH_IO_BANK

        jsr kff_send_command
        beq retOk
        cpx #REPLY_WRITE_WAIT
        beq writeWait
retError:
        plp
        sec ; error
        bcs ret

retOk:
        plp
        clc
ret:
        ldy EAPI_WRITE_ADDR_HI
        ldx EAPI_WRITE_ADDR_LO
        lda EAPI_WRITE_VAL
        rts

; =============================================================================
;
; Wait in RAM to allow the cartridge to be temporary disabled
;
; =============================================================================
writeWait:
        lda #0
        wait_usb_tx_ok
        sta USB_DATA
waitDone:
        wait_usb_rx_ok
        ldx USB_DATA
checkDone:
        cpx #'d'
        bne waitDone

        wait_usb_rx_ok
        ldx USB_DATA
        cpx #'o'
        bne checkDone

        wait_usb_rx_ok
        ldx USB_DATA
        cpx #'n'
        bne checkDone

        wait_usb_rx_ok
        ldx USB_DATA
        cpx #'e'
        bne checkDone
waitReply:
        wait_usb_rx_ok
        ldx USB_DATA
        beq retOk

        clc
        bcc retError

; =============================================================================
;
; EAPISetBank: User API: To be called with JSR jmpTable + 6 = $df86
;
; Set the bank. This will take effect immediately for cartridge read access
; and will be used for the next flash write or read command.
;
; This function can only be used after having called EAPIInit.
;
; parameters:
;       A   bank
;
; return:
;       -
;
; changes:
;       -
;
; =============================================================================
EAPISetBank:
        sta EAPI_SHADOW_BANK
        sta EASYFLASH_IO_BANK
        rts

; =============================================================================
;
; EAPIGetBank: User API: To be called with JSR jmpTable + 9 = $df89
;
; Get the selected bank which has been set with EAPISetBank.
; Note that the current bank number can not be read back using the hardware
; register $de00 directly, this function uses a mirror of that register in RAM.
;
; This function can only be used after having called EAPIInit.
;
; parameters:
;       -
;
; return:
;       A  bank
;
; changes:
;       Z,N <- bank
;
; =============================================================================
EAPIGetBank:
        lda EAPI_SHADOW_BANK
        rts

; =============================================================================
;
; EAPISetPtr: User API: To be called with JSR jmpTable + 12 = $df8c
;
; Set the pointer for EAPIReadFlashInc/EAPIWriteFlashInc.
;
; This function can only be used after having called EAPIInit.
;
; parameters:
;       A   bank mode, where to continue at the end of a bank
;           $D0: 00:0:1FFF=>00:1:0000, 00:1:1FFF=>01:0:1FFF (lhlh...)
;           $B0: 00:0:1FFF=>01:0:0000 (llll...)
;           $D4: 00:1:1FFF=>01:1:0000 (hhhh...)
;       XY  address (X = low) address must be in range $8000-$bfff
;
; return:
;       -
;
; changes:
;       -
;
; =============================================================================
EAPISetPtr:
        sta EAPI_INC_TYPE
        stx EAPI_INC_ADDR_LO
        sty EAPI_INC_ADDR_HI
        rts

; =============================================================================
;
; EAPISetLen: User API: To be called with JSR jmpTable + 15 = $df8f
;
; Set the number of bytes to be read with EAPIReadFlashInc.
;
; This function can only be used after having called EAPIInit.
;
; parameters:
;       XYA length, 24 bits (X = low, Y = med, A = high)
;
; return:
;       -
;
; changes:
;       -
;
; =============================================================================
EAPISetLen:
        stx EAPI_LENGTH_LO
        sty EAPI_LENGTH_MED
        sta EAPI_LENGTH_HI
        rts

; =============================================================================
;
; EAPIReadFlashInc: User API: To be called with JSR jmpTable + 18 = $df92
;
; Read a byte from the current pointer from EasyFlash flash memory.
; Increment the pointer according to the current bank wrap strategy.
; Pointer and wrap strategy have been set by a call to EAPISetPtr.
;
; The number of bytes to be read may be set by calling EAPISetLen.
; EOF will be set if the length is zero, otherwise it will be decremented.
; Even when EOF is delivered a new byte has been read and the pointer
; incremented. This means the use of EAPISetLen is optional.
;
; This function can only be used after having called EAPIInit.
;
; parameters:
;       -
;
; return:
;       A   value
;       C   set if EOF
;
; changes:
;       Z,N <- value
;
; =============================================================================
EAPIReadFlashInc:
        ; now we have to activate the right bank
        lda EAPI_SHADOW_BANK
        sta EASYFLASH_IO_BANK

        ; call the read-routine
        jsr readByteForInc

        ; remember the result & x/y registers
        sta EAPI_WRITE_VAL
        stx EAPI_TMP_VAL1
        sty EAPI_TMP_VAL2

        ; make sure that the increment subroutine of the
        ; write routine jumps back to us, and call it
        lda #$00
        sta EAPI_WRITE_ADDR_HI
        beq rwInc_inc

readInc_Length:
        ; decrement length
        lda EAPI_LENGTH_LO
        bne readInc_nomed
        lda EAPI_LENGTH_MED
        bne readInc_nohi
        lda EAPI_LENGTH_HI
        beq readInc_eof
        dec EAPI_LENGTH_HI
readInc_nohi:
        dec EAPI_LENGTH_MED
readInc_nomed:
        dec EAPI_LENGTH_LO
        ;clc ; no EOF - already set by rwInc_noInc
        bcc rwInc_return

readInc_eof:
        sec ; EOF
        bcs rwInc_return

; =============================================================================
;
; EAPIWriteFlashInc: User API: To be called with JSR jmpTable + 21 = $df95
;
; Write a byte to the current pointer to EasyFlash flash memory.
; Increment the pointer according to the current bank wrap strategy.
; Pointer and wrap strategy have been set by a call to EAPISetPtr.
;
; In case of an error the position is not inc'ed.
;
;
; This function can only be used after having called EAPIInit.
;
; parameters:
;       A   value
;
; return:
;       C   set: Error
;           clear: Okay
; changes:
;       Z,N <- value
;
; =============================================================================
EAPIWriteFlashInc:
        sta EAPI_WRITE_VAL
        stx EAPI_TMP_VAL1
        sty EAPI_TMP_VAL2

        ; load address to store to
        ldx EAPI_INC_ADDR_LO
        lda EAPI_INC_ADDR_HI
        cmp #$a0
        bcc writeInc_skip
        ora #$40 ; $a0 => $e0
writeInc_skip:
        tay
        lda EAPI_WRITE_VAL

        ; write to flash
        jsr jmpTable + 0
        bcs rwInc_return

        ; the increment code is used by both functions
rwInc_inc:
        ; inc to next position
        inc EAPI_INC_ADDR_LO
        bne rwInc_noInc

        ; inc page
        inc EAPI_INC_ADDR_HI
        lda EAPI_INC_TYPE
        and #$e0
        cmp EAPI_INC_ADDR_HI
        bne rwInc_noInc
        ; inc bank
        lda EAPI_INC_TYPE
        asl
        asl
        asl
        sta EAPI_INC_ADDR_HI
        inc EAPI_SHADOW_BANK

rwInc_noInc:
        ; no errors here, clear carry
        clc
        ; readInc: value has be set to zero, so jump back
        ; writeInc: value ist set by EAPIWriteFlash to the HI address (never zero)
        lda EAPI_WRITE_ADDR_HI
        beq readInc_Length
rwInc_return:
        ldy EAPI_TMP_VAL2
        ldx EAPI_TMP_VAL1
        lda EAPI_WRITE_VAL

EAPISetSlot:
EAPIGetSlot:
        rts
