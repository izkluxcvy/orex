include common.mk

SUBDIRS:= stand

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	for dir in $(SUBDIRS); do \
		cd $$dir && $(MAKE); \
		cd ..; \
	done

clean:
	rm -rf $(OBJDIR)/*
	rm -rf $(BINDIR)/*
