#ifdef HAVE_GTK2
#include <gtk/gtk.h>
#endif

int wm_msgbox(char *name, char *fmt, ...)
{
	char buf[1000];
	va_list va;

	va_start(va,fmt);
	vsprintf(buf,fmt,va);
	va_end(va);

#ifdef HAVE_GTK2
	{
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new(NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_OK,
				buf);
		gtk_window_set_title(GTK_WINDOW(dialog), name);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}
#else
	puts(buf);
	getchar();
#endif
	return 0;
}

int wm_ynbox(char *name, char *fmt, ...)
{
	char buf[1000];
	va_list va;
	int r;

	va_start(va,fmt);
	vsprintf(buf,fmt,va);
	va_end(va);

#ifdef HAVE_GTK2
	{
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new(NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_YES_NO,
				buf);
		gtk_window_set_title(GTK_WINDOW(dialog), name);
		r = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		if (r == GTK_RESPONSE_YES) return 1;
	}
#else
	{
		char c;
		puts(buf);
		do c = getchar(); while (c != 'Y' && c != 'y' && c != 'N' && c != 'n');
		if (c == 'Y' || c == 'y') return 1;
	}
#endif
	return 0;
}

