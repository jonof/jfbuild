noinst_LIBRARIES=libengine.a libeditor.a

libengine_a_CPPFLAGS=-I$(srcdir)/../include
libengine_a_CFLAGS=-W -Wall -Wimplicit -Wno-char-subscripts -Wno-unused \
		   -funsigned-char -fno-pic -fno-strict-aliasing -m32
libengine_a_SOURCES=a-c.c baselayer.c cache1d.c compat.c crc32.c defs.c \
		    engine.c polymost.c polymosttex.c polymosttex-squish.cc \
		    polymosttexcache.c hightile.c mdsprite.c glbuild.c \
		    kplib.c lzf_c.c lzf_d.c md4.c mmulti.c osd.c pragmas.c \
		    scriptfile.c textfont.c smalltextfont.c
if RENDERTYPEWIN
libengine_a_SOURCES += winlayer.c
endif
if RENDERTYPESDL
libengine_a_SOURCES += sdlayer.c
endif

libeditor_a_CPPFLAGS=-I$(srcdir)/../include
libeditor_a_CFLAGS=-W -Wall -Wimplicit -Wno-char-subscripts -Wno-unused \
		   -funsigned-char -fno-pic -fno-strict-aliasing -m32
libeditor_a_SOURCES=build.c config.c
if RENDERTYPEWIN
libeditor_a_SOURCES += startwin.editor.c
endif

