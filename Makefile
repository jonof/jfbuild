# Build Makefile for GNU Make

# Notes:
#  As of 6 July 2005, the engine should handle optimisations being enabled.
#  If things seem to be going wrong, lower or disable optimisations, then
#  try again. If things are still going wrong, call me.
#   

SRC=src/
RSRC=rsrc/
OBJ?=obj.gnu/
INC=include/
CFLAGS?=-DSUPERBUILD -DPOLYMOST -DUSE_OPENGL -DDYNAMIC_OPENGL -DKSFORBUILD

# filename extensions
o=o
res=o
asm=nasm

DXROOT=c:/sdks/msc/dx61
FMODROOT=c:/sdks/fmodapi374win/api

# debugging enabled
debug=-ggdb -O0 #-DDEBUGGINGAIDS
# debugging disabled
#debug=-fomit-frame-pointer -O2

# -D these to enable certain features of the port's compile process
# USE_A_C   This uses a C version of the classic renderer code rather
#           than the assembly version in A.ASM.
#           If this is defined, alter the $(OBJ)a.$o in the
#           ENGINEOBJS declaration to be $(OBJ)a-c.$o
# NOASM     When defined, uses C code instead of GCC inline
#           assembly for the features in PRAGMAS.H
TARGETOPTS=#-DUSE_A_C #-DNOASM

CC=gcc
AS=nasm
RC=windres
override CFLAGS+= $(debug) -W -Wall -Wimplicit -Wno-char-subscripts -Wno-unused \
	-march=pentium -funsigned-char -fno-strict-aliasing -DNO_GCC_BUILTINS $(TARGETOPTS) \
	-I$(INC) -I../jfaud/inc
LIBS=-lm -lfmod # ../jfaud/jfaud.a
ASFLAGS=-s #-g
EXESUFFIX=

ENGINEOBJS=$(OBJ)a.$o \
	$(OBJ)baselayer.$o \
	$(OBJ)cache1d.$o \
	$(OBJ)compat.$o \
	$(OBJ)crc32.$o \
	$(OBJ)defs.$o \
	$(OBJ)engine.$o \
	$(OBJ)engineinfo.$o \
	$(OBJ)glbuild.$o \
	$(OBJ)kplib.$o \
	$(OBJ)mmulti.$o \
	$(OBJ)osd.$o \
	$(OBJ)pragmas.$o \
	$(OBJ)scriptfile.$o

EDITOROBJS=$(OBJ)build.$o \
	$(OBJ)config.$o

GAMEEXEOBJS=$(OBJ)game.$o \
	$(OBJ)sound.$o \
	$(OBJ)config.$o \
	$(OBJ)$(ENGINELIB)

EDITOREXEOBJS=$(OBJ)bstub.$o \
	$(OBJ)$(EDITORLIB) \
	$(OBJ)$(ENGINELIB)

include Makefile.shared

# detect the platform
ifeq ($(PLATFORM),LINUX)
	ASFLAGS+= -f elf
endif
ifeq ($(PLATFORM),BSD)
	ASFLAGS+= -f elf
	override CFLAGS+= -I/usr/X11R6/include
endif
ifeq ($(PLATFORM),WINDOWS)
	override CFLAGS+= -DUNDERSCORES -I$(DXROOT)/include -I$(FMODROOT)/inc
	LIBS+= -L$(FMODROOT)/lib
	ASFLAGS+= -DUNDERSCORES -f win32
endif

ifeq ($(RENDERTYPE),SDL)
	ENGINEOBJS+= $(OBJ)sdlayer.$o
	override CFLAGS+= $(subst -Dmain=SDL_main,,$(shell $(SDLCONFIG) --cflags))

	ifeq (1,$(HAVE_GTK2))
		override CFLAGS+= -DHAVE_GTK2 $(shell pkg-config --cflags gtk+-2.0)
	endif

	GAMEEXEOBJS+= $(OBJ)game_icon.$o
	EDITOREXEOBJS+= $(OBJ)build_icon.$o
endif
ifeq ($(RENDERTYPE),WIN)
	ENGINEOBJS+= $(OBJ)winlayer.$o
	EDITOROBJS+= $(OBJ)buildstartwin.$o
	GAMEEXEOBJS+= $(OBJ)gameres.$(res) $(OBJ)gamestartwin.$o
	EDITOREXEOBJS+= $(OBJ)buildres.$(res)
endif


ifeq ($(DYNAMIC_OPENGL),1)
	override CFLAGS+= -DDYNAMIC_OPENGL
endif

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
#	cp -f $@ game.sym$(EXESUFFIX)
#	strip $@
	
build$(EXESUFFIX): $(EDITOREXEOBJS)
	$(CC) $(CFLAGS) -o $(OBJ)$@ $^ $(LIBS)
	cp -f $(OBJ)$@ $@
#	cp -f $@ build.sym$(EXESUFFIX)
#	strip $@

pragmacheck$(EXESUFFIX): $(OBJ)pragmacheck.$o $(OBJ)pragmas.$o
	$(CC) $(subst -Dmain=app_main,,$(CFLAGS)) -o $@ $^
	
kextract$(EXESUFFIX): $(OBJ)kextract.$o $(OBJ)compat.$o
	$(CC) -o $@ $^
kgroup$(EXESUFFIX): $(OBJ)kgroup.$o $(OBJ)compat.$o
	$(CC) -o $@ $^
transpal$(EXESUFFIX): $(OBJ)transpal.$o $(OBJ)pragmas.$o $(OBJ)compat.$o
	$(CC) -o $@ $^
wad2art$(EXESUFFIX): $(OBJ)wad2art.$o $(OBJ)pragmas.$o $(OBJ)compat.$o
	$(CC) -o $@ $^
wad2map$(EXESUFFIX): $(OBJ)wad2map.$o $(OBJ)pragmas.$o $(OBJ)compat.$o
	$(CC) -o $@ $^
generateicon$(EXESUFFIX): $(OBJ)generateicon.$o $(OBJ)kplib.$o
	$(CC) -o $@ $^

# DEPENDENCIES
include Makefile.deps

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

$(OBJ)%.$o: $(RSRC)%.c
	$(CC) $(CFLAGS) -c $< -o $@ 2>&1


# PHONIES	
clean:
	-rm -f $(ENGINEOBJS) $(EDITOROBJS) $(GAMEEXEOBJS) $(EDITOREXEOBJS) game$(EXESUFFIX) game.sym$(EXESUFFIX) build$(EXESUFFIX) build.sym$(EXESUFFIX) core*

