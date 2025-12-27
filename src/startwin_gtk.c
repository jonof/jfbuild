#include "build.h"

#include <gtk/gtk.h>

#include "baselayer.h"
#include "build.h"
#include "startwin.h"
#include "startwin_priv.h"

#define TAB_CONFIG 0
#define TAB_GAME 1
#define TAB_MESSAGES 2

extern int gtkenabled;

static GtkWindow *startwin;
static struct {
    GtkWidget *tabs;
    GtkWidget *configbox;
    GtkWidget *configtab;
    GtkWidget *gamebox;
    GtkWidget *gametab;
    GtkWidget *messagesbox;
    GtkWidget *messagestab;

    GtkContainer *featurevideo;
    GtkContainer *featureeditor;
    GtkContainer *featureaudio;
    GtkContainer *featureinput;
    GtkContainer *featurenetwork;

    GtkWidget *vmode2dcombo;
    GtkListStore *vmode2dlist;
    GtkWidget *vmode3dcombo;
    GtkListStore *vmode3dlist;
    GtkWidget *fullscreencheck;
    GtkWidget *displaycombo;
    GtkListStore *displaylist;

    GtkWidget *soundqualitycombo;
    GtkListStore *soundqualitylist;

    GtkWidget *usemousecheck;
    GtkWidget *usejoystickcheck;

    GtkWidget *singleplayerbutton;
    GtkWidget *joinmultibutton;
    GtkWidget *hostmultibutton;
    GtkWidget *hostfield;
    GtkWidget *numplayersspin;
    GtkAdjustment *numplayersadjustment;

    GtkWidget *gametable;
    GtkListStore *gamelist;
    GtkWidget *chooseimportbutton;
    GtkWidget *importinfobutton;

    GtkWidget *messagestext;

    GtkWidget *alwaysshowcheck;
    GtkWidget *startbutton;
    GtkWidget *cancelbutton;

    GtkWindow *importstatuswindow;
    GtkWidget *importstatustext;
    GtkWidget *importstatuscancelbutton;
} controls;

static gboolean startwinloop = FALSE;
static gboolean quiteventonclose = FALSE;
static gboolean ignoresignals = FALSE;
static int retval = -1;

// -- SUPPORT FUNCTIONS -------------------------------------------------------

static GObject * get_and_connect_signal(GtkBuilder *builder, const char *name, const char *signal_name, GCallback handler)
{
    GObject *object;

    object = gtk_builder_get_object(builder, name);
    if (!object) {
        debugprintf("%s: %s not found\n", __func__, name);
        return 0;
    }
    g_signal_connect(object, signal_name, handler, NULL);
    return object;
}

