# if2a Makefile (C) D. Gauchard <deyv@free.fr>
# Licensed under the terms of the GNU Public License version 2

# Makefile.default is included by default
# if it does not exist, then Makefile.defaultconf will be tried.
# If you need to adjust the default conf, please copy Makefile.defaultconf
# to Makefile.default then edit it.

ifeq ($(shell test -f Makefile.default; echo $$?),0)
ifneq ($(quiet),1)
$(warning Using configuration from Makefile.default)
endif
include Makefile.default
else
ifneq ($(quiet),1)
$(warning Using configuration from Makefile.defaultconf)
endif
include Makefile.defaultconf
endif

######################################
# environment settings

hostmachine=$(shell uname -m)
hostkernel=$(shell uname -s)

#-----------------------
# native win32 environment autodetection
ifneq ($(COMSPEC),)

CFLAGS		+= -mno-cygwin
LDFLAGS		+= -mno-cygwin
WIN32		= 1
EXT		= .exe
HOSTEXT		= .exe

# libusb path where include/ and lib/gcc/ can be found
LIBUSBPATH	?= $(LIBUSBPATH_WIN32)

endif # native win32

#-----------------------
# crosswin32 environment
ifneq ($(CROSSWIN32),)

CC		= $(CROSSWIN32CC)
AR		= $(CROSSWIN32AR)
STRIP		= $(CROSSWIN32STRIP)
HOSTCC		= $(CROSSWIN32HOSTCC)
WIN32		= 1
EXT		= .exe
HOSTEXT		=

# libusb path where include/ and lib/gcc/ can be found
LIBUSBPATH	?= $(LIBUSBPATH_WIN32)

endif # crosswin32

#-----------------------
# OSX environment

# autodetection
ifeq ($(hostmachine),Power Macintosh)
OSX		= 1
endif # OSX autodetect

# environment
ifneq ($(OSX),)

LIBUSBPATH	?= $(LIBUSBPATH_OSX)

CFLAGS		+= -DOSX=1
LIBUSB		+= -framework IOKit
STATIC		= 0

# this will statically link libusb. Comment this if it does not work
# compiles on darwin 7.9.0
LINKUSB		= $(LIBUSBLIB)/libusb.a /System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation

endif # OSX env

#-----------------------
# default unix environment

ifeq ($(LIBUSBPATH),)
ifneq ($(LIBUSBPATH_UNIX),)
LIBUSBPATH	?= $(LIBUSBPATH_UNIX)
endif
endif

#-----------------------
# check LIBUSBPATH validity
ifneq ($(LIBUSBPATH),)

ifneq ($(quiet),1)
$(warning Checking libusb in LIBUSBPATH=$(LIBUSBPATH))
endif

ifeq ($(LIBUSBINCL),)
LIBUSBINCL=$(shell for i in . include; do if [ -f $(LIBUSBPATH)/$$i/usb.h ]; then echo $(LIBUSBPATH)/$$i; break; fi; done)
ifneq ($(LIBUSBINCL),)
ifneq ($(quiet),1)
$(warning .	found libusb include files in '$(LIBUSBINCL)/usb.h')
endif
endif
endif

ifeq ($(LIBUSBLIB),)
LIBUSBLIB=$(shell for i in lib/gcc lib .libs; do if [ -f $(LIBUSBPATH)/$$i/libusb.a ]; then echo $(LIBUSBPATH)/$$i; break; fi; done)
ifneq ($(LIBUSBLIB),)
ifneq ($(quiet),1)
$(warning .	found libusb lib     files in '$(LIBUSBLIB)/libusb.a')
endif
endif
endif

endif # libusbpath

ifneq ($(LIBUSBINCL),)
ifneq ($(shell test -f $(LIBUSBINCL)/usb.h; echo $$?),0)
$(warning '$(LIBUSBINCL)/usb.h' not found)
endif
CFLAGS		+= -I$(LIBUSBINCL)
endif

ifneq ($(LIBUSBLIB),)
ifneq ($(shell test -f $(LIBUSBLIB)/libusb.a; echo $$?),0)
$(warning '$(LIBUSBLIB)/libusb.a' not found)
endif
LIBUSB		+= -L$(LIBUSBLIB)
endif

######################################
# more general/default settings

# libusb
LINKUSB		?= -lusb

