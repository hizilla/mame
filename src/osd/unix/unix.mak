###########################################################################
#
#   unix.mak
#
#   unix-specific makefile
#
#   Copyright (c) 2024-2024 lixiasong.
###########################################################################


###########################################################################
#################   BEGIN USER-CONFIGURABLE OPTIONS   #####################
###########################################################################


#-------------------------------------------------
# specify build options; see each option below
# for details
#-------------------------------------------------

# uncomment next line to enable a build using Microsoft tools
# MSVC_BUILD = 1

# uncomment next line to use cygwin compiler
# CYGWIN_BUILD = 1

# uncomment next line to enable multi-monitor stubs on Windows 95/NT
# you will need to find multimon.h and put it into your include
# path in order to make this work
# WIN95_MULTIMON = 1

# uncomment next line to enable a Unicode build
# UNICODE = 1



###########################################################################
##################   END USER-CONFIGURABLE OPTIONS   ######################
###########################################################################


#-------------------------------------------------
# object and source roots
#-------------------------------------------------

UNIXSRC = $(SRC)/osd/$(OSD)
UNIXOBJ = $(OBJ)/osd/$(OSD)

OBJDIRS += $(UNIXOBJ)

#-------------------------------------------------
# OSD core library
#-------------------------------------------------

OSDCOREOBJS = \
	$(UNIXOBJ)/main.o

#-------------------------------------------------
# OSD UNIX library
#-------------------------------------------------

OSDOBJS = \
	$(UNIXOBJ)/osd_stub.o

#-------------------------------------------------
# rules for building the libaries
#-------------------------------------------------

$(LIBOCORE): $(OSDOBJS)

$(LIBOSD): $(OSDOBJS) $(OSDCOREOBJS)
