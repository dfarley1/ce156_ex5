# File:         Makefile
# Author:       Kerry Veenstra
#
# Purpose:      This is the base Makefile for CMPE 156 Assignments.
#
#               You can edit this Makefile as necessary.
#               This Makefile expects the following directory structure:
#
#                   ./Makefile          This file.
#                   ./src/              Put your project's source files here.
#                   ./doc/              Put your project's documentation here.
#                   ./obj/              Object files are created here.
#                   ./obj/Makefile      Don't edit this helper Makefile.
#                   ./bin/              Executables are created here.
#                   ./bin/Makefile      Don't edit this helper Makefile.
#
#               The bin and obj directories contain their own Makefiles.
#               Usually these Makefiles do not need to be changed.
#
#               The executable programs are written in the bin directory.
#
# References:   Mecklenburg, Robert. Managing Projects with GNU Make.
#               O'Reilly Media. 2004.


# ======================================
# Edit these variables for your project.
# ======================================

CLIENT_EXE = myproxy
CLIENT_CFILES = myproxy.c
COMMON_CFILES = myunp.c


# ================================================================
# Usually the lines in the rest of this file can remain unchanged.
# ================================================================

CLIENT_OBJS   = $(subst .c,.o,$(CLIENT_CFILES))
SERVER_OBJS   = $(subst .c,.o,$(SERVER_CFILES))
COMMON_OBJS   = $(subst .c,.o,$(COMMON_CFILES))

CFILES = $(CLIENT_CFILES) $(SERVER_CFILES) $(COMMON_CFILES)
OBJS = $(CLIENT_OBJS) $(SERVER_OBJS) $(COMMON_OBJS)

export CLIENT_EXE SERVER_EXE
export CLIENT_CFILES SERVER_CFILES COMMON_CFILES CFILES
export CLIENT_OBJS SERVER_OBJS COMMON_OBJS OBJS


.PHONY: all
all : objs bins


.PHONY: objs
objs :
	cd obj && $(MAKE)


.PHONY: bins
bins :
	cd bin && $(MAKE)


.PHONY: clean
clean :
	cd obj && $(MAKE) clean