static void populate_video_modes(gboolean firsttime)
{
    int i, mode2d = -1, mode3d = -1;
    int xdim3d = 0, ydim3d = 0, bitspp = 0, display = 0, fullsc = 0;
    int xdim2d = 0, ydim2d = 0;
    char modestr[64];
    int cd[] = { 32, 24, 16, 15, 8, 0 };
    GtkTreeIter iter;

    if (firsttime) {
        getvalidmodes();
        if (startwin_settings.features.video) {
            xdim3d = startwin_settings.video.xdim;
            ydim3d = startwin_settings.video.ydim;
            bitspp = startwin_settings.video.bpp;
            fullsc = startwin_settings.video.fullscreen;
            display = min(displaycnt-1, max(0, startwin_settings.video.display));

            gtk_list_store_clear(controls.displaylist);
            for (i = 0; i < displaycnt; i++) {
                snprintf(modestr, sizeof(modestr), "Display %d \xe2\x80\x93 %s", i, getdisplayname(i));
                gtk_list_store_insert_with_values(controls.displaylist, &iter, -1, 0, modestr, 1, i, -1);
            }
            if (displaycnt < 2) gtk_widget_set_visible(controls.displaycombo, FALSE);
        }
        if (startwin_settings.features.editor) {
            xdim2d = startwin_settings.editor.xdim;
            ydim2d = startwin_settings.editor.ydim;
        }
    } else {
        // Read back the current resolution information selected in the combobox.
        fullsc = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.fullscreencheck));
        if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(controls.displaycombo), &iter)) {
            gtk_tree_model_get(GTK_TREE_MODEL(controls.displaylist), &iter, 1 /*index*/, &display, -1);
        }
        if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(controls.vmode3dcombo), &iter)) {
            gtk_tree_model_get(GTK_TREE_MODEL(controls.vmode3dlist), &iter, 1 /*index*/, &mode3d, -1);
        }
        if (mode3d >= 0) {
            xdim3d = validmode[mode3d].xdim;
            ydim3d = validmode[mode3d].ydim;
            bitspp = validmode[mode3d].bpp;
        }
        if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(controls.vmode2dcombo), &iter)) {
            gtk_tree_model_get(GTK_TREE_MODEL(controls.vmode2dlist), &iter, 1 /*index*/, &mode2d, -1);
        }
        if (mode2d >= 0) {
            xdim2d = validmode[mode2d].xdim;
            ydim2d = validmode[mode2d].ydim;
        }
    }

    // Find an ideal match.
    mode3d = checkvideomode(&xdim3d, &ydim3d, bitspp, SETGAMEMODE_FULLSCREEN(display, fullsc), 1);
    mode2d = checkvideomode(&xdim2d, &ydim2d, 8, SETGAMEMODE_FULLSCREEN(display, fullsc), 1);
    if (mode2d < 0) mode2d = 0;
    for (i=0; mode3d < 0 && cd[i]; i++) {
        mode3d = checkvideomode(&xdim3d, &ydim3d, cd[i], SETGAMEMODE_FULLSCREEN(display, fullsc), 1);
    }
    if (mode3d < 0) mode3d = 0;
    fullsc = validmode[mode3d].fs;
    display = validmode[mode3d].display;

    // Repopulate the list.
    ignoresignals = TRUE;
    gtk_list_store_clear(controls.vmode3dlist);
    gtk_list_store_clear(controls.vmode2dlist);
    for (i = 0; i < validmodecnt; i++) {
        if (validmode[i].fs != fullsc) continue;
        if (validmode[i].display != display) continue;

        sprintf(modestr, "%d \xc3\x97 %d %d-bpp",
                validmode[i].xdim, validmode[i].ydim, validmode[i].bpp);
        gtk_list_store_insert_with_values(controls.vmode3dlist,
            &iter, -1,
            0, modestr, 1, i, -1);
        if (i == mode3d) {
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(controls.vmode3dcombo), &iter);
        }

        if (validmode[i].bpp == 8 && validmode[i].xdim >= 640 && validmode[i].ydim >= 480) {
            sprintf(modestr, "%d \xc3\x97 %d",
                validmode[i].xdim, validmode[i].ydim);
            gtk_list_store_insert_with_values(controls.vmode2dlist,
                &iter, -1,
                0, modestr, 1, i, -1);
            if (i == mode2d) {
                gtk_combo_box_set_active_iter(GTK_COMBO_BOX(controls.vmode2dcombo), &iter);
            }
        }
    }

    for (gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(controls.displaylist), &iter);
            (fullsc || firsttime) && valid; valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(controls.displaylist), &iter)) {
        gint index;
        gtk_tree_model_get(GTK_TREE_MODEL(controls.displaylist), &iter, 1, &index, -1);
        if (index == validmode[mode3d].display) {
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(controls.displaycombo), &iter);
            break;
        }
    }
    gtk_widget_set_sensitive(controls.displaycombo, validmode[mode3d].fs);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.fullscreencheck), validmode[mode3d].fs);
    ignoresignals = FALSE;
}

