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

void wmgtk_setapptitle(const char *name)
{
	if (!gtkenabled) return;
	g_set_application_name(name);
}

int wmgtk_msgbox(char *name, char *msg)
{
	GtkWidget *dialog;

	if (!gtkenabled) return -1;

	dialog = gtk_message_dialog_new(NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			"%s", msg);
	gtk_window_set_title(GTK_WINDOW(dialog), name);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	return 1;
}

int wmgtk_ynbox(char *name, char *msg)
{
	int r;
	GtkWidget *dialog;

	if (!gtkenabled) return -1;

	dialog = gtk_message_dialog_new(NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_YES_NO,
			"%s", msg);
	gtk_window_set_title(GTK_WINDOW(dialog), name);
	r = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	if (r == GTK_RESPONSE_YES) return 1;

	return 0;
}

int wmgtk_filechooser(const char *initialdir, const char *initialfile, const char *type, int foropen, char **choice)
{
	GtkWidget *dialog;
	GtkFileChooserAction action;
	const char *title;
	gint r;

	GtkFileFilter *filter;
	char typepat[20];

	(void)initialdir;

	*choice = NULL;
	if (!gtkenabled) return -1;

	if (foropen) {
		action = GTK_FILE_CHOOSER_ACTION_OPEN;
		title = "_Open";
	} else {
		action = GTK_FILE_CHOOSER_ACTION_SAVE;
		title = "_Save";
	}
	dialog = gtk_file_chooser_dialog_new(title + 1, NULL, action,
		"_Cancel", GTK_RESPONSE_CANCEL,
		title, GTK_RESPONSE_ACCEPT,
		NULL);

	if (!foropen && initialfile) {
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), initialfile);
	}

	filter = gtk_file_filter_new();
	sprintf(typepat, "*.%s", type);
	gtk_file_filter_add_pattern(filter, typepat);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);

	r = gtk_dialog_run(GTK_DIALOG(dialog));
	if (r == GTK_RESPONSE_ACCEPT) {
		GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
		char *fn = gtk_file_chooser_get_filename(chooser);
		if (fn) {
			*choice = strdup(fn);
			g_free(fn);
		}
	}
	gtk_widget_destroy(dialog);

	return r == GTK_RESPONSE_ACCEPT ? 1 : 0;
}

void wmgtk_init(int *argc, char ***argv)
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

void wmgtk_exit(void)
{
	if (!gtkenabled) {
		return;
	}

    startgtk_unregister_resource();
}

int wmgtk_idle(void *ptr)
{
	(void)ptr;

	if (!gtkenabled) return 0;
	gtk_main_iteration_do(FALSE);
	return 0;
}
