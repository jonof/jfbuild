// Common Start-up Window
// for the Build Engine

// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __startwin_h__
#define __startwin_h__

struct startwin_settings {
    struct {
        // Enable 3D mode options.
        unsigned video:1;

        // Enable 2D mode options.
        unsigned editor:1;

        // Enable sound quality options.
        unsigned audio:1;

        // Enable mouse and controller options.
        unsigned input:1;

        // Enable multiplayer options.
        unsigned network:1;

        // Enable game data management.
        unsigned game:1;
    } features;

    struct {
        int fullscreen;
        int display;
        int xdim;
        int ydim;
        int bpp;
    } video;

    struct {
        int xdim;
        int ydim;
    } editor;

    struct {
        int samplerate;
        int channels;
        int bitspersample;
    } audio;

    struct {
        int mouse;
        int controller;
    } input;

    struct {
        int numplayers;
        char *joinhost;
        int netoverride;
    } network;

    struct {
        // Dataset identifier of the selected game dataset.
        int gamedataid;

        // A null-pointer-terminated list of wildcards encompassing all the filespec files.
        const char **gamedatafilepatterns;

        // Specifications of game data sets.
        const struct startwin_dataset *gamedata;

        // URL where freely-distributable game data files can be downloaded, or NULL.
        const char *demourl;

        // Advisory text indicating where full-version data can be obtained.
        const char *moreinfobrief;
        const char *moreinfodetail;
    } game;

    int alwaysshow;
};

enum {
    STARTWIN_CANCEL = 0,
    STARTWIN_RUN = 1,
};

enum {
    STARTWIN_DATASET_UNIDENTIFIED = 0,
    STARTWIN_DATASET_EXTRA0 = 0x10000, // The first ID of extra GRP files being returned as extra datasets.
};

enum {
    STARTWIN_PRESENCE_OPTIONAL = 0,  // The file is optional.
    STARTWIN_PRESENCE_REQUIRED = 1,  // The file is required for a dataset to be complete.
    STARTWIN_PRESENCE_GROUP    = 2,  // The file is a required GRP file for passing to cache1d.
    STARTWIN_PRESENCE_EXCEPT   = 3,  // The file cannot exist for a dataset to be complete.
};

// Dataset specification ------------------------
struct startwin_dataset {
    const char *name; // Human name.
    int id;           // Dataset identifier.
    int type;         // Dataset type, meaning defined by the game, or STARTWIN_DATASET_UNIDENTIFIED.
    const struct startwin_datasetfilespec *filespec; // Terminate with a NULL 'file'.
};

struct startwin_datasetfilespec {
    const char *name;   // If presence is OPTIONAL or EXCEPT, can be a wildcard. Otherwise the size and crc members are necessary.
    const char *storename;  // An alternative unique filename for importing into where 'name' would conflict with other datasets.
    size_t size;
    unsigned crc;
    unsigned presence:2;    // See STARTWIN_PRESENCE_xxx values. Must be OPTIONAL if 'alternate' is 1.
    unsigned alternate:1;   // If 1 this entry is an alternate version of a non-alternate entry following it (with a 0 here).
};

extern struct startwin_settings startwin_settings; // Declared in the game and populated according to needs.

// Dataset discovery ----------------------------
struct startwin_datasetfoundfile {
    const struct startwin_datasetfilespec *filespec; // The dataset filespec the file matches to.
    const char *name; // The filename as it exists on-disk.
};
struct startwin_datasetfound {
    struct startwin_datasetfound *next;
    const struct startwin_dataset *dataset;
    unsigned complete:1;
    int numfiles; // Count of items in the files member.
    struct startwin_datasetfoundfile files[]; // All the presence-required files that were matched.
};

int startwin_run(void);

const struct startwin_datasetfound * startwin_scan_gamedata(void);
void startwin_free_gamedata(void);

const struct startwin_datasetfound * startwin_find_id(int id);
const struct startwin_datasetfound * startwin_find_type(int type);
const struct startwin_datasetfound * startwin_find_filename(const char *filename);
const struct startwin_datasetfoundfile * startwin_find_id_group(int id);
const struct startwin_datasetfoundfile * startwin_find_type_group(int type);
const struct startwin_datasetfoundfile * startwin_find_dataset_group(const struct startwin_datasetfound *dataset);

#endif // __startwin_h__