static void populate_sound_quality(gboolean firsttime)
{
    int i, curidx = -1;
    int samplerate = 0, bitspersample = 0, channels = 0;
    char modestr[64];
    GtkTreeIter iter;

    if (firsttime) {
        samplerate = startwin_settings.audio.samplerate;
        bitspersample = startwin_settings.audio.bitspersample;
        channels = startwin_settings.audio.channels;
    } else {
        if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(controls.soundqualitycombo), &iter)) {
            gtk_tree_model_get(GTK_TREE_MODEL(controls.soundqualitylist), &iter, 1 /*index*/, &curidx, -1);
        }
    }

    gtk_list_store_clear(controls.soundqualitylist);
    for (i = 0; startwin_soundqualities[i].frequency; i++) {
        if ((samplerate == startwin_soundqualities[i].frequency &&
                bitspersample == startwin_soundqualities[i].samplesize &&
                channels == startwin_soundqualities[i].channels) || curidx < 0) curidx = i;
        sprintf(modestr, "%d kHz, %d-bit, %s",
            startwin_soundqualities[i].frequency / 1000,
            startwin_soundqualities[i].samplesize,
            startwin_soundqualities[i].channels == 1 ? "Mono" : "Stereo");
        gtk_list_store_insert_with_values(controls.soundqualitylist,
            &iter, -1,
            0, modestr, 1, i, -1);
        if (i == curidx) {
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(controls.soundqualitycombo), &iter);
        }
    }
}

static void populate_game_list(gboolean firsttime)
{
    const struct startwin_datasetfound * datasetp;
    GtkTreeIter iter, anyiter;
    GtkTreeSelection *treesel;
    int sel = -1, oldid = -1, any = 0;

    treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls.gametable));

    if (!firsttime) {
        if (gtk_tree_selection_get_selected(treesel, NULL, &iter)) {
            gtk_tree_model_get(GTK_TREE_MODEL(controls.gamelist), &iter, 2 /*id*/, &oldid, -1);
        }
    }

    gtk_list_store_clear(controls.gamelist);

    for (datasetp = startwin_scan_gamedata(); datasetp; datasetp = datasetp->next) {
        const char *filename = "";
        const char *gamename;

        if (!datasetp->complete) continue;

        gamename = datasetp->dataset->name;

        const struct startwin_datasetfoundfile *grp = startwin_find_dataset_group(datasetp);
        if (grp) filename = grp->name;

        gtk_list_store_insert_with_values(controls.gamelist,
            &iter, -1,
            0, gamename, 1, filename, 2, datasetp->dataset->id, -1);
        if (!any) {
            any = 1;
            anyiter = iter;
        }

        if (oldid == datasetp->dataset->id) sel = 1;
        else if (sel < 0 && datasetp->dataset->id == startwin_settings.game.gamedataid) sel = 1;
        if (sel == 1) {
            gtk_tree_selection_select_iter(treesel, &iter);
            sel = 0;
        }
    }
    if (sel < 0 && any) gtk_tree_selection_select_iter(treesel, &anyiter);
}

static void configure(gboolean firsttime)
{
    if (firsttime) {
        g_object_ref(G_OBJECT(controls.configbox));
        g_object_ref(G_OBJECT(controls.configtab));
        g_object_ref(G_OBJECT(controls.gamebox));
        g_object_ref(G_OBJECT(controls.gametab));
        gtk_notebook_remove_page(GTK_NOTEBOOK(controls.tabs), TAB_GAME);
        gtk_notebook_remove_page(GTK_NOTEBOOK(controls.tabs), TAB_CONFIG);
        return;
    }

    gtk_notebook_insert_page(GTK_NOTEBOOK(controls.tabs), controls.configbox, controls.configtab, TAB_CONFIG);
    const struct {
        GtkContainer *container;
        int enabled;
    } containers[] = {
        { controls.featurevideo,   startwin_settings.features.video   },
        { controls.featureeditor,  startwin_settings.features.editor  },
        { controls.featureaudio,   startwin_settings.features.audio   },
        { controls.featureinput,   startwin_settings.features.input   },
        { controls.featurenetwork, startwin_settings.features.network },
    };
    for (size_t i=0; i<Barraylen(containers); i++) {
        gtk_widget_set_visible(GTK_WIDGET(containers[i].container), containers[i].enabled);
    }

    if (startwin_settings.features.game) {
        gtk_notebook_insert_page(GTK_NOTEBOOK(controls.tabs), controls.gamebox, controls.gametab, TAB_GAME);
    }

    g_object_unref(G_OBJECT(controls.configbox));
    g_object_unref(G_OBJECT(controls.configtab));
    g_object_unref(G_OBJECT(controls.gamebox));
    g_object_unref(G_OBJECT(controls.gametab));
}

