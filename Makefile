# Build Makefile for GNU Make

# Notes:
#  As of 6 July 2005, the engine should handle optimisations being enabled.
#  If things seem to be going wrong, lower or disable optimisations, then
#  try again. If things are still going wrong, call me.
#

# Create Makefile.user yourself to provide your own overrides
# for configurable values
-include Makefile.user

# Engine options - these may be overridden by game makefiles
#  SUPERBUILD     - enables voxels
#  POLYMOST       - enables Polymost renderer
#  USE_OPENGL     - enables OpenGL support in Polymost
#  DYNAMIC_OPENGL - enables run-time loading of OpenGL library
#  NOASM          - disables the use of assembly code
#  LINKED_GTK     - enables compile-time linkage to GTK
#
SUPERBUILD ?= 1
POLYMOST ?= 1
USE_OPENGL ?= 1
DYNAMIC_OPENGL ?= 1
NOASM ?= 0
LINKED_GTK ?= 0

# Debugging options
#  RELEASE - 1 = no debugging
#  EFENCE  - 1 = compile with Electric Fence for malloc() debugging
RELEASE?=1
EFENCE?=0

# SDK locations - adjust to match your setup
DXROOT ?= $(USERPROFILE)/sdks/directx/dx81

# build locations - OBJ gets overridden to the game-specific objects dir
OBJ?=obj.gnu
SRC=src
GAME=kenbuild
TOOLS=tools
INC=include

# filename extensions - these won't need to change
o=o
res=o
asm=nasm

# debugging and release
ifneq ($(RELEASE),0)
  # debugging disabled
  debug=-fomit-frame-pointer -O2
else
  # debugging enabled
  debug=-ggdb -O0 -DDEBUGGINGAIDS -DNOSDLPARACHUTE
endif

CC?=gcc
CXX?=g++
NASM?=nasm
WINDRES?=windres
AR?=ar
RANLIB=ranlib
OURCFLAGS=$(debug) -W -Wall -Wimplicit -Wno-unused \
	-fno-pic -fno-strict-aliasing -DNO_GCC_BUILTINS \
	-DKSFORBUILD -I$(INC) -I$(SRC)
OURCXXFLAGS=-fno-exceptions -fno-rtti
GAMECFLAGS=-I$(GAME) -I$(INC)
LIBS=
GAMELIBS=
NASMFLAGS=-s #-g
EXESUFFIX=

include Makefile.shared

ENGINEOBJS=
ifeq (0,$(NOASM))
  ENGINEOBJS+= $(SRC)/a.$o
endif

ENGINEOBJS+= \
	$(SRC)/a-c.$o \
  	$(SRC)/asmprot.$o \
	$(SRC)/baselayer.$o \
	$(SRC)/cache1d.$o \
	$(SRC)/compat.$o \
	$(SRC)/crc32.$o \
	$(SRC)/defs.$o \
	$(SRC)/engine.$o \
	$(SRC)/polymost.$o \
	$(SRC)/polymosttex.$o \
	$(SRC)/polymosttex-squish.$o \
	$(SRC)/polymosttexcache.$o \
	$(SRC)/hightile.$o \
	$(SRC)/mdsprite.$o \
	$(SRC)/glbuild.$o \
	$(SRC)/kplib.$o \
	$(SRC)/mmulti.$o \
	$(SRC)/osd.$o \
	$(SRC)/pragmas.$o \
	$(SRC)/scriptfile.$o \
	$(SRC)/textfont.$o \
	$(SRC)/smalltextfont.$o

EDITOROBJS=$(SRC)/build.$o \
	$(SRC)/config.$o

GAMEEXEOBJS=$(GAME)/game.$o \
	$(GAME)/sound_stub.$o \
	$(GAME)/config.$o \
	$(ENGINELIB)

EDITOREXEOBJS=$(GAME)/bstub.$o \
	$(EDITORLIB) \
	$(ENGINELIB)

ENGINEOBJS+= \
	$(LIBSQUISH)/alpha.$o \
	$(LIBSQUISH)/clusterfit.$o \
	$(LIBSQUISH)/colourblock.$o \
	$(LIBSQUISH)/colourfit.$o \
	$(LIBSQUISH)/colourset.$o \
	$(LIBSQUISH)/maths.$o \
	$(LIBSQUISH)/rangefit.$o \
	$(LIBSQUISH)/singlecolourfit.$o \
	$(LIBSQUISH)/squish.$o

