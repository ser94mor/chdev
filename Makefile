#Module
MODULENAME  = chdev

#Root dirrectories
INCDIR  := include
SRCDIR  := src
OBJDIR  := obj
BINDIR  := bin

#Module variables
EXTRA_CFLAGS := -I$(src)/$(INCDIR)

#Application variables
APPCC         = g++
APPCFLAGS     = -std=c++11 -I$(INCDIR)
APPTESTSRC   := $(SRCDIR)/chdev_test.cpp

#Invokes kbuild system in case KERNELRELEASE was defined
ifneq ($(KERNELRELEASE),)
	obj-m               := $(MODULENAME).o
	$(MODULENAME)-objs  := $(SRCDIR)/chdev_main.o   \
	                       $(SRCDIR)/chdev_shared.o
else
	KERNELDIR ?=  /lib/modules/$(shell uname -r)/build
	PWD       := $(shell pwd)
	
#Module and applications will be compiled
default: module app
	
#Only module will be compiled
module: | $(OBJDIR) $(BINDIR)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	@cp chdev.ko           bin/chdev.ko
	@mv chdev.ko           obj/chdev.ko
	@mv chdev.mod.c        obj/chdev.mod.c
	@mv chdev.mod.o        obj/chdev.mod.o
	@mv chdev.o            obj/chdev.o
	@mv Module.symvers     obj/Module.symvers
	@mv modules.order      obj/modules.order
	@mv src/chdev_main.o   obj/chdev_main.o
	@mv src/chdev_shared.o obj/chdev_shared.o
	@mv src/.tmp_versions  obj/.tmp_versions
	
#Only applications will be compiled
app:    | $(OBJDIR) $(BINDIR)
	$(APPCC) $(APPCFLAGS) $(APPTESTSRC) -o $(BINDIR)/test_chdev

#Creates dirrectory for objects
$(OBJDIR):
	@mkdir -p $(OBJDIR)
	
#Creates dirrectory for binary files
$(BINDIR):
	@mkdir -p $(BINDIR)
	
endif

load:
	@chmod +x load_chdev
	@./load_chdev
	
unload:
	@chmod +x unload_chdev
	@./unload_chdev

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	@rm -rf $(OBJDIR) $(BINDIR)