static void setup_config_mode(void)
{
    gtk_widget_set_sensitive(controls.configbox, TRUE);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.alwaysshowcheck), startwin_settings.alwaysshow);
    gtk_widget_set_sensitive(controls.alwaysshowcheck, TRUE);

    if (startwin_settings.features.video || startwin_settings.features.editor) {
        populate_video_modes(TRUE);
    }
    if (startwin_settings.features.audio) {
        populate_sound_quality(TRUE);
    }
    if (startwin_settings.features.input) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.usemousecheck), startwin_settings.input.mouse);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.usejoystickcheck), startwin_settings.input.controller);
    }
    if (startwin_settings.features.network) {
        if (!startwin_settings.network.netoverride) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.singleplayerbutton), TRUE);

            gtk_spin_button_set_range(GTK_SPIN_BUTTON(controls.numplayersspin), 2, MAXPLAYERS);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(controls.numplayersspin), 2.0);
        }
    }
    if (startwin_settings.features.game) {
        populate_game_list(TRUE);
        gtk_widget_set_sensitive(controls.gamebox, TRUE);
    }

    if (startwin_settings.features.game && !startwin_settings.game.gamedataid) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(controls.tabs), TAB_GAME);
    } else {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(controls.tabs), TAB_CONFIG);
    }

    gtk_widget_set_sensitive(controls.cancelbutton, TRUE);
    gtk_widget_set_sensitive(controls.startbutton, TRUE);
}

static void setup_messages_mode(gboolean allowcancel)
{
    gtk_notebook_set_current_page(GTK_NOTEBOOK(controls.tabs), TAB_MESSAGES);

    gtk_widget_set_sensitive(controls.configbox, FALSE);
    if (startwin_settings.features.game) gtk_widget_set_sensitive(controls.gamebox, FALSE);

    gtk_widget_set_sensitive(controls.alwaysshowcheck, FALSE);
    gtk_widget_set_sensitive(controls.cancelbutton, allowcancel);
    gtk_widget_set_sensitive(controls.startbutton, FALSE);
}

// -- EVENT CALLBACKS AND CREATION STUFF --------------------------------------

static void on_fullscreencheck_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
    (void)togglebutton; (void)user_data;

    if (!ignoresignals) populate_video_modes(FALSE);
}

static void on_displaycombo_changed(GtkComboBox *combobox, gpointer user_data)
{
    (void)combobox; (void)user_data;

    if (!ignoresignals) populate_video_modes(FALSE);
}