# detect the platform
ifeq ($(PLATFORM),LINUX)
	NASMFLAGS+= -f elf
	LIBS+= -lm
endif
ifeq ($(PLATFORM),BSD)
	NASMFLAGS+= -f elf
	OURCFLAGS+= -I/usr/X11R6/include
	LIBS+= -lm
endif
ifeq ($(PLATFORM),WINDOWS)
	OURCFLAGS+= -I$(DXROOT)/include
	LIBS+= -lm
	NASMFLAGS+= -f win32 --prefix _
endif

ifeq ($(RENDERTYPE),SDL)
	ifeq ($(SDLAPI),2)
		ENGINEOBJS+= $(SRC)/sdlayer2.$o
	else
		ENGINEOBJS+= $(SRC)/sdlayer.$o
	endif
	OURCFLAGS+= $(SDLCONFIG_CFLAGS)

	ifeq (1,$(HAVE_GTK2))
		OURCFLAGS+= -DHAVE_GTK2 $(shell pkg-config --cflags gtk+-2.0)
		ENGINEOBJS+= $(SRC)/gtkbits.$o $(SRC)/dynamicgtk.$o
		EDITOROBJS+= $(SRC)/startgtk.editor.$o
		GAMEEXEOBJS+= $(GAME)/game_banner.$o $(GAME)/startgtk.game.$o
		EDITOREXEOBJS+= $(GAME)/editor_banner.$o
	endif

	GAMEEXEOBJS+= $(GAME)/game_icon.$o
	EDITOREXEOBJS+= $(GAME)/build_icon.$o
endif
ifeq ($(RENDERTYPE),WIN)
	ENGINEOBJS+= $(SRC)/winlayer.$o
	EDITOROBJS+= $(SRC)/startwin.editor.$o
	GAMEEXEOBJS+= $(GAME)/gameres.$(res) $(GAME)/startwin.game.$o
	EDITOREXEOBJS+= $(GAME)/buildres.$(res)
endif

ifneq (0,$(EFENCE))
	LIBS+= -lefence
	OURCFLAGS+= -DEFENCE
endif

OURCFLAGS+= $(BUILDCFLAGS)

.PHONY: clean veryclean all utils enginelib editorlib

# TARGETS

# Invoking Make from the terminal in OSX just chains the build on to xcode
ifeq ($(PLATFORM),DARWIN)
ifeq ($(RELEASE),0)
style=Debug
else
style=Release
endif
.PHONY: alldarwin
alldarwin:
	cd xcode && xcodebuild -project engine.xcodeproj -alltargets -configuration $(style)
	cd xcode && xcodebuild -project game.xcodeproj -alltargets -configuration $(style)
endif

UTILS=kextract$(EXESUFFIX) kgroup$(EXESUFFIX) transpal$(EXESUFFIX) wad2art$(EXESUFFIX) wad2map$(EXESUFFIX) arttool$(EXESUFFIX)

all: game$(EXESUFFIX) build$(EXESUFFIX)
utils: $(UTILS)
enginelib: $(ENGINELIB)
editorlib: $(EDITORLIB)

$(ENGINELIB): $(ENGINEOBJS)
	$(AR) rc $@ $^
	$(RANLIB) $@

$(EDITORLIB): $(EDITOROBJS)
	$(AR) rc $@ $^
	$(RANLIB) $@

game$(EXESUFFIX): $(GAMEEXEOBJS)
	$(CXX) $(CFLAGS) $(OURCFLAGS) -o $@ $^ $(GAMELIBS) $(LIBS)

build$(EXESUFFIX): $(EDITOREXEOBJS)
	$(CXX) $(CFLAGS) $(OURCFLAGS) -o $@ $^ $(LIBS)

kextract$(EXESUFFIX): $(TOOLS)/kextract.$o $(ENGINELIB)
	$(CC) -o $@ $^ $(ENGINELIB)
kgroup$(EXESUFFIX): $(TOOLS)/kgroup.$o $(ENGINELIB)
	$(CC) -o $@ $^ $(ENGINELIB)
transpal$(EXESUFFIX): $(TOOLS)/transpal.$o $(ENGINELIB)
	$(CC) -o $@ $^ $(ENGINELIB)
