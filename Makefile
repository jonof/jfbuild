# Build Makefile for GNU Make

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

# Source locations
SRC=src
TOOLS=tools
INC=include
LIBSQUISH=libsquish

CC?=gcc
CXX?=g++
NASM?=nasm
AR?=ar
RANLIB?=ranlib
NASMFLAGS=-s #-g

# Host platform compiler for build-time tools
HOSTCC?=$(CC)
HOSTCXX?=$(CXX)

OURCFLAGS=-g -W -Wall -fno-strict-aliasing -std=c99
OURCXXFLAGS=-g -W -Wall -fno-exceptions -fno-rtti -std=c++98
OURCPPFLAGS=-I$(INC) -I$(SRC) -I$(LIBSQUISH)
OURLDFLAGS=

ifneq ($(RELEASE),0)
	# Default optimisation settings
	CFLAGS?=-fomit-frame-pointer -O2
	CXXFLAGS?=-fomit-frame-pointer -O2
else
	CFLAGS?=-Og
	CXXFLAGS?=-Og

	OURCPPFLAGS+= -DDEBUGGINGAIDS
	#OURCPPFLAGS+= -DMMULTI_DEBUG_SENDRECV
	#OURCPPFLAGS+= -DMMULTI_DEBUG_SENDRECV_WIRE
endif

# Filename extensions, used in Makefile.deps etc
o=o
res=o
asm=nasm

include Makefile.shared

# Apply shared flags
OURCFLAGS+= $(BUILDCFLAGS)
OURCXXFLAGS+= $(BUILDCXXFLAGS)
OURCPPFLAGS+= $(BUILDCPPFLAGS)
OURLDFLAGS+= $(BUILDLDFLAGS)

ENGINEOBJS= \
	$(SRC)/a-c.$o \
	$(SRC)/asmprot.$o \
	$(SRC)/baselayer.$o \
	$(SRC)/cache1d.$o \
	$(SRC)/compat.$o \
	$(SRC)/crc32.$o \
	$(SRC)/defs.$o \
	$(SRC)/engine.$o \
	$(SRC)/kplib.$o \
	$(SRC)/mmulti.$o \
	$(SRC)/osd.$o \
	$(SRC)/pragmas.$o \
	$(SRC)/scriptfile.$o \
	$(SRC)/textfont.$o \
	$(SRC)/talltextfont.$o \
	$(SRC)/smalltextfont.$o

ifneq (0,$(USE_ASM))
	ENGINEOBJS+= $(SRC)/a.$o
endif

ifneq ($(USE_OPENGL),0)
	ENGINEOBJS+= \
		$(SRC)/glbuild.$o \
		$(SRC)/glbuild_fs.$o \
		$(SRC)/glbuild_vs.$o
endif
ifneq ($(USE_POLYMOST),0)
	ENGINEOBJS+= $(SRC)/polymost.$o
	ifneq ($(USE_OPENGL),0)
		ENGINEOBJS+= \
			$(SRC)/hightile.$o \
			$(SRC)/mdsprite.$o \
			$(SRC)/polymost_fs.$o \
			$(SRC)/polymost_vs.$o \
			$(SRC)/polymostaux_fs.$o \
			$(SRC)/polymostaux_vs.$o \
			$(SRC)/polymosttex.$o \
			$(SRC)/polymosttexcache.$o \
			$(SRC)/polymosttexcompress.$o \
			$(SRC)/rg_etc1.$o \
			$(LIBSQUISH)/alpha.$o \
			$(LIBSQUISH)/clusterfit.$o \
			$(LIBSQUISH)/colourblock.$o \
			$(LIBSQUISH)/colourfit.$o \
			$(LIBSQUISH)/colourset.$o \
			$(LIBSQUISH)/maths.$o \
			$(LIBSQUISH)/rangefit.$o \
			$(LIBSQUISH)/singlecolourfit.$o \
			$(LIBSQUISH)/squish.$o
	endif
endif

EDITOROBJS=$(SRC)/build.$o \
	$(SRC)/config.$o

# Specialise for the platform
ifeq ($(PLATFORM),LINUX)
	NASMFLAGS+= -f elf
	OURLDFLAGS+= -lm
endif
ifeq ($(PLATFORM),BSD)
	NASMFLAGS+= -f elf
	OURLDFLAGS+= -lm
endif
ifeq ($(PLATFORM),WINDOWS)
	NASMFLAGS+= -f win32 --prefix _
	OURLDFLAGS+= -lm
endif
ifeq ($(PLATFORM),DARWIN)
	OURLDFLAGS+= -framework AppKit

	ENGINEOBJS+= $(SRC)/osxbits.$o
	EDITOROBJS+= $(SRC)/EditorStartupWinController.$o
endif

# Select the system layer
ifeq ($(RENDERTYPE),SDL)
	ENGINEOBJS+= $(SRC)/sdlayer2.$o
	OURCFLAGS+= $(SDLCONFIG_CFLAGS)
	OURLDFLAGS+= $(SDLCONFIG_LIBS)

	ifeq (1,$(HAVE_GTK))
		OURCFLAGS+= $(GTKCONFIG_CFLAGS)
		OURLDFLAGS+= $(GTKCONFIG_LIBS)
		ENGINEOBJS+= $(SRC)/gtkbits.$o
		EDITOROBJS+= $(SRC)/startgtk_editor.$o
	endif