static void on_multiplayerradio_toggled(GtkRadioButton *radiobutton, gpointer user_data)
{
    (void)radiobutton; (void)user_data;

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
    int mode;
    GtkTreeIter iter;
    GtkTreeSelection *sel;

    (void)button; (void)user_data;

    if (startwin_settings.features.video) {
        mode = -1;
        if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(controls.vmode3dcombo), &iter)) {
            gtk_tree_model_get(GTK_TREE_MODEL(controls.vmode3dlist), &iter, 1 /*index*/, &mode, -1);
        }
        if (mode >= 0) {
            startwin_settings.video.xdim = validmode[mode].xdim;
            startwin_settings.video.ydim = validmode[mode].ydim;
            startwin_settings.video.bpp = validmode[mode].bpp;
            startwin_settings.video.fullscreen = validmode[mode].fs;
            startwin_settings.video.display = validmode[mode].display;
        }
    }
    if (startwin_settings.features.editor) {
        mode = -1;
        if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(controls.vmode2dcombo), &iter)) {
            gtk_tree_model_get(GTK_TREE_MODEL(controls.vmode2dlist), &iter, 1 /*index*/, &mode, -1);
        }
        if (mode >= 0) {
            startwin_settings.editor.xdim = validmode[mode].xdim;
            startwin_settings.editor.ydim = validmode[mode].ydim;
        }
    }
    if (startwin_settings.features.input) {
        startwin_settings.input.mouse = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.usemousecheck));
        startwin_settings.input.controller = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.usejoystickcheck));
    }
    if (startwin_settings.features.audio) {
        mode = -1;
        if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(controls.soundqualitycombo), &iter)) {
            gtk_tree_model_get(GTK_TREE_MODEL(controls.soundqualitylist), &iter, 1 /*index*/, &mode, -1);
        }
        if (mode >= 0) {
            startwin_settings.audio.samplerate = startwin_soundqualities[mode].frequency;
            startwin_settings.audio.bitspersample = startwin_soundqualities[mode].samplesize;
            startwin_settings.audio.channels = startwin_soundqualities[mode].channels;
        }
    }
    if (startwin_settings.features.network) {
        startwin_settings.network.numplayers = 0;
        startwin_settings.network.joinhost = NULL;
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.singleplayerbutton))) {
            startwin_settings.network.numplayers = 1;
        } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.joinmultibutton))) {
            startwin_settings.network.numplayers = 2;
            startwin_settings.network.joinhost = strdup(gtk_entry_get_text(GTK_ENTRY(controls.hostfield)));
        } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.hostmultibutton))) {
            startwin_settings.network.numplayers = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls.numplayersspin));
        }
    }
    if (startwin_settings.features.game) {
        startwin_settings.game.gamedataid = 0;
        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls.gametable));
        if (gtk_tree_selection_get_selected(sel, NULL, &iter)) {
            gtk_tree_model_get(GTK_TREE_MODEL(controls.gamelist), &iter, 2 /*id*/, &startwin_settings.game.gamedataid, -1);
        }
    }

    startwin_settings.alwaysshow = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.alwaysshowcheck));

    startwinloop = FALSE;   // Break the loop.
    retval = STARTWIN_RUN;
}

static gboolean on_startwin_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    (void)widget; (void)event; (void)user_data;

    startwinloop = FALSE;   // Break the loop.
    retval = STARTWIN_CANCEL;
    quitevent = quitevent || quiteventonclose;
    return TRUE;    // FALSE would let the event go through. We want the game to decide when to close.
}

static void on_importstatus_cancelbutton_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    g_cancellable_cancel(G_CANCELLABLE(user_data));
}

static int set_importstatus_text(void *text)
{
    // Called in the main thread via g_main_context_invoke in the import thread.
    gtk_label_set_text(GTK_LABEL(controls.importstatustext), text);
    free(text);
    return 0;
}

static void import_progress(void *data, const char *path)
{
    // Called in the import thread.
    (void)data;
    g_main_context_invoke(NULL, set_importstatus_text, (gpointer)strdup(path));
}

static int import_cancelled(void *data)
{
    // Called in the import thread.
    return g_cancellable_is_cancelled(G_CANCELLABLE(data));
}

static void import_thread_func(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
    struct startwin_import_meta meta = {
        (void *)cancellable,
        0,
        import_progress,
        import_cancelled
    };
    (void)source_object;
    g_task_return_int(task, startwin_import_path((char *)task_data, &meta));
}

