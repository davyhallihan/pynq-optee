# Top-level Makefile for the secure_switch benchmark suite
#
# Builds:
#   host/optee_benchmark  -- Linux userspace benchmark binary
#
# The secure-world benchmark logic runs in the PTA (pta/pta_benchmark.c),
# which is compiled into the OP-TEE OS image separately -- not by this Makefile.

export V?=0

HOST_CROSS_COMPILE ?= $(CROSS_COMPILE)

.PHONY: all
all:
	$(MAKE) -C host CROSS_COMPILE="$(HOST_CROSS_COMPILE)" --no-builtin-variables

.PHONY: clean
clean:
	$(MAKE) -C host clean
