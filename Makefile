# Build Makefile for GNU Make

# Notes:
#  As of 6 July 2005, the engine should handle optimisations being enabled.
#  If things seem to be going wrong, lower or disable optimisations, then
#  try again. If things are still going wrong, call me.
#   

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
DXROOT=c:/sdks/directx/dx61
FMODROOTWIN=c:/sdks/fmodapi374win/api

# build locations - OBJ gets overridden to the game-specific objects dir
OBJ?=obj.gnu
SRC=src
GAME=testgame
RSRC=rsrc
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

CC=gcc
CXX=g++
AS=nasm
RC=windres
AR=ar
RANLIB=ranlib
OURCFLAGS=$(debug) -W -Wall -Wimplicit -Wno-char-subscripts -Wno-unused \
	-fno-pic -funsigned-char -fno-strict-aliasing -DNO_GCC_BUILTINS \
	-DKSFORBUILD -I$(INC)
OURCXXFLAGS=-fno-exceptions -fno-rtti
GAMECFLAGS=-I$(GAME)
LIBS=
GAMELIBS=
ASFLAGS=-s #-g
EXESUFFIX=

include Makefile.shared

ENGINEOBJS=
ifeq (0,$(NOASM))
  ENGINEOBJS+= $(OBJ)/a.$o
endif

ENGINEOBJS+= \
	$(OBJ)/a-c.$o \
	$(OBJ)/baselayer.$o \
	$(OBJ)/cache1d.$o \
	$(OBJ)/compat.$o \
	$(OBJ)/crc32.$o \
	$(OBJ)/defs.$o \
	$(OBJ)/engine.$o \
	$(OBJ)/polymost.$o \
	$(OBJ)/polymosttex.$o \
	$(OBJ)/polymosttex-squish.$o \
	$(OBJ)/polymosttexcache.$o \
	$(OBJ)/hightile.$o \
	$(OBJ)/mdsprite.$o \
	$(OBJ)/glbuild.$o \
	$(OBJ)/kplib.$o \
	$(OBJ)/lzf_c.$o \
	$(OBJ)/lzf_d.$o \
	$(OBJ)/md4.$o \
	$(OBJ)/mmulti.$o \
	$(OBJ)/osd.$o \
	$(OBJ)/pragmas.$o \
	$(OBJ)/scriptfile.$o \
	$(OBJ)/textfont.$o \
	$(OBJ)/smalltextfont.$o

EDITOROBJS=$(OBJ)/build.$o \
	$(OBJ)/config.$o

GAMEEXEOBJS=$(OBJ)/game.$o \
	$(OBJ)/sound_stub.$o \
	$(OBJ)/config.$o \
	$(OBJ)/$(ENGINELIB)

EDITOREXEOBJS=$(OBJ)/bstub.$o \
	$(OBJ)/$(EDITORLIB) \
	$(OBJ)/$(ENGINELIB)

ENGINEOBJS+= \
	$(OBJ)/libsquish/alpha.$o \
	$(OBJ)/libsquish/clusterfit.$o \
	$(OBJ)/libsquish/colourblock.$o \
	$(OBJ)/libsquish/colourfit.$o \
	$(OBJ)/libsquish/colourset.$o \
	$(OBJ)/libsquish/maths.$o \
	$(OBJ)/libsquish/rangefit.$o \
	$(OBJ)/libsquish/singlecolourfit.$o \
	$(OBJ)/libsquish/squish.$o

# detect the platform
ifeq ($(PLATFORM),LINUX)
	ASFLAGS+= -f elf
	LIBS+= -lm
endif
ifeq ($(PLATFORM),BSD)
	ASFLAGS+= -f elf
	OURCFLAGS+= -I/usr/X11R6/include
	LIBS+= -lm
endif
ifeq ($(PLATFORM),WINDOWS)
	OURCFLAGS+= -DUNDERSCORES -I$(DXROOT)/include -I$(FMODROOTWIN)/inc
	LIBS+= -lm
	GAMELIBS+= -L$(FMODROOTWIN)/lib
	ASFLAGS+= -DUNDERSCORES -f win32
endif
ifeq ($(PLATFORM),BEOS)
	ASFLAGS+= -f elf
	TARGETOPTS+= -DNOASM
endif
ifeq ($(PLATFORM),SUNOS)
	LIBS+= -lm
endif
ifeq ($(PLATFORM),SYLLABLE)
	ASFLAGS+= -f elf
endif

ifeq ($(RENDERTYPE),SDL)
	ENGINEOBJS+= $(OBJ)/sdlayer.$o
	OURCFLAGS+= $(subst -Dmain=SDL_main,,$(SDLCONFIG_CFLAGS))
	
	ifeq (1,$(HAVE_GTK2))
		OURCFLAGS+= -DHAVE_GTK2 $(shell pkg-config --cflags gtk+-2.0)
		ENGINEOBJS+= $(OBJ)/gtkbits.$o $(OBJ)/dynamicgtk.$o
		EDITOROBJS+= $(OBJ)/startgtk.editor.$o
		GAMEEXEOBJS+= $(OBJ)/game_banner.$o $(OBJ)/startgtk.game.$o
		EDITOREXEOBJS+= $(OBJ)/editor_banner.$o
	endif

	GAMEEXEOBJS+= $(OBJ)/game_icon.$o
	EDITOREXEOBJS+= $(OBJ)/build_icon.$o
endif
ifeq ($(RENDERTYPE),WIN)
	ENGINEOBJS+= $(OBJ)/winlayer.$o
	EDITOROBJS+= $(OBJ)/startwin.editor.$o
	GAMEEXEOBJS+= $(OBJ)/gameres.$(res) $(OBJ)/startwin.game.$o
	EDITOREXEOBJS+= $(OBJ)/buildres.$(res)
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

