all: ef3usb ef3usb.prg

# For Windows run "cl /O2 ef3usb.c" in a Visual Studio Command Prompt
ef3usb: ef3usb.c
	cc -O2 -o ../ef3usb ef3usb.c

ef3usb.prg: *
	64tass -a -o ../ef3usb.prg ef3usb.tas

clean:
	rm -f ../ef3usb ../ef3usb.prg