static void on_chooseimportbutton_clicked(GtkButton *button, gpointer user_data)
{
    GtkWidget *dialog;
    char *filename = NULL;

    (void)button; (void)user_data;

    dialog = gtk_file_chooser_dialog_new("Import game data", startwin,
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Import", GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), TRUE);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new("Select a folder to search.");
    gtk_widget_show(label);
    gtk_widget_set_margin_top(label, 7);
    gtk_widget_set_margin_bottom(label, 7);
    gtk_container_add(GTK_CONTAINER(content), label);
    gtk_box_reorder_child(GTK_BOX(content), label, 0);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        filename = gtk_file_chooser_get_filename(chooser);
    }
    gtk_widget_destroy(dialog);

    if (filename) {
        GTask *task = NULL;
        GError *err = NULL;
        GCancellable *cancellable = NULL;
        gulong clickhandlerid;

        cancellable = g_cancellable_new();
        task = g_task_new(NULL, cancellable, NULL, NULL);
        g_task_set_check_cancellable(task, FALSE);

        // Pass the filename as task data.
        g_task_set_task_data(task, (gpointer)filename, NULL);

        // Connect the import status cancel button passing the GCancellable* as user data.
        clickhandlerid = g_signal_connect(controls.importstatuscancelbutton, "clicked",
            G_CALLBACK(on_importstatus_cancelbutton_clicked), (gpointer)cancellable);

        // Show the status window, run the import thread, and while it's running, pump the Gtk queue.
        gtk_widget_show(GTK_WIDGET(controls.importstatuswindow));
        g_task_run_in_thread(task, import_thread_func);
        while (!g_task_get_completed(task)) gtk_main_iteration();

        // Get the return value from the import thread, then hide the status window.
        if (g_task_propagate_int(task, &err) >= STARTWIN_IMPORT_OK) populate_game_list(FALSE);
        gtk_widget_hide(GTK_WIDGET(controls.importstatuswindow));

        // Disconnect the cancel button and clean up.
        g_signal_handler_disconnect(controls.importstatuscancelbutton, clickhandlerid);
        if (err) g_error_free(err);
        g_object_unref(cancellable);
        g_object_unref(task);

        g_free(filename);
    }
}

static void on_importinfobutton_clicked(GtkButton *button, gpointer user_data)
{
    GtkWidget *dialog;

    (void)button; (void)user_data;

    dialog = gtk_message_dialog_new(startwin, GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
        "%s", startwin_settings.game.moreinfobrief);
    gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(dialog),
        "%s", startwin_settings.game.moreinfodetail);
    if (startwin_settings.game.demourl) {
        gtk_dialog_add_button(GTK_DIALOG(dialog), "Download Demo", 1);
    }
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == 1) {
        g_app_info_launch_default_for_uri(startwin_settings.game.demourl, NULL, NULL);
    }
    gtk_widget_destroy(dialog);
}

