all: ef3usb ef3usb.prg

ef3usb: ef3usb.c
	cc -O2 -o ../ef3usb ef3usb.c

ef3usb.prg: *
    # Build with 64tass 1.46 r38
	64tass -a -o ../ef3usb.prg ef3usb.tas

clean:
	rm -f ../ef3usb ../ef3usb.prg

