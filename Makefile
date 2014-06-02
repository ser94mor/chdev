# If KERNELRELEASE is defined, we've been invoked# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-m      := chdev.o
	chdev-objs := chdev_main.o chdev_shared.o
else
	KERNELDIR ?=  /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(shell pwd) modules
endif

clean:
	$(MAKE) -C $(KERNELDIR) M=$(shell pwd) clean

