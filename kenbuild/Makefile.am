bin_PROGRAMS = kenbuild kenbuild-build

kenbuild_CPPFLAGS=-I$(srcdir)/../include
kenbuild_CFLAGS=-W -Wall -Wimplicit -Wno-char-subscripts -Wno-unused \
		-funsigned-char -fno-pic -fno-strict-aliasing -m32
kenbuild_SOURCES=game.c sound_stub.c config.c
kenbuild_LDADD=../src/libengine.a

kenbuild_build_SOURCES=bstub.c
