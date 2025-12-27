// Common Start-up Window, internal API
// for the Build Engine

// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __startwin_priv_h__
#define __startwin_priv_h__

// Common set of sound quality parameters terminated by a 0 frequency.
extern const struct startwin_soundqualities {
    int frequency;
    int samplesize;
    int channels;
} startwin_soundqualities[];

int startwin_open(void);
int startwin_close(void);
int startwin_idle(void *);
int startwin_puts(const char *);
int startwin_settitle(const char *);

struct startwin_import_meta {
    // Opaque data pointer passed to callbacks.
    void *data;

    // State of cancellation preserved from the callback.
    int wascancelled;

    // Callback to report scan progress.
    void (*progress)(void *data, const char *path);

    // Callback to check whether the user requested cancellation.
    int (*cancelled)(void *data);
};

enum {
    STARTWIN_IMPORT_ERROR = -1,
    STARTWIN_IMPORT_OK = 0,         // Nothing good nor bad.
    STARTWIN_IMPORT_SKIPPED = 1,    // File was identified as interesting, but passed over.
    STARTWIN_IMPORT_COPIED = 2,     // File was imported.
};

int startwin_import_path(const char *path, struct startwin_import_meta *meta);

#endif // __startwin_priv_h__
