KDIR ?= /lib/modules/`uname -r`/build
obj-m := rvtrace-trigger.o
M := make -C ${KDIR} M=`pwd`

all:
	${M} modules

modules_install:
	${M} modules_install

clean:
	${M} clean
