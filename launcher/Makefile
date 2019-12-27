# do not remove intermediate targets
.SECONDARY:

ifndef version
version := 1.xx
endif

name := KungFuLauncher_v$(version)

ld_config := ld.crt.cfg

obj :=
obj += build/main.o
obj += build/base.o
obj += build/screen.o
obj += build/dir.o
obj += build/kff_usb.o
obj += build/prg.o

inc      := .
inc      += ../3rd_party/libef3usb/src

libef3usb := ../3rd_party/libef3usb/libef3usb.lib

INCLUDE  := $(addprefix -I,$(inc))
DEFINE   := -DVERSION=\"${version}\"

.PHONY: all
all: build/$(name).bin

###############################################################################
# Poor men's dependencies: Let all files depend from all header files
#
headers := $(wildcard *.h)

build/%.s: %.c $(headers) | build
	cc65 -t c64 -T -O --static-locals $(INCLUDE) $(DEFINE) -o $@ $<

###############################################################################
build/%.o: build/%.s | build
	ca65 -t c64 $(INCLUDE) -o $@ $<

###############################################################################
build/%.o: %.s | build
	ca65 -t c64 $(INCLUDE) -o $@ $<

###############################################################################
#
$(libef3usb): force
	$(MAKE) -C $(dir $@)

.PHONY: force
force:

###############################################################################
build:
	mkdir -p $@

build/$(name).bin: build/crt0.crt.o $(obj) $(ld_config) $(libef3usb)
	ld65 -o $@ -m $@.map -C $(ld_config) build/crt0.crt.o $(obj) \
		--lib  $(libef3usb) \
		-L /usr/local/lib/cc65/lib --lib c64.lib
	cat $@.map | grep -e "^Name\|^CODE\|^DATA\|^BSS\|^RODATA\|^ONCE\|^INIT"

.PHONY: clean
clean:
	rm -rf build
	-$(MAKE) -C $(dir $(libef3usb)) clean