# local compiler

HOSTCC		?= $(CC)
HOSTCFLAGS	?= $(CFLAGS)
HOSTLDFLAGS	?= $(LDFLAGS)

ifeq ($(STATIC),1)
LDFLAGS		+= -static
endif

######################################
# project

LIB			= libf2a.a

ifeq ($(F2ACOMMON),1)
LIBOBJS_DRIVERS		+= drivers/cart-f2a/f2amisc.o drivers/cart-f2a/f2aultra.o
endif

ifeq ($(F2AUSBLINKER),1)
CFLAGS			+= -DF2AL=1
LIBOBJS_DRIVERS		+= drivers/cart-f2a/f2aio.o drivers/cart-f2a/f2ausb.o
endif

ifeq ($(F2AUSBWRITER),1)
CFLAGS			+= -DF2AW=1
LIBOBJS_DRIVERS		+= drivers/cart-f2a/f2ausbwriter.o
endif

ifeq ($(EFALINKER),1)
LIBOBJS_DRIVERS		+= drivers/cart-efa/efausb.o
endif

LIBOBJS_DRIVERS		+= drivers/cart-template/template.o
LIBOBJS_DRIVERS		+= drivers/linker-usb/an2131.o drivers/linker-usb/usblinker.o

LIBOBJS			= binware.o cartio.o cartmap.o cartrom.o cartutils.o $(LIBOBJS_DRIVERS)
ifneq ($(WIN32),) # win32
LIBOBJS			+= getopt.o
endif

ifeq ($(F2ACOMMON),1)
TARGETS			+= if2a$(EXT)
endif
ifeq ($(EFALINKER),1)
TARGETS			+= iefa$(EXT)
endif

all: help $(TARGETS)

crosswin32:
	$(MAKE) release CROSSWIN32=1 quiet=1

osx:
	$(MAKE) all OSX=1 quiet=1

help:
ifneq ($(quiet),1)
	@echo "available rules:"
	@echo "	$(MAKE) $(TARGETS) (default)"
	@echo "	$(MAKE) crosswin32"
	@echo "	$(MAKE) osx"
	@echo "	$(MAKE) strip = release"
	@echo "	$(MAKE) doc-html | doc-txt | doc"
	@echo "	$(MAKE) clean"
	@echo "	$(MAKE) distclean"
	@echo "	$(MAKE) binaries		- make binaries tarball in .."
	@echo "	$(MAKE) sources		- make sources  tarball in .."
	@echo "	$(MAKE) tarball		- make both sources and binaries"
endif

########## depends are done
ifeq ($(shell test -f .depends; echo $$?),0)

.PHONY: strip

include .depends

libf2a.a: $(LIBOBJS)
	$(AR) $(ARFLAGS) $@ $^

if2a$(EXT): if2a.o libf2a.a
	$(LINKDEBUG) $(CC) -o $@ $< -L. -lf2a -lm $(LIBUSB) $(LINKUSB) $(LDFLAGS)

iefa$(EXT): iefa.o libf2a.a
	$(LINKDEBUG) $(CC) -o $@ $< -L. -lf2a -lm $(LIBUSB) $(LINKUSB) $(LDFLAGS)

release strip: $(TARGETS)
	$(STRIP) $(TARGETS)

########## make depend first
else

libf2a.a $(TARGETS) strip release: .depends
	@$(MAKE) $@ quiet=1

endif

########## source builder

rawc-multi$(HOSTEXT): rawc-multi.o
	$(HOSTCC) -o $@ $^ $(HOSTLDFLAGS)
	
rawc-multi.o: rawc-multi.c
	$(HOSTCC) $(HOSTCFLAGS) -c $<

######################################
# binware

B		= binware
BINWARE		= $(B).c $(B).h

