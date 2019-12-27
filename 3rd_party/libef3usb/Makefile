
libname := libef3usb

inc := src/ef3usb_macros.s

obj := 
obj += obj/ef3usb_check_cmd.o
obj += obj/ef3usb_discard_rx.o
obj += obj/ef3usb_send_data.o
obj += obj/ef3usb_send_str.o
obj += obj/ef3usb_fread.o
obj += obj/ef3usb_fload.o
obj += obj/ef3usb_fclose.o

.PHONY: all
all: $(libname).lib

obj/%.o: src/%.s $(inc) | obj
	ca65 -I src -t c64 -o $@ $<

obj:
	mkdir -p obj

$(libname).lib: $(obj)
	rm -f $@
	ar65 a $@ $(obj)

.PHONY: clean
clean:
	rm -f $(obj)
	rm -rf obj
	rm -f $(libname).lib

.PHONY: distclean
distclean: clean
	rm -f *~
