#Module
MODULENAME  = chdev

#Root dirrectories
INCDIR  := include
SRCDIR  := src
OBJDIR  := obj
BINDIR  := bin

#Compiler variables
subdir-ccflags-y := -I$(src)/$(INCDIR)
CPPCC             = g++
CPPCFLAGS         = -std=c++11 -I$(INCDIR)

#Application variables
CPPTESTSRCS  := $(wildcard $(SRCDIR)/*.cpp)

#Invokes kbuild system in case KERNELRELEASE was defined
ifneq ($(KERNELRELEASE),)
	obj-m     += $(OBJDIR)/
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD       := $(shell pwd)
	
#Module and applications will be compiled
default: module app
	
#Only module will be compiled
module: | $(OBJDIR) $(BINDIR)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	@cp $(OBJDIR)/$(MODULENAME).ko $(BINDIR)/$(MODULENAME).ko
	
#Only applications will be compiled
app:    | $(OBJDIR) $(BINDIR)
	$(CPPCC) $(CPPCFLAGS) $(CPPTESTSRCS) -o $(BINDIR)/test_chdev

#Creates dirrectory for objects
$(OBJDIR):
	@cp -ar $(SRCDIR) $(OBJDIR)
	@rm -f $(OBJDIR)/chdev_test.cpp
	@touch $(OBJDIR)/Makefile
	@echo 'obj-m += $(MODULENAME).o'                           >> $(OBJDIR)/Makefile
	@echo '$(MODULENAME)-objs := chdev_main.o chdev_shared.o'  >> $(OBJDIR)/Makefile

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
