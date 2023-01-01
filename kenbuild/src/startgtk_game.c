#include "build.h"

#include <gtk/gtk.h>

#include "baselayer.h"
#include "game.h"

#define TAB_CONFIG 0
#define TAB_MESSAGES 1

extern int gtkenabled;

static GtkWindow *startwin;
static struct {
    GtkWidget *startbutton;
    GtkWidget *cancelbutton;

    GtkWidget *tabs;
    GtkWidget *configbox;
    GtkWidget *alwaysshowcheck;

    GtkWidget *messagestext;

    GtkWidget *vmode3dcombo;
    GtkListStore *vmode3dlist;
    GtkWidget *fullscreencheck;

    GtkWidget *singleplayerbutton;
    GtkWidget *joinmultibutton;
    GtkWidget *hostmultibutton;
    GtkWidget *hostfield;
    GtkWidget *numplayersspin;
    GtkAdjustment *numplayersadjustment;
} controls;

static gboolean startwinloop = FALSE;
static struct startwin_settings *settings;
static gboolean quiteventonclose = FALSE;
static int retval = -1;

// -- SUPPORT FUNCTIONS -------------------------------------------------------

static GObject * get_and_connect_signal(GtkBuilder *builder, const char *name, const char *signal_name, GCallback handler)
{
    GObject *object;

    object = gtk_builder_get_object(builder, name);
    if (!object) {
        buildprintf("gtk_builder_get_object: %s not found\n", name);
        return 0;
    }
    g_signal_connect(object, signal_name, handler, NULL);
    return object;
}

static void foreach_gtk_widget_set_sensitive(GtkWidget *widget, gpointer data)
{
    gtk_widget_set_sensitive(widget, (gboolean)(intptr_t)data);
}

static void populate_video_modes(gboolean firsttime)
{
    int i, mode3d = -1;
    int xdim = 0, ydim = 0, bpp = 0, fullscreen = 0;
    char modestr[64];
    int cd[] = { 32, 24, 16, 15, 8, 0 };
    GtkTreeIter iter;

    if (firsttime) {
        getvalidmodes();
        xdim = settings->xdim3d;
        ydim = settings->ydim3d;
        bpp  = settings->bpp3d;
        fullscreen = settings->fullscreen;
    } else {
        // Read back the current resolution information selected in the combobox.
        fullscreen = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.fullscreencheck));
        if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(controls.vmode3dcombo), &iter)) {
            gtk_tree_model_get(GTK_TREE_MODEL(controls.vmode3dlist), &iter, 1 /*index*/, &mode3d, -1);
        }
        if (mode3d >= 0) {
            xdim = validmode[mode3d].xdim;
            ydim = validmode[mode3d].ydim;
            bpp = validmode[mode3d].bpp;
        }
    }

    // Find an ideal match.
    mode3d = checkvideomode(&xdim, &ydim, bpp, fullscreen, 1);
    if (mode3d < 0) {
        for (i=0; cd[i]; ) { if (cd[i] >= bpp) i++; else break; }
        for ( ; cd[i]; i++) {
            mode3d = checkvideomode(&xdim, &ydim, cd[i], fullscreen, 1);
            if (mode3d < 0) continue;
            break;
        }
    }

    // Repopulate the list.
    gtk_list_store_clear(controls.vmode3dlist);
    for (i = 0; i < validmodecnt; i++) {
        if (validmode[i].fs != fullscreen) continue;

        sprintf(modestr, "%d \xc3\x97 %d %d-bpp",
            validmode[i].xdim, validmode[i].ydim, validmode[i].bpp);
        gtk_list_store_insert_with_values(controls.vmode3dlist,
            &iter, -1,
            0, modestr, 1, i, -1);
        if (i == mode3d) {
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(controls.vmode3dcombo), &iter);
        }
    }
}

static void set_settings(struct startwin_settings *thesettings)
{
    settings = thesettings;
}

static void setup_config_mode(void)
{
    gtk_notebook_set_current_page(GTK_NOTEBOOK(controls.tabs), TAB_CONFIG);

    // Enable all the controls on the Configuration page.
    gtk_container_foreach(GTK_CONTAINER(controls.configbox),
            foreach_gtk_widget_set_sensitive, (gpointer)TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.alwaysshowcheck), settings->forcesetup);
    gtk_widget_set_sensitive(controls.alwaysshowcheck, TRUE);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.fullscreencheck), settings->fullscreen);
    gtk_widget_set_sensitive(controls.fullscreencheck, TRUE);

    populate_video_modes(TRUE);
    gtk_widget_set_sensitive(controls.vmode3dcombo, TRUE);

    if (!settings->netoverride) {
        gtk_widget_set_sensitive(controls.singleplayerbutton, TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.singleplayerbutton), TRUE);

        gtk_widget_set_sensitive(controls.joinmultibutton, TRUE);
        gtk_widget_set_sensitive(controls.hostfield, FALSE);

        gtk_widget_set_sensitive(controls.hostmultibutton, TRUE);
        gtk_widget_set_sensitive(controls.numplayersspin, FALSE);
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(controls.numplayersspin), 2, MAXPLAYERS);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(controls.numplayersspin), 2.0);
    } else {
        gtk_widget_set_sensitive(controls.singleplayerbutton, FALSE);
        gtk_widget_set_sensitive(controls.joinmultibutton, FALSE);
        gtk_widget_set_sensitive(controls.hostfield, FALSE);
        gtk_widget_set_sensitive(controls.hostmultibutton, FALSE);
        gtk_widget_set_sensitive(controls.numplayersspin, FALSE);
    }

    gtk_widget_set_sensitive(controls.cancelbutton, TRUE);
    gtk_widget_set_sensitive(controls.startbutton, TRUE);
}

