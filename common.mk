ARCH?= x86_64
BOOT?= efiboot

ROOT:= $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BINDIR:= $(ROOT)/bin
