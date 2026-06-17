ARCH ?= amd64
BOOT ?= efiboot

ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OBJDIR := $(ROOT)/obj
BINDIR := $(ROOT)/bin
