BASEDIR := ..
include ../common.mk

ROOT := $(BINDIR)/root
TARGET := $(BINDIR)/initramfs.cpio

STATIC := $(shell find root -type f)

.PHONY: all

all: $(TARGET)

$(TARGET): owners tools/mkcpio.py $(STATIC)
	mkdir -p $(ROOT)
	cp -r root/. $(ROOT)
	find $(ROOT) -type d -exec chmod 0755 {} +
	find $(ROOT) -type f -exec chmod 0644 {} +
	python3 tools/mkcpio.py $(ROOT) owners $(TARGET)

.DELETE_ON_ERROR:
