#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(3,4,0) != TRUE
# error Gtk+ 3.4 or higher required.
#endif

#include "build.h"
#include "baselayer.h"

// Copied from a glib-compile-resource generated header.
extern GResource *startgtk_get_resource (void);
extern void startgtk_register_resource (void);
extern void startgtk_unregister_resource (void);


int gtkenabled = 0;

int gtkbuild_msgbox(char *name, char *msg)
{
	GtkWidget *dialog;

	if (!gtkenabled) return -1;

	dialog = gtk_message_dialog_new(NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			msg);
	gtk_window_set_title(GTK_WINDOW(dialog), name);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	return 1;
}

int gtkbuild_ynbox(char *name, char *msg)
{
	int r;
	GtkWidget *dialog;

	if (!gtkenabled) return -1;

	dialog = gtk_message_dialog_new(NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_YES_NO,
			msg);
	gtk_window_set_title(GTK_WINDOW(dialog), name);
	r = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	if (r == GTK_RESPONSE_YES) return 1;

	return 0;
}

void gtkbuild_init(int *argc, char ***argv)
{
	GdkPixbuf *appicon = NULL;
	GError *error = NULL;

	gtkenabled = gtk_init_check(argc, argv);
	if (!gtkenabled) {
		return;
	}

	startgtk_register_resource();

	appicon = gdk_pixbuf_new_from_resource("/appicon.png", &error);
	if (!appicon) {
        buildprintf("gdk_pixbuf_new_from_resource error: %s\n", error->message);
	} else {
		gtk_window_set_default_icon(appicon);
		g_object_unref(G_OBJECT(appicon));
	}
}

void gtkbuild_exit(void)
{
	if (!gtkenabled) {
		return;
	}

    startgtk_unregister_resource();
}
