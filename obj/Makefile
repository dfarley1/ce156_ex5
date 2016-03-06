# File:         obj/Makefile
# Author:       Kerry Veenstra
#
# Note:         Do not run this Makefile directly. It is intended to be
#               run from a parent Makefile.
#
# References:   Mecklenburg, Robert. Managing Projects with GNU Make.
#               O'Reilly Media. 2004.


# ======================================================
# Usually the lines of this file do not need to change.
# But this file *is* where the GDB debugger flag is set.
# ======================================================


# The .c files are in the adjacent src directory.
vpath %.c ../src
vpath %.h ../src


.PHONY: all
all : $(OBJS)

CFLAGS += -ggdb


# Remember to do a "make clean" after editing the Makefile.
.PHONY: clean
clean :
	rm -f *.o *.d


# Automatic Dependency Generation for C files.
# See Mecklenburg, pp. 31-34.
# Note: with gcc, use "-M". With clint, use "-MM -MG".

%.d: %.c
	$(CC) -M $(CPPCFLAGS) $< > $@.$$$$; \
	sed 's,/($*/)/.o[ :]*,/1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(subst .c,.d,$(CFILES))