enginelib: $(OBJ)/$(ENGINELIB)
$(OBJ)/$(ENGINELIB): $(ENGINEOBJS)
	$(AR) rc $@ $^
	$(RANLIB) $@

editorlib: $(OBJ)/$(EDITORLIB)
$(OBJ)/$(EDITORLIB): $(EDITOROBJS)
	$(AR) rc $@ $^
	$(RANLIB) $@

game$(EXESUFFIX): $(GAMEEXEOBJS)
	$(CXX) $(CFLAGS) $(OURCFLAGS) -o $@ $^ $(GAMELIBS) $(LIBS)
	
build$(EXESUFFIX): $(EDITOREXEOBJS)
	$(CXX) $(CFLAGS) $(OURCFLAGS) -o $@ $^ $(LIBS)

pragmacheck$(EXESUFFIX): $(OBJ)/pragmacheck.$o $(OBJ)/pragmas.$o
	$(CC) $(subst -Dmain=app_main,,$(OURCFLAGS)) -o $@ $^
	
kextract$(EXESUFFIX): $(OBJ)/kextract.$o $(OBJ)/compat.$o
	$(CC) -o $@ $^
kgroup$(EXESUFFIX): $(OBJ)/kgroup.$o $(OBJ)/compat.$o
	$(CC) -o $@ $^
transpal$(EXESUFFIX): $(OBJ)/transpal.$o $(OBJ)/pragmas.$o $(OBJ)/compat.$o
	$(CC) -o $@ $^
arttool$(EXESUFFIX): $(OBJ)/arttool.$o
	$(CXX) -o $@ $^
wad2art$(EXESUFFIX): $(OBJ)/wad2art.$o $(OBJ)/pragmas.$o $(OBJ)/compat.$o
	$(CC) -o $@ $^
wad2map$(EXESUFFIX): $(OBJ)/wad2map.$o $(OBJ)/pragmas.$o $(OBJ)/compat.$o
	$(CC) -o $@ $^
generateicon$(EXESUFFIX): $(OBJ)/generateicon.$o $(OBJ)/kplib.$o
	$(CC) -o $@ $^
cacheinfo$(EXESUFFIX): $(OBJ)/cacheinfo.$o $(OBJ)/compat.$o
	$(CC) -o $@ $^
enumdisplay$(EXESUFFIX): src/misc/enumdisplay.c
	$(CC) -g -Os -o $@ $^ -I$(DXROOT)/include -lgdi32
mapdump$(EXESUFFIX): $(OBJ)/mapdump.$o
	$(CC) -o $@ $^

# DEPENDENCIES
include Makefile.deps

# RULES
$(OBJ)/%.$o: $(SRC)/%.nasm
	$(AS) $(ASFLAGS) $< -o $@

$(OBJ)/%.$o: $(SRC)/%.c
	$(CC) $(CFLAGS) $(OURCFLAGS) -c $< -o $@ 2>&1

$(OBJ)/%.$o: $(SRC)/%.cc
	$(CXX) $(CXXFLAGS) $(OURCXXFLAGS) $(OURCFLAGS) -c $< -o $@ 2>&1

$(OBJ)/%.$o: $(SRC)/%.cpp
	$(CXX) $(CXXFLAGS) $(OURCXXFLAGS) $(OURCFLAGS) -c $< -o $@ 2>&1

$(OBJ)/libsquish/%.$o: $(LIBSQUISH)/%.cpp
	test -d $(OBJ)/libsquish || mkdir $(OBJ)/libsquish
	$(CXX) $(CXXFLAGS) $(BUILDCFLAGS) -O2 -c $< -o $@ 2>&1

$(OBJ)/%.$o: $(SRC)/misc/%.rc
	$(RC) -i $< -o $@ --include-dir=$(INC) --include-dir=$(SRC) --include-dir=$(GAME)

$(OBJ)/%.$o: $(SRC)/util/%.c
	$(CC) $(CFLAGS) $(OURCFLAGS) -c $< -o $@ 2>&1

$(OBJ)/%.$o: $(SRC)/util/%.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@ 2>&1

$(OBJ)/%.$o: $(RSRC)/%.c
	$(CC) $(CFLAGS) $(OURCFLAGS) -c $< -o $@ 2>&1

$(OBJ)/%.$o: $(GAME)/%.c
	$(CC) $(CFLAGS) $(OURCFLAGS) $(GAMECFLAGS) -c $< -o $@ 2>&1

$(OBJ)/%.$o: $(GAME)/%.cpp
	$(CXX) $(CXXFLAGS) $(OURCXXFLAGS) $(OURCFLAGS) $(GAMECFLAGS) -c $< -o $@ 2>&1

$(OBJ)/game_banner.$o: $(RSRC)/game_banner.c
$(OBJ)/editor_banner.$o: $(RSRC)/editor_banner.c
$(RSRC)/game_banner.c: $(RSRC)/game.bmp
	echo "#include <gdk-pixbuf/gdk-pixdata.h>" > $@
	gdk-pixbuf-csource --extern --struct --raw --name=startbanner_pixdata $^ | sed 's/load_inc//' >> $@
$(RSRC)/editor_banner.c: $(RSRC)/build.bmp
	echo "#include <gdk-pixbuf/gdk-pixdata.h>" > $@
	gdk-pixbuf-csource --extern --struct --raw --name=startbanner_pixdata $^ | sed 's/load_inc//' >> $@

# PHONIES	
clean:
ifeq ($(PLATFORM),DARWIN)
	cd xcode && xcodebuild -project engine.xcodeproj -alltargets -configuration $(style) clean
	cd xcode && xcodebuild -project game.xcodeproj -alltargets -configuration $(style) clean
else
	-rm -f $(OBJ)/*
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