$(BINWARE): rawc-multi$(HOSTEXT) $(shell echo binware/*.{hex,gba,mb,img})
	@echo "Building $(B)..."
	@rm -f $(BINWARE)
ifeq ($(POGO),1)
	@./rawc-multi$(HOSTEXT) $(B) pogoshell $(POGOLOADER:%=$(B)/%)
endif
ifeq ($(F2ACOMMON),1)
	@./rawc-multi$(HOSTEXT) $(B) f2a_loader_pro $(F2A-LDR-PRO:%=$(B)/%)
	@./rawc-multi$(HOSTEXT) $(B) f2a_loader_ultra $(F2A-LDR-ULTRA:%=$(B)/%)
endif
ifeq ($(F2AUSBLINKER),1)
	@./rawc-multi$(HOSTEXT) $(B) f2a_usb_firmware $(F2A-LNK-FIRM:%=$(B)/%)
	@./rawc-multi$(HOSTEXT) $(B) f2a_multiboot $(F2A-LNK-MB:%=$(B)/%)
	@./rawc-multi$(HOSTEXT) $(B) f2a_splash $(F2A-LNK-SPLASH:%=$(B)/%)
endif
ifeq ($(F2AUSBWRITER),1)
	@./rawc-multi$(HOSTEXT) $(B) f2a_usb_writer_firmware $(F2A-WRT-FIRM:%=$(B)/%)
endif
ifeq ($(EFALINKER),1)
	@./rawc-multi$(HOSTEXT) $(B) efa_loader $(EFA-LDR:%=$(B)/%)
	@./rawc-multi$(HOSTEXT) $(B) efa_usb_firmware $(EFA-USB-FIRM:%=$(B)/%)
endif
	@./rawc-multi$(HOSTEXT) $(B) # close header file
	
######################################
# doc

SGML2TXT	= docbook2txt
SGML2HTML	= docbook2html -u

SGML2TXT	?= sgmltools -b txt
SGML2HTML	?= sgmltools -b onehtml

doc: doc-html doc-txt
doc-html: doc/FAQWTO.html
doc-txt: doc/FAQWTO.txt

%.html: %.sgml
	cd doc; $(SGML2HTML) $(<:doc/%=%)

%.txt: %.sgml
	cd doc; $(SGML2TXT) $(<:doc/%=%)

######################################
# distribution tarball

subbinaries: doc if2a$(EXT)
	@version=$(shell grep VERSION libf2a.h | awk '{print $$3;}' | sed "s,\",,g"); \
	uname=win32; [ "$(WIN32)" = 1 ] || uname=$(shell uname); \
	dirname=if2a-$$version-$$uname; \
	rm -rf if2a-$$version; mkdir -p $$dirname; \
	cp if2a$(EXT) doc/FAQWTO.{html,txt} $$dirname; \
	strip $$dirname/if2a$(EXT); \
	if [ "$(WIN32)" = 1 ]; then \
		zip -r9 ../$$dirname.zip $$dirname; \
		echo "../$$dirname.zip generated"; \
	else \
		tar cvf - $$dirname 2> /dev/null | bzip2 -c9 > ../$$dirname.tar.bz2; \
		echo "../$$dirname.tar.bz2 generated"; \
	fi; \
	rm -rf ./$$dirname

subsources: doc
	@version=$(shell grep VERSION libf2a.h | awk '{print $$3;}' | sed "s,\",,g"); \
	if2a=if2a-$$version; \
	tmp=/tmp/tmp-if2a.$$$$; \
	pwd=`pwd`; \
	tarname=$$pwd/../$$if2a.tar.bz2; \
	pwd=`pwd`; rm -rf $$tmp; mkdir -p $$tmp; cd $$tmp; ln -s $$pwd $$if2a; \
	files="`echo $$if2a/{Makefile{,.defaultconf},COPYING,*.[ch],binware/*.{hex,gba,mb,img},doc/FAQWTO.{html,txt}}; find $$if2a/drivers -name "*.[ch]";`"; \
	tar cvfh - $$files 2> /dev/null | bzip2 -c9 > $$tarname; \
	echo "$$tarname is generated"; \
	rm -rf $$tmp

tarball: distclean subsources subbinaries 
binaries: distclean subbinaries
sources: distclean subsources

######################################
# admin

.PHONY: dep clean distclean

.depends: $(BINWARE) #Makefile
	@echo "Building dependencies..."
	@$(HOSTCC) -MM $(CFLAGS) if2a.c iefa.c $(LIBOBJS:%.o=%.c) > $@

dep: .depends

clean:
	rm -f {.,*/*}/*.o

distclean: clean
	rm -f .depends $(BINWARE) libf2a.a {rawc-multi,if2a,iefa}{,.exe} doc/FAQWTO.{html,txt}
	@rm -f {.,*/*}/*~ DEADJOE