static GtkWindow *create_window(void)
{
    GtkBuilder *builder = NULL;
    GError *error = NULL;
    GtkWidget *window = NULL;
    GtkLabel *label = NULL;
    GBytes *bytes = NULL;

    builder = gtk_builder_new();
    if (!builder) {
        goto fail;
    }
    if (!gtk_builder_add_from_resource(builder, "/startwin_gtk.glade", &error)) {
        debugprintf("%s: gtk_builder_add_from_resource error: %s\n", __func__, error->message);
        goto fail;
    }

    // Get the window widget.
    window = GTK_WIDGET(get_and_connect_signal(builder, "startwin",
        "delete-event", G_CALLBACK(on_startwin_delete_event)));
    if (!window) {
        goto fail;
    }

    // Set the app name and version from resources.
    if ((bytes = g_resources_lookup_data("/appname.txt", G_RESOURCE_LOOKUP_FLAGS_NONE, &error))) {
        char appname[256] = {0};
        gconstpointer data = g_bytes_get_data(bytes, NULL);
        if (data) sscanf((const char *)data, " %255[^\r\n]", appname);
        g_bytes_unref(bytes);
        if ((label = GTK_LABEL(gtk_builder_get_object(builder, "appname")))) {
            gtk_label_set_text(label, appname);
        }
    } else {
        debugprintf("%s: g_resources_lookup_data error: %s\n", __func__, error->message);
    }
    if ((bytes = g_resources_lookup_data("/website.txt", G_RESOURCE_LOOKUP_FLAGS_NONE, &error))) {
        char appwebsite[256] = {0};
        gconstpointer data = g_bytes_get_data(bytes, NULL);
        if (data) sscanf((const char *)data, " %255[^\r\n]", appwebsite);
        g_bytes_unref(bytes);
        if ((label = GTK_LABEL(gtk_builder_get_object(builder, "appwebsite")))) {
            gtk_label_set_markup(label, appwebsite);
        }
    } else {
        debugprintf("%s: g_resources_lookup_data error: %s\n", __func__, error->message);
    }
    if ((bytes = g_resources_lookup_data("/version.txt", G_RESOURCE_LOOKUP_FLAGS_NONE, &error))) {
        char appversion[256] = {0}, tmp[255];
        gconstpointer data = g_bytes_get_data(bytes, NULL);
        if (data) sscanf((const char *)data, " %255[^\r\n]", appversion);
        g_bytes_unref(bytes);
        if ((label = GTK_LABEL(gtk_builder_get_object(builder, "appversion")))) {
            snprintf(tmp, sizeof(tmp), "Version %s", appversion);
            gtk_label_set_text(label, tmp);
        }
    } else {
        debugprintf("%s: g_resources_lookup_data error: %s\n", __func__, error->message);
    }

    // Get the window widgets we need and wire them up as appropriate.
    controls.startbutton = GTK_WIDGET(get_and_connect_signal(builder, "startbutton",
        "clicked", G_CALLBACK(on_startbutton_clicked)));
    controls.cancelbutton = GTK_WIDGET(get_and_connect_signal(builder, "cancelbutton",
        "clicked", G_CALLBACK(on_cancelbutton_clicked)));
    controls.alwaysshowcheck = GTK_WIDGET(gtk_builder_get_object(builder, "alwaysshowcheck"));

    controls.tabs = GTK_WIDGET(gtk_builder_get_object(builder, "tabs"));
    controls.configbox = GTK_WIDGET(gtk_builder_get_object(builder, "configbox"));
    controls.configtab = GTK_WIDGET(gtk_builder_get_object(builder, "configtab"));
    controls.gamebox = GTK_WIDGET(gtk_builder_get_object(builder, "gamebox"));
    controls.gametab = GTK_WIDGET(gtk_builder_get_object(builder, "gametab"));
    controls.messagesbox = GTK_WIDGET(gtk_builder_get_object(builder, "messagesbox"));
    controls.messagestab = GTK_WIDGET(gtk_builder_get_object(builder, "messagestab"));

    controls.featurevideo = GTK_CONTAINER(gtk_builder_get_object(builder, "featurevideo"));
    controls.vmode3dcombo = GTK_WIDGET(gtk_builder_get_object(builder, "vmode3dcombo"));
    controls.vmode3dlist = GTK_LIST_STORE(gtk_builder_get_object(builder, "vmode3dlist"));
    controls.fullscreencheck = GTK_WIDGET(get_and_connect_signal(builder, "fullscreencheck",
        "toggled", G_CALLBACK(on_fullscreencheck_toggled)));
    controls.displaycombo = GTK_WIDGET(get_and_connect_signal(builder, "displaycombo",
        "changed", G_CALLBACK(on_displaycombo_changed)));
    controls.displaylist = GTK_LIST_STORE(gtk_builder_get_object(builder, "displaylist"));

    controls.featureeditor = GTK_CONTAINER(gtk_builder_get_object(builder, "featureeditor"));
    controls.vmode2dcombo = GTK_WIDGET(gtk_builder_get_object(builder, "vmode2dcombo"));
    controls.vmode2dlist = GTK_LIST_STORE(gtk_builder_get_object(builder, "vmode2dlist"));

    controls.featureaudio = GTK_CONTAINER(gtk_builder_get_object(builder, "featureaudio"));
    controls.soundqualitycombo = GTK_WIDGET(gtk_builder_get_object(builder, "soundqualitycombo"));
    controls.soundqualitylist = GTK_LIST_STORE(gtk_builder_get_object(builder, "soundqualitylist"));

    controls.featureinput = GTK_CONTAINER(gtk_builder_get_object(builder, "featureinput"));
    controls.usemousecheck = GTK_WIDGET(gtk_builder_get_object(builder, "usemousecheck"));
    controls.usejoystickcheck = GTK_WIDGET(gtk_builder_get_object(builder, "usejoystickcheck"));

    controls.featurenetwork = GTK_CONTAINER(gtk_builder_get_object(builder, "featurenetwork"));
    controls.singleplayerbutton = GTK_WIDGET(get_and_connect_signal(builder, "singleplayerbutton",
        "toggled", G_CALLBACK(on_multiplayerradio_toggled)));
    controls.joinmultibutton = GTK_WIDGET(get_and_connect_signal(builder, "joinmultibutton",
        "toggled", G_CALLBACK(on_multiplayerradio_toggled)));
    controls.hostmultibutton = GTK_WIDGET(get_and_connect_signal(builder, "hostmultibutton",
        "toggled", G_CALLBACK(on_multiplayerradio_toggled)));
    controls.hostfield = GTK_WIDGET(gtk_builder_get_object(builder, "hostfield"));
    controls.numplayersspin = GTK_WIDGET(gtk_builder_get_object(builder, "numplayersspin"));
    controls.numplayersadjustment = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "numplayersadjustment"));

    controls.gametable = GTK_WIDGET(gtk_builder_get_object(builder, "gametable"));
    controls.gamelist = GTK_LIST_STORE(gtk_builder_get_object(builder, "gamelist"));

    controls.messagestext = GTK_WIDGET(gtk_builder_get_object(builder, "messagestext"));

    controls.chooseimportbutton = GTK_WIDGET(get_and_connect_signal(builder, "chooseimportbutton",
        "clicked", G_CALLBACK(on_chooseimportbutton_clicked)));
    controls.importinfobutton = GTK_WIDGET(get_and_connect_signal(builder, "importinfobutton",
        "clicked", G_CALLBACK(on_importinfobutton_clicked)));

    controls.importstatuswindow = GTK_WINDOW(gtk_builder_get_object(builder, "importstatuswindow"));
    controls.importstatustext = GTK_WIDGET(gtk_builder_get_object(builder, "importstatustext"));
    controls.importstatuscancelbutton = GTK_WIDGET(gtk_builder_get_object(builder, "importstatuscancelbutton"));

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
    configure(TRUE);
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

