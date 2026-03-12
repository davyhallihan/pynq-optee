# Host application Makefile for optee_benchmark
#
# Builds the Linux userspace binary that runs the benchmarks.
# Links against libteec (OP-TEE client library) and libm (for sqrt).
#
# Called by the parent secure_switch/Makefile with CROSS_COMPILE and
# TEEC_EXPORT set appropriately for the target board.

CC      ?= $(CROSS_COMPILE)gcc
LD      ?= $(CROSS_COMPILE)ld
AR      ?= $(CROSS_COMPILE)ar
NM      ?= $(CROSS_COMPILE)nm
OBJCOPY ?= $(CROSS_COMPILE)objcopy
OBJDUMP ?= $(CROSS_COMPILE)objdump
READELF ?= $(CROSS_COMPILE)readelf

OBJS = main.o

CFLAGS += -Wall -I../include -I$(TEEC_EXPORT)/include -I./include
LDADD += -lteec -lm -L$(TEEC_EXPORT)/lib

BINARY = optee_benchmark

.PHONY: all
all: $(BINARY)

$(BINARY): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $< $(LDADD)

.PHONY: clean
clean:
	rm -f $(OBJS) $(BINARY)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
