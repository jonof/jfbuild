# Build Makefile for GNU Make

# Notes:
#   Turning on any level of optimisation with GCC 2.95 results in a binary
#   which has bugs. Example: photocopier in Pigsty of Episode 4 of Duke.
#   Turning on any level of optimisation with GCC 3.x results in a binary
#   which instantly crashes when run.
#   Moral: don't enable optimisations
#
# New notes as of 25 August 2004:
#   Apparently GCC 2.95 now produces acceptable code if you enable -O2, and
#   GCC 3.3.1 MinGW32 is OK with -O too, but I'm still not excited about
#   trusting it though, so the moral is probably best still "don't enable
#   optimisations".
#
# Compilation on Linux:
#   * GCC 3 objects to something in the GCC inline pragmas, so disable
#     USE_GCC_PRAGMAS. GCC also didn't like to link to NASM objects
#     compiled with the -g switch, so remove -g from ASFLAGS.
#   

SRC=src/
OBJ?=obj.gnu/
INC=include/
CFLAGS?=-DSUPERBUILD -DPOLYMOST -DUSE_OPENGL -DDYNAMIC_OPENGL

# If DYNAMIC_BUILD is absent from above, uncomment these lines
#GLLIBWIN=-lopengl32
#GLLIBLIN=-lGL

# filename extensions
o=o
res=o
asm=nasm

ENGINELIB=libengine.a
EDITORLIB=libbuild.a

# JBF 20040622: I've begun linking to the MS DX6.1 SDK as I'm using
# DirectMusic in another project and MinGW is happy to use MS-format
# libraries.
DXROOT=c:/sdks/msc/dx61
FMODROOT=c:/sdks/fmodapi370win32/api

# debugging enabled
debug=-DDEBUGGINGAIDS -ggdb
# debugging disabled
#debug=-fomit-frame-pointer

# -D these to enable certain features of the port's compile process
# USE_GCC_ASSEMBLY   Use NASM and compile the ported A.ASM assembly code.
#                    If this is not defined, alter the $(OBJ)a.$o in the
#                    ENGINEOBJS declaration to be $(OBJ)a-c.$o
# USE_GCC_PRAGMAS    Use GCC inline assembly macros instead of C code for
#                    the features in PRAGMAS.H
TARGETOPTS=-DUSE_GCC_ASSEMBLY -DUSE_GCC_PRAGMAS

CC=gcc
AS=nasm
RC=windres
override CFLAGS+= $(debug) -W -Wall -Werror-implicit-function-declaration \
	-Wno-char-subscripts -Wno-unused \
	-march=pentium -funsigned-char -DNO_GCC_BUILTINS $(TARGETOPTS) \
	-I$(INC) -I../jfaud/inc
LIBS=-lm ../jfaud/libjfaud.a
ASFLAGS=-s #-g
EXESUFFIX=

ENGINEOBJS=$(OBJ)engine.$o \
	$(OBJ)cache1d.$o \
	$(OBJ)a.$o \
	$(OBJ)pragmas.$o \
	$(OBJ)osd.$o \
	$(OBJ)crc32.$o \
	$(OBJ)engineinfo.$o \
	$(OBJ)baselayer.$o \
	$(OBJ)glbuild.$o \
	$(OBJ)compat.$o \
	$(OBJ)kplib.$o \
	$(OBJ)scriptfile.$o \
	$(OBJ)mmulti.$o \
	$(OBJ)defs.$o

EDITOROBJS=$(OBJ)build.$o \
	$(OBJ)config.$o

GAMEEXEOBJS=$(OBJ)game.$o \
	$(OBJ)jfaud_sound.$o \
	$(OBJ)config.$o \
	$(OBJ)$(ENGINELIB)

EDITOREXEOBJS=$(OBJ)bstub.$o \
	$(OBJ)$(EDITORLIB) \
	$(OBJ)$(ENGINELIB)

# detect the platform
uname=$(strip $(shell uname -s))
ifeq ($(findstring Linux,$(uname)),Linux)
	PLATFORM=LINUX
	RENDERTYPE=SDL
	ASFLAGS+= -f elf
	LIBS+= $(GLLIBLIN)
else
	ifeq ($(findstring MINGW32,$(uname)),MINGW32)
		PLATFORM=WINDOWS
		EXESUFFIX=.exe
		override CFLAGS+= -DUNDERSCORES -I$(DXROOT)/include -I$(FMODROOT)/inc
		LIBS+= -lmingwex -lwinmm -L$(DXROOT)/lib -L$(FMODROOT)/lib -lws2_32 $(GLLIBWIN)
		ASFLAGS+= -DUNDERSCORES
		GAMEEXEOBJS+= $(OBJ)gameres.$(res)
		EDITOREXEOBJS+= $(OBJ)buildres.$(res)
		RENDERTYPE ?= WIN
		ASFLAGS+= -f win32
	else
		PLATFORM=UNKNOWN
	endif
