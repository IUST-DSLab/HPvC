#
# Copyright (C) 2009-2014 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

CAPI_INC      = -Ilib/include
PLATFORM_INC  = -Ilib/xpcom/include
PLATFORM_LIB  = lib/xpcom/lib
GLUE_DIR      = lib/glue
GLUE_INC      = -I$(GLUE_DIR)

CC            = gcc
CFLAGS        = -g -Wall

.PHONY: all
all: vhpc

.PHONY: clean
clean:
	rm -f vhpc.o VBoxCAPIGlue.o VirtualBox_i.o vhpc

vhpc: vhpc.o VBoxCAPIGlue.o VirtualBox_i.o
	$(CC) -o $@ $^ -ldl -lpthread

vhpc.o: vhpc.c
	$(CC) $(CFLAGS) $(CAPI_INC) $(PLATFORM_INC) $(GLUE_INC) -o $@ -c $<

VBoxCAPIGlue.o: $(GLUE_DIR)/VBoxCAPIGlue.c
	$(CC) $(CFLAGS) $(CAPI_INC) $(PLATFORM_INC) $(GLUE_INC) -o $@ -c $<

VirtualBox_i.o: $(PLATFORM_LIB)/VirtualBox_i.c
	$(CC) $(CFLAGS) $(CAPI_INC) $(PLATFORM_INC) $(GLUE_INC) -o $@ -c $<
