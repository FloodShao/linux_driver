KVERS = $(shell uname -r)
KERNEL_PATH = /lib/modules/$(KVERS)/build

obj-m += second.o

build: kernel_modules user_test

kernel_modules:
	make -C $(KERNEL_PATH) M=$(CURDIR) modules

user_test:
	gcc -o second_test second_test.c

clean:
	make -C $(KERNEL_PATH) M=$(CURDIR) clean
	rm second_test second_test.o