endif
ifeq ($(RENDERTYPE),WIN)
	ENGINEOBJS+= $(SRC)/winlayer.$o
	EDITOROBJS+= $(SRC)/startwin_editor.$o
endif

# TARGETS

# Source-control version stamping
ifneq (,$(findstring git version,$(shell git --version)))
ENGINEOBJS+= $(SRC)/version-auto.$o
else
ENGINEOBJS+= $(SRC)/version.$o
endif

UTILS=kextract$(EXESUFFIX) kgroup$(EXESUFFIX) klist$(EXESUFFIX) transpal$(EXESUFFIX) arttool$(EXESUFFIX)
BUILDUTILS=generatesdlappicon$(EXESUFFIX) bin2c$(EXESUFFIX)

.PHONY: clean veryclean all utils libs enginelib editorlib kenbuild
all: libs utils kenbuild
utils: $(UTILS)
libs: enginelib editorlib
enginelib: $(ENGINELIB)
editorlib: $(EDITORLIB)

$(ENGINELIB): $(ENGINEOBJS)
	$(AR) rc $@ $^
	$(RANLIB) $@

$(EDITORLIB): $(EDITOROBJS)
	$(AR) rc $@ $^
	$(RANLIB) $@

kextract$(EXESUFFIX): $(TOOLS)/kextract.$o $(ENGINELIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(OURLDFLAGS)
kgroup$(EXESUFFIX): $(TOOLS)/kgroup.$o $(ENGINELIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(OURLDFLAGS)
klist$(EXESUFFIX): $(TOOLS)/klist.$o $(ENGINELIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(OURLDFLAGS)
transpal$(EXESUFFIX): $(TOOLS)/transpal.$o $(ENGINELIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(OURLDFLAGS)
arttool$(EXESUFFIX): $(TOOLS)/arttool.$o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^
wad2art$(EXESUFFIX): $(TOOLS)/wad2art.$o $(ENGINELIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(OURLDFLAGS)
wad2map$(EXESUFFIX): $(TOOLS)/wad2map.$o $(ENGINELIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(OURLDFLAGS)

# These tools are only used at build time and should be compiled
# using the host toolchain rather than any cross-compiler.
generatesdlappicon$(EXESUFFIX): $(TOOLS)/generatesdlappicon.c $(SRC)/kplib.c
	$(HOSTCC) $(CFLAGS) -I$(SRC) -I$(INC) -o $@ $^
bin2c$(EXESUFFIX): $(TOOLS)/bin2c.cc
	$(HOSTCXX) $(CXXFLAGS) -o $@ $^

# DEPENDENCIES
include Makefile.deps

# RULES
$(SRC)/%.$o: $(SRC)/%.$(asm)
	$(NASM) $(NASMFLAGS) $< -o $@

$(SRC)/%.$o: $(SRC)/%.c
	$(CC) $(CPPFLAGS) $(OURCPPFLAGS) $(CFLAGS) $(OURCFLAGS) -c $< -o $@

$(SRC)/%.$o: $(SRC)/%.cc
	$(CXX) $(CPPFLAGS) $(OURCPPFLAGS) $(CXXFLAGS) $(OURCXXFLAGS) -c $< -o $@

$(SRC)/%.$o: $(SRC)/%.cpp
	$(CXX) $(CPPFLAGS) $(OURCPPFLAGS) $(CXXFLAGS) $(OURCXXFLAGS) -c $< -o $@

$(SRC)/%.$o: $(SRC)/%.m
	$(CC) $(CPPFLAGS) $(OURCPPFLAGS) $(CFLAGS) $(OURCFLAGS) -c $< -o $@

$(SRC)/%.c: $(SRC)/%.glsl | bin2c$(EXESUFFIX)
	./bin2c$(EXESUFFIX) -text $< default_$*_glsl > $@

$(LIBSQUISH)/%.$o: $(LIBSQUISH)/%.cpp
	$(CXX) $(CPPFLAGS) $(OURCPPFLAGS) $(CXXFLAGS) $(OURCXXFLAGS) -O2 -c $< -o $@

$(TOOLS)/%.$o: $(TOOLS)/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -I$(SRC) -I$(INC) -c $< -o $@

$(TOOLS)/%.$o: $(TOOLS)/%.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# PHONIES
kenbuild:
	$(MAKE) -C kenbuild -f Makefile \
		USE_POLYMOST=$(USE_POLYMOST) \
		USE_OPENGL=$(USE_OPENGL) \
		USE_ASM=$(USE_ASM) \
		RELEASE=$(RELEASE)

clean:
	-rm -f $(ENGINEOBJS) $(EDITOROBJS)
	-$(MAKE) -C kenbuild -f Makefile clean

veryclean: clean
	-rm -f $(ENGINELIB) $(EDITORLIB) $(UTILS) $(BUILDUTILS)
	-$(MAKE) -C kenbuild -f Makefile veryclean

.PHONY: $(SRC)/version-auto.c
$(SRC)/version-auto.c:
	printf "const char *build_version = \"%s\";\n" "$(shell git describe --always || echo git error)" > $@
	echo "const char *build_date = __DATE__;" >> $@
	echo "const char *build_time = __TIME__;" >> $@