static gboolean startwin_puts_inner(gpointer str)
{
    GtkTextBuffer *textbuffer;
    GtkTextIter enditer;
    GtkTextMark *mark;
    const char *aptr, *bptr;

    textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(controls.messagestext));

    gtk_text_buffer_get_end_iter(textbuffer, &enditer);
    for (aptr = bptr = (const char *)str; *aptr != 0; ) {
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

    free(str);
    return FALSE;
}

int startwin_puts(const char *str)
{
    // Called in either the main thread or the import thread via buildprintf.
    if (!gtkenabled || !str) return 0;
    if (!startwin) return 1;

    g_main_context_invoke(NULL, startwin_puts_inner, (gpointer)strdup(str));

    return 0;
}

int startwin_settitle(const char *title)
{
    if (!gtkenabled) return 0;

    if (startwin) {
        gtk_window_set_title(startwin, title);
    }

    return 0;
}

int startwin_idle(void *s)
{
    (void)s;
    if (startwin) {
        while (gtk_events_pending()) gtk_main_iteration();
    }
    return 0;
}

int startwin_run(void)
{
    if (!gtkenabled || !startwin) return STARTWIN_RUN;

    configure(FALSE);
    setup_config_mode();
    startwinloop = TRUE;
    while (startwinloop) {
        gtk_main_iteration_do(TRUE);
    }
    setup_messages_mode(startwin_settings.features.network &&
        (startwin_settings.network.numplayers > 1));

    return retval;
}