static void setup_messages_mode(gboolean allowcancel)
{
    gtk_notebook_set_current_page(GTK_NOTEBOOK(controls.tabs), TAB_MESSAGES);

    // Disable all the controls on the Configuration page.
    gtk_container_foreach(GTK_CONTAINER(controls.configbox),
            foreach_gtk_widget_set_sensitive, (gpointer)FALSE);
    gtk_widget_set_sensitive(controls.alwaysshowcheck, FALSE);

    gtk_widget_set_sensitive(controls.cancelbutton, allowcancel);
    gtk_widget_set_sensitive(controls.startbutton, FALSE);
}

// -- EVENT CALLBACKS AND CREATION STUFF --------------------------------------

static void on_fullscreencheck_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
    (void)togglebutton; (void)user_data;

    populate_video_modes(FALSE);
}

static void on_multiplayerradio_toggled(GtkRadioButton *radiobutton, gpointer user_data)
{
    (void)radiobutton; (void)user_data;

    //gboolean singleactive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.singleplayerbutton));
    gboolean joinmultiactive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.joinmultibutton));
    gboolean hostmultiactive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.hostmultibutton));

    gtk_widget_set_sensitive(controls.hostfield, joinmultiactive);
    gtk_widget_set_sensitive(controls.numplayersspin, hostmultiactive);
}

static void on_cancelbutton_clicked(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;

    startwinloop = FALSE;   // Break the loop.
    retval = STARTWIN_CANCEL;
    quitevent = quitevent || quiteventonclose;
}

static void on_startbutton_clicked(GtkButton *button, gpointer user_data)
{
    int mode = -1;
    GtkTreeIter iter;

    (void)button; (void)user_data;

    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(controls.vmode3dcombo), &iter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(controls.vmode3dlist), &iter, 1 /*index*/, &mode, -1);
    }
    if (mode >= 0) {
        settings->xdim3d = validmode[mode].xdim;
        settings->ydim3d = validmode[mode].ydim;
        settings->bpp3d = validmode[mode].bpp;
        settings->fullscreen = validmode[mode].fs;
    }

    settings->numplayers = 0;
    settings->joinhost = NULL;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.singleplayerbutton))) {
        settings->numplayers = 1;
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.joinmultibutton))) {
        settings->numplayers = 2;
        settings->joinhost = strdup(gtk_entry_get_text(GTK_ENTRY(controls.hostfield)));
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.hostmultibutton))) {
        settings->numplayers = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls.numplayersspin));
    }

    settings->forcesetup = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.alwaysshowcheck));

    startwinloop = FALSE;   // Break the loop.
    retval = STARTWIN_RUN;
}

static gboolean on_startgtk_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    (void)widget; (void)event; (void)user_data;

    startwinloop = FALSE;   // Break the loop.
    retval = STARTWIN_CANCEL;
    quitevent = quitevent || quiteventonclose;
    return TRUE;    // FALSE would let the event go through. We want the game to decide when to close.
}

