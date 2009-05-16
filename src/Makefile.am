include $(top_srcdir)/Makefile.amshared

AM_CPPFLAGS = $(optionflags) -I$(srcdir)/../include $(WINDOWS_CPPFLAGS)
AM_CFLAGS = -W -Wall -Wimplicit -Wno-char-subscripts -Wno-unused \
	    -funsigned-char -fno-pic -fno-strict-aliasing

if RENDERTYPESDL
AM_CPPFLAGS += $(SDL_CFLAGS)
if HAVE_GTK2
AM_CPPFLAGS += $(GTK_CFLAGS)
endif
endif


noinst_LTLIBRARIES = libengine-base.la
libengine_base_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/../libsquish
libengine_base_la_SOURCES = a-c.c baselayer.c cache1d.c compat.c crc32.c defs.c \
		            engine.c polymost.c polymosttex.c polymosttex-squish.cc \
		            polymosttexcache.c hightile.c mdsprite.c glbuild.c \
		            kplib.c lzf_c.c lzf_d.c md4.c mmulti.c osd.c pragmas.c \
		            scriptfile.c textfont.c smalltextfont.c

if !NOASM
libengine_base_la_SOURCES += a.nasm
endif

if RENDERTYPEWIN
libengine_base_la_CPPFLAGS += $(DIRECTX_SDK_CPPFLAGS)
libengine_base_la_SOURCES += winlayer.c
endif

if RENDERTYPESDL
libengine_base_la_SOURCES += sdlayer.c

if HAVE_GTK2
libengine_base_la_SOURCES += gtkbits.c dynamicgtk.c
endif
endif


noinst_LIBRARIES    = libeditor.a
libeditor_a_SOURCES = build.c config.c

if RENDERTYPEWIN
libeditor_a_SOURCES += startwin.editor.c
endif
if RENDERTYPESDL
if HAVE_GTK2
libeditor_a_SOURCES += startgtk.editor.c
endif
endif


# combine the base engine with its supporting libraries into one unit for linking
noinst_LTLIBRARIES += libengine.la
libengine_la_SOURCES =
noinst_EXTRA_libengine_la_SOURCES = dummy.cxx
libengine_la_LIBADD = libengine-base.la ../libsquish/libsquish.la
