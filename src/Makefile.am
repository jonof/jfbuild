noinst_LIBRARIES=libengine.a libeditor.a

optionflags=
if SUPERBUILD
optionflags += -DSUPERBUILD
endif
if POLYMOST
optionflags += -DPOLYMOST
endif
if USE_OPENGL
optionflags += -DUSE_OPENGL
endif
if DYNAMIC_OPENGL
optionflags += -DDYNAMIC_OPENGL
endif
if NOASM
optionflags += -DNOASM
endif
if LINKED_GTK
optionflags += -DLINKED_GTK
endif
if HAVE_GTK2
optionflags += -DHAVE_GTK2
endif
if RENDERTYPEWIN
optionflags += -DRENDERTYPEWIN
endif
if RENDERTYPESDL
optionflags += -DRENDERTYPESDL
endif

AM_CPPFLAGS=$(optionflags) -I$(srcdir)/../include
AM_CFLAGS=-W -Wall -Wimplicit -Wno-char-subscripts -Wno-unused \
	  -funsigned-char -fno-pic -fno-strict-aliasing

libengine_a_CPPFLAGS=$(AM_CPPFLAGS) -I$(srcdir)/../libsquish
libengine_a_SOURCES=a-c.c baselayer.c cache1d.c compat.c crc32.c defs.c \
		    engine.c polymost.c polymosttex.c polymosttex-squish.cc \
		    polymosttexcache.c hightile.c mdsprite.c glbuild.c \
		    kplib.c lzf_c.c lzf_d.c md4.c mmulti.c osd.c pragmas.c \
		    scriptfile.c textfont.c smalltextfont.c

libeditor_a_SOURCES=build.c config.c

if RENDERTYPEWIN
libengine_a_SOURCES += winlayer.c
libeditor_a_SOURCES += startwin.editor.c
endif
if RENDERTYPESDL
AM_CPPFLAGS += $(SDL_CFLAGS)

libengine_a_SOURCES += sdlayer.c

if HAVE_GTK2
AM_CPPFLAGS += $(GTK_CFLAGS)

libengine_a_SOURCES += gtkbits.c dynamicgtk.c
libeditor_a_SOURCES += startgtk.editor.c
endif
endif

