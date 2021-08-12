all: ef3bbs ef3bbs.prg

# For Windows run "cl /O2 ef3bbs.c xfer.c net.c xmodem.c punter.c wsock32.lib" in a Visual Studio Command Prompt
ef3bbs: *
	cc -O2 -o ../ef3bbs ef3bbs.c xfer.c net.c xmodem.c punter.c

ef3bbs.prg: *
	64tass -a -o ../ef3bbs.prg ef3bbs.tas

clean:
	rm -f ../ef3bbs ../ef3bbs.prg
