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

libengine_a_CPPFLAGS=-I$(srcdir)/../include $(optionflags)
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
libengine_a_CPPFLAGS += $(SDL_CFLAGS)
libengine_a_SOURCES += sdlayer.c

if HAVE_GTK2
libengine_a_CPPFLAGS += $(GTK_CFLAGS)
libengine_a_SOURCES += gtkbits.c dynamicgtk.c
endif
endif



libeditor_a_CPPFLAGS=-I$(srcdir)/../include $(optionflags)
libeditor_a_CFLAGS=-W -Wall -Wimplicit -Wno-char-subscripts -Wno-unused \
		   -funsigned-char -fno-pic -fno-strict-aliasing -m32
libeditor_a_SOURCES=build.c config.c
if RENDERTYPEWIN
libeditor_a_SOURCES += startwin.editor.c
endif
if RENDERTYPESDL
if HAVE_GTK2
libeditor_a_CPPFLAGS += $(GTK_CFLAGS)
libeditor_a_SOURCES += startgtk.editor.c
endif
endif