endif
	
ifeq ($(RENDERTYPE),SDL)
	ENGINEOBJS+= $(OBJ)sdlayer.$o
	override CFLAGS+= $(subst -Dmain=SDL_main,,$(shell sdl-config --cflags))
	LIBS+= $(shell sdl-config --libs)
else
	ifeq ($(RENDERTYPE),WIN)
		ENGINEOBJS+= $(OBJ)winlayer.$o
		LIBS+= -mwindows -ldxguid
	endif
endif

override CFLAGS+= -D$(PLATFORM) -DRENDERTYPE$(RENDERTYPE)=1

.PHONY: clean all utils writeengineinfo enginelib editorlib

# TARGETS
all: game$(EXESUFFIX) build$(EXESUFFIX) $(OBJ)$(ENGINELIB) $(OBJ)$(EDITORLIB)
utils: kextract$(EXESUFFIX) kgroup$(EXESUFFIX) transpal$(EXESUFFIX)

enginelib: $(OBJ)$(ENGINELIB)
$(OBJ)$(ENGINELIB): $(ENGINEOBJS)
	ar rc $@ $^
	ranlib $@

editorlib: $(OBJ)$(EDITORLIB)
$(OBJ)$(EDITORLIB): $(EDITOROBJS)
	ar rc $@ $^
	ranlib $@

game$(EXESUFFIX): $(GAMEEXEOBJS)
	$(CC) $(CFLAGS) -o $(OBJ)$@ $^ $(LIBS)
	cp -f $(OBJ)$@ $@
	cp -f $@ game.sym$(EXESUFFIX)
	strip $@
	
build$(EXESUFFIX): $(EDITOREXEOBJS)
	$(CC) $(CFLAGS) -o $(OBJ)$@ $^ $(LIBS)
	cp -f $(OBJ)$@ $@
	cp -f $@ build.sym$(EXESUFFIX)
	strip $@

pragmacheck$(EXESUFFIX): $(OBJ)pragmacheck.$o $(OBJ)pragmas.$o
	$(CC) $(subst -Dmain=app_main,,$(CFLAGS)) -o $@ $^
	
kextract$(EXESUFFIX): $(OBJ)kextract.$o
	$(CC) -o $@ $^
kgroup$(EXESUFFIX): $(OBJ)kgroup.$o
	$(CC) -o $@ $^
transpal$(EXESUFFIX): $(OBJ)transpal.$o $(OBJ)pragmas.$o
	$(CC) -o $@ $^

# DEPENDENCIES
include Makefile.deps

$(OBJ)kextract.$o: $(SRC)util/kextract.c
	$(CC) -funsigned-char -c $< -o $@
$(OBJ)kgroup.$o: $(SRC)util/kgroup.c
	$(CC) -funsigned-char -c $< -o $@
$(OBJ)transpal.$o: $(SRC)util/transpal.c
	$(CC) -funsigned-char -I$(INC) -c $< -o $@

.PHONY: $(OBJ)engineinfo.$o
$(OBJ)engineinfo.$o:
	echo "const char _engine_cflags[] = \"$(CFLAGS)\";" > $(SRC)tmp/engineinfo.c
	echo "const char _engine_libs[] = \"$(LIBS)\";" >> $(SRC)tmp/engineinfo.c
	echo "const char _engine_uname[] = \"$(shell uname -a)\";" >> $(SRC)tmp/engineinfo.c
	echo "const char _engine_compiler[] = \"$(CC) $(shell $(CC) -dumpversion) $(shell $(CC) -dumpmachine)\";" >> $(SRC)tmp/engineinfo.c
	echo "const char _engine_date[] = __DATE__ \" \" __TIME__;" >> $(SRC)tmp/engineinfo.c
	$(CC) $(CFLAGS) -c $(SRC)tmp/engineinfo.c -o $@ 2>&1

# RULES
$(OBJ)%.$o: $(SRC)%.nasm
	$(AS) $(ASFLAGS) $< -o $@

$(OBJ)%.$o: $(SRC)%.c
	$(CC) $(CFLAGS) -c $< -o $@ 2>&1
	
$(OBJ)%.$o: $(SRC)tmp/%.c
	$(CC) $(CFLAGS) -c $< -o $@ 2>&1

$(OBJ)%.$o: $(SRC)misc/%.rc
	$(RC) -i $^ -o $@

$(OBJ)%.$o: $(SRC)util/%.c
	$(CC) $(CFLAGS) -c $< -o $@ 2>&1


# PHONIES	
clean:
	-rm -f $(ENGINEOBJS) $(EDITOROBJS) $(GAMEEXEOBJS) $(EDITOREXEOBJS) game$(EXESUFFIX) game.sym$(EXESUFFIX) build$(EXESUFFIX) build.sym$(EXESUFFIX) core*

