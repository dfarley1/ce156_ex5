# File:         bin/Makefile
# Author:       Kerry Veenstra
#
# Note:         Do not run this Makefile directly. It is intended to be
#               run from a parent Makefile.
#
# References:   Mecklenburg, Robert. Managing Projects with GNU Make.
#               O'Reilly Media. 2004.


# ==============================================
# Usually the lines below do not need to change.
# ==============================================


# The .o files are in the adjacent obj directory.
vpath %.o ../obj

.PHONY: all
all : $(CLIENT_EXE) $(SERVER_EXE)

$(CLIENT_EXE) : $(CLIENT_OBJS) $(COMMON_OBJS)
	$(CC) $^ -o $@

$(SERVER_EXE) : $(SERVER_OBJS) $(COMMON_OBJS)
	$(CC) $^ -o $@

.PHONY: clean
clean :