arttool$(EXESUFFIX): $(TOOLS)/arttool.$o
	$(CXX) -o $@ $^
wad2art$(EXESUFFIX): $(TOOLS)/wad2art.$o $(ENGINELIB)
	$(CC) -o $@ $^ $(ENGINELIB)
wad2map$(EXESUFFIX): $(TOOLS)/wad2map.$o $(ENGINELIB)
	$(CC) -o $@ $^ $(ENGINELIB)
generateicon$(EXESUFFIX): $(TOOLS)/generateicon.$o $(ENGINELIB)
	$(CC) -o $@ $^ $(ENGINELIB)
cacheinfo$(EXESUFFIX): $(TOOLS)/cacheinfo.$o $(ENGINELIB)
	$(CC) -o $@ $^ $(ENGINELIB)

# DEPENDENCIES
include Makefile.deps

# RULES
$(SRC)/%.$o: $(SRC)/%.$(asm)
	$(NASM) $(NASMFLAGS) $< -o $@

$(SRC)/%.$o: $(SRC)/%.c
	$(CC) $(CFLAGS) $(OURCFLAGS) -c $< -o $@

$(SRC)/%.$o: $(SRC)/%.cc
	$(CXX) $(CXXFLAGS) $(OURCXXFLAGS) $(OURCFLAGS) -c $< -o $@

$(SRC)/%.$o: $(SRC)/%.cpp
	$(CXX) $(CXXFLAGS) $(OURCXXFLAGS) $(OURCFLAGS) -c $< -o $@

$(LIBSQUISH)/%.$o: $(LIBSQUISH)/%.cpp
	$(CXX) $(CXXFLAGS) $(BUILDCFLAGS) -O2 -c $< -o $@

$(GAME)/%.$o: $(GAME)/%.c
	$(CC) $(CFLAGS) $(OURCFLAGS) $(GAMECFLAGS) -c $< -o $@

$(GAME)/%.$o: $(GAME)/%.cpp
	$(CXX) $(CXXFLAGS) $(OURCXXFLAGS) $(OURCFLAGS) $(GAMECFLAGS) -c $< -o $@

$(GAME)/%.$o: $(GAME)/rsrc/%.c
	$(CC) $(CFLAGS) $(OURCFLAGS) -c $< -o $@

$(GAME)/%.$(res): $(GAME)/rsrc/%.rc
	$(WINDRES) -i $< -o $@ --include-dir=$(INC) --include-dir=$(GAME)

$(RSRC)/game_banner.c: $(RSRC)/game.bmp
	echo "#include <gdk-pixbuf/gdk-pixdata.h>" > $@
	gdk-pixbuf-csource --extern --struct --raw --name=startbanner_pixdata $^ | sed 's/load_inc//' >> $@
$(RSRC)/editor_banner.c: $(RSRC)/build.bmp
	echo "#include <gdk-pixbuf/gdk-pixdata.h>" > $@
	gdk-pixbuf-csource --extern --struct --raw --name=startbanner_pixdata $^ | sed 's/load_inc//' >> $@

$(TOOLS)/%.$o: $(TOOLS)/%.c
	$(CC) $(CFLAGS) $(OURCFLAGS) -c $< -o $@

$(TOOLS)/%.$o: $(TOOLS)/%.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

# PHONIES
clean:
ifeq ($(PLATFORM),DARWIN)
	cd xcode && xcodebuild -project engine.xcodeproj -alltargets -configuration $(style) clean
	cd xcode && xcodebuild -project game.xcodeproj -alltargets -configuration $(style) clean
else
	-rm -f $(ENGINEOBJS) $(EDITOROBJS) $(GAMEEXEOBJS) $(EDITOREXEOBJS)
endif

veryclean: clean
ifeq ($(PLATFORM),DARWIN)
else
	-rm -f $(ENGINELIB) $(EDITORLIB) game$(EXESUFFIX) build$(EXESUFFIX) $(UTILS)
endif

.PHONY: fixlineends
fixlineends:
	for a in `find . -type f \( -name '*.c' -o -name '*.h' -o -name 'Makefile*' \) \! -path '*/.svn/*'`; do \
		echo Fixing $$a && tr -d "\015" < $$a > $$a.fix && mv $$a.fix $$a; \
	done
