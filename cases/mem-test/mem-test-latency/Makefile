NAME_1 = mem-test-latency
INST ?= 10
CFLAGS += -DINST=$(INST) -g
NAME = $(NAME_1)-$(INST)
$(NAME).c:mem-test-latency.c
	cp mem-test-latency.c $(NAME).c
SRCS = $(NAME).c
include $(AM_HOME)/Makefile.app
clean-build:
	rm -f mem-test-latency-*
