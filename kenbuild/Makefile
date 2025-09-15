# KenBuild Makefile for GNU Make

# Create Makefile.user yourself to provide your own overrides
# for configurable values
-include Makefile.user

# Engine options - these may be overridden by game makefiles
#  USE_POLYMOST   - enables Polymost renderer
#  USE_OPENGL     - enables OpenGL support in Polymost
#     Define as 0 to disable OpenGL
#     Define as USE_GL2 (or 1 or 2) for GL 2.0/2.1 profile
#     Define as USE_GL3 (or 3) for GL 3.2 Core profile
#     Define as USE_GLES2 (or 12) for GLES 2.0 profile
#  USE_ASM        - enables the use of assembly code if supported
#
USE_POLYMOST ?= 1
USE_OPENGL ?= 1
USE_ASM ?= 1

# Debugging options
#  RELEASE - 1 = optimised release build, no debugging aids
RELEASE?=1

# Engine source tree and game source locations
ENGINEROOT=..
ENGINEINC=$(ENGINEROOT)/include
SRC=src
RSRC=rsrc
GAMEDATA=data

CC?=gcc
CXX?=g++
RC?=windres

GAMECFLAGS=-g -W -Wall -fno-strict-aliasing -std=c99
GAMECXXFLAGS=-g -W -Wall -fno-exceptions -fno-rtti -std=c++98
GAMECPPFLAGS=-I$(SRC) -I$(ENGINEINC)
GAMELDFLAGS=

ifneq ($(RELEASE),0)
	# Default optimisation settings
	CFLAGS?=-fomit-frame-pointer -O2
	CXXFLAGS?=-fomit-frame-pointer -O2
else
	CFLAGS?=-Og
	CXXFLAGS?=-Og
endif

# Filename extensions, used in Makefile.deps etc
o=o
res=o

include $(ENGINEROOT)/Makefile.shared

# Apply shared flags
GAMECFLAGS+= $(BUILDCFLAGS)
GAMECXXFLAGS+= $(BUILDCXXFLAGS)
GAMECPPFLAGS+= $(BUILDCPPFLAGS)
GAMELDFLAGS+= $(BUILDLDFLAGS)

GAMEEXEOBJS=$(SRC)/game.$o \
	$(SRC)/config.$o \
	$(SRC)/kdmsound.$o

EDITOREXEOBJS=$(SRC)/bstub.$o

# Specialise for the platform
ifeq ($(PLATFORM),DARWIN)
	GAMEEXEOBJS+= $(SRC)/StartupWinController.$o
	GAMELDFLAGS+= -framework AppKit
endif

# Select the system layer
ifeq ($(RENDERTYPE),SDL)
	GAMECFLAGS+= $(SDLCONFIG_CFLAGS)
	GAMELDFLAGS+= $(SDLCONFIG_LIBS)

	GAMEEXEOBJS+= $(SRC)/kdmsound_sdl2.$o $(RSRC)/sdlappicon_game.$o
	EDITOREXEOBJS+= $(RSRC)/sdlappicon_build.$o

	ifeq (1,$(HAVE_GTK))
		GAMECFLAGS+= $(GTKCONFIG_CFLAGS)
		GAMELDFLAGS+= $(GTKCONFIG_LIBS)

		GAMEEXEOBJS+= $(SRC)/startgtk_game.$o $(RSRC)/startgtk_game_gresource.$o
		EDITOREXEOBJS+= $(RSRC)/startgtk_build_gresource.$o
	endif
endif
ifeq ($(RENDERTYPE),WIN)
	GAMEEXEOBJS+= $(SRC)/kdmsound_xaudio2.$o $(SRC)/gameres.$(res) $(SRC)/startwin_game.$o
	EDITOREXEOBJS+= $(SRC)/buildres.$(res)
	GAMELDFLAGS+= -lxaudio2_9 -lole32
endif

.PHONY: clean veryclean all
all: $(GAMEDATA)/game$(EXESUFFIX) $(GAMEDATA)/build$(EXESUFFIX)

include Makefile.deps

$(ENGINEROOT)/%:
	$(MAKE) -C $(@D) -f Makefile \
		USE_POLYMOST=$(USE_POLYMOST) \
		USE_OPENGL=$(USE_OPENGL) \
		USE_ASM=$(USE_ASM) \
		RELEASE=$(RELEASE) $(@F)

$(GAMEDATA)/game$(EXESUFFIX): $(GAMEEXEOBJS) $(ENGINEROOT)/$(ENGINELIB)
	$(CXX) $(CPPFLAGS) $(GAMECPPFLAGS) $(CXXFLAGS) $(GAMECXXFLAGS) -o $@ $^ $(LDFLAGS) $(GAMELDFLAGS)

$(GAMEDATA)/build$(EXESUFFIX): $(EDITOREXEOBJS) $(ENGINEROOT)/$(EDITORLIB) $(ENGINEROOT)/$(ENGINELIB)
	$(CXX) $(CPPFLAGS) $(GAMECPPFLAGS) $(CXXFLAGS) $(GAMECXXFLAGS) -o $@ $^ $(LDFLAGS) $(GAMELDFLAGS)

$(SRC)/%.$o: $(SRC)/%.c
	$(CC) $(CPPFLAGS) $(GAMECPPFLAGS) $(CFLAGS) $(GAMECFLAGS) -c $< -o $@

$(SRC)/%.$o: $(SRC)/%.m
	$(CC) $(CPPFLAGS) $(GAMECPPFLAGS) $(CFLAGS) $(GAMECFLAGS) -c $< -o $@

$(RSRC)/%.$o: $(RSRC)/%.c
	$(CC) $(CPPFLAGS) $(GAMECPPFLAGS) $(CFLAGS) $(GAMECFLAGS) -c $< -o $@

$(SRC)/%.$(res): $(SRC)/%.rc
	$(RC) -O coff -i $< -o $@ --include-dir=$(SRC) --include-dir=$(ENGINEINC)

$(RSRC)/%_gresource.c $(RSRC)/%_gresource.h: $(RSRC)/%.gresource.xml
	glib-compile-resources --generate --manual-register --c-name=startgtk --target=$@ --sourcedir=$(RSRC) $<
$(RSRC)/sdlappicon_%.c: $(RSRC)/%.png | $(ENGINEROOT)/generatesdlappicon$(EXESUFFIX)
	$(ENGINEROOT)/generatesdlappicon$(EXESUFFIX) $< > $@

clean:
	-rm -f $(GAMEEXEOBJS) $(EDITOREXEOBJS)

veryclean: clean
	-rm -f $(GAMEDATA)/game$(EXESUFFIX) $(GAMEDATA)/build$(EXESUFFIX)