static GtkWindow *create_window(void)
{
    GtkBuilder *builder = NULL;
    GError *error = NULL;
    GtkWidget *window = NULL;
    GtkImage *appicon = NULL;

    builder = gtk_builder_new();
    if (!builder) {
        goto fail;
    }
    if (!gtk_builder_add_from_resource(builder, "/startgtk.glade", &error)) {
        buildprintf("gtk_builder_add_from_resource error: %s\n", error->message);
        goto fail;
    }

    // Get the window widget.
    window = GTK_WIDGET(get_and_connect_signal(builder, "startgtk",
        "delete-event", G_CALLBACK(on_startgtk_delete_event)));
    if (!window) {
        goto fail;
    }

    // Set the appicon image.
    appicon = GTK_IMAGE(gtk_builder_get_object(builder, "appicon"));
    if (appicon) {
        gtk_image_set_from_resource(appicon, "/appicon.png");
    }

    // Get the window widgets we need and wire them up as appropriate.
    controls.startbutton = GTK_WIDGET(get_and_connect_signal(builder, "startbutton",
        "clicked", G_CALLBACK(on_startbutton_clicked)));
    controls.cancelbutton = GTK_WIDGET(get_and_connect_signal(builder, "cancelbutton",
        "clicked", G_CALLBACK(on_cancelbutton_clicked)));

    controls.tabs = GTK_WIDGET(gtk_builder_get_object(builder, "tabs"));
    controls.configbox = GTK_WIDGET(gtk_builder_get_object(builder, "configbox"));
    controls.alwaysshowcheck = GTK_WIDGET(gtk_builder_get_object(builder, "alwaysshowcheck"));

    controls.messagestext = GTK_WIDGET(gtk_builder_get_object(builder, "messagestext"));

    controls.vmode3dcombo = GTK_WIDGET(gtk_builder_get_object(builder, "vmode3dcombo"));
    controls.vmode3dlist = GTK_LIST_STORE(gtk_builder_get_object(builder, "vmode3dlist"));
    controls.fullscreencheck = GTK_WIDGET(get_and_connect_signal(builder, "fullscreencheck",
        "toggled", G_CALLBACK(on_fullscreencheck_toggled)));

    controls.singleplayerbutton = GTK_WIDGET(get_and_connect_signal(builder, "singleplayerbutton",
        "toggled", G_CALLBACK(on_multiplayerradio_toggled)));
    controls.joinmultibutton = GTK_WIDGET(get_and_connect_signal(builder, "joinmultibutton",
        "toggled", G_CALLBACK(on_multiplayerradio_toggled)));
    controls.hostmultibutton = GTK_WIDGET(get_and_connect_signal(builder, "hostmultibutton",
        "toggled", G_CALLBACK(on_multiplayerradio_toggled)));
    controls.hostfield = GTK_WIDGET(gtk_builder_get_object(builder, "hostfield"));
    controls.numplayersspin = GTK_WIDGET(gtk_builder_get_object(builder, "numplayersspin"));
    controls.numplayersadjustment = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "numplayersadjustment"));

    g_object_unref(G_OBJECT(builder));

    return GTK_WINDOW(window);

fail:
    if (window) {
        gtk_widget_destroy(window);
    }
    if (builder) {
        g_object_unref(G_OBJECT(builder));
    }
    return 0;
}




// -- BUILD ENTRY POINTS ------------------------------------------------------

int startwin_open(void)
{
    if (!gtkenabled) return 0;
    if (startwin) return 1;

    startwin = create_window();
    if (!startwin) {
        return -1;
    }

    quiteventonclose = TRUE;
    setup_messages_mode(TRUE);
    gtk_widget_show_all(GTK_WIDGET(startwin));
    return 0;
}

int startwin_close(void)
{
    if (!gtkenabled) return 0;
    if (!startwin) return 1;

    quiteventonclose = FALSE;
    gtk_widget_destroy(GTK_WIDGET(startwin));
    startwin = NULL;

    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    return 0;
}

int startwin_puts(const char *str)
{
    GtkTextBuffer *textbuffer;
    GtkTextIter enditer;
    GtkTextMark *mark;
    const char *aptr, *bptr;

    if (!gtkenabled || !str) return 0;
    if (!startwin) return 1;

    textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(controls.messagestext));

    gtk_text_buffer_get_end_iter(textbuffer, &enditer);
    for (aptr = bptr = str; *aptr != 0; ) {
        switch (*bptr) {
            case '\b':
                if (bptr > aptr) {
                    // Insert any normal characters seen so far.
                    gtk_text_buffer_insert(textbuffer, &enditer, (const gchar *)aptr, (gint)(bptr-aptr)-1);
                }
                gtk_text_buffer_backspace(textbuffer, &enditer, FALSE, TRUE);
                aptr = ++bptr;
                break;
            case 0:
                if (bptr > aptr) {
                    gtk_text_buffer_insert(textbuffer, &enditer, (const gchar *)aptr, (gint)(bptr-aptr));
                }
                aptr = bptr;
                break;
            default:
                bptr++;
                break;
        }
    }

    mark = gtk_text_buffer_create_mark(textbuffer, NULL, &enditer, 1);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(controls.messagestext), mark, 0.0, FALSE, 0.0, 1.0);
    gtk_text_buffer_delete_mark(textbuffer, mark);

    return 0;
}

int startwin_settitle(const char *title)
{
    if (!gtkenabled) return 0;

    if (startwin) {
        gtk_window_set_title(startwin, title);
    }
    g_set_application_name(title);

    return 0;
}

int startwin_idle(void *s)
{
    (void)s;
    return 0;
}

int startwin_run(struct startwin_settings *settings)
{
    if (!gtkenabled || !startwin) return 0;

    set_settings(settings);
    setup_config_mode();
    startwinloop = TRUE;
    while (startwinloop) {
        gtk_main_iteration_do(TRUE);
    }
    setup_messages_mode(settings->numplayers > 1);
    set_settings(NULL);

    return retval;
}

