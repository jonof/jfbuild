// Common Start-up Window
// for the Build Engine

// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#include "compat.h"
#include "startwin.h"
#include "startwin_priv.h"
#include "cache1d.h"
#include "crc32.h"
#include "scriptfile.h"
#include "baselayer.h"

const struct startwin_soundqualities startwin_soundqualities[] = {
    { 48000, 16, 2 },
    { 44100, 16, 2 },
    { 22050, 16, 2 },
    { 11025, 16, 2 },
    { 22050, 8, 2 },
    { 11025, 8, 2 },
    { 22050, 8, 1 },
    { 11025, 8, 1 },
    { 0 },
};

// Extra file meta data stored in the 'user' member of 'files' items.
struct file_meta {
    size_t size;
    unsigned mtime;
    unsigned crc;
    int claimed;
};
static CACHE1D_FIND_REC *files;

// Loaded data file cache details.
struct cache_meta {
    const char *name;   // Note: points to a string in the 'cache' static global.
    size_t size;
    unsigned mtime;
    unsigned crc;
    int adhoc; // If nonzero this link in the list was allocated piecemeal.
    struct cache_meta *next;
};
static scriptfile *cache;
static struct cache_meta *cachemeta;
#define CACHEFILE "files.cache"

// Dataset identification results.
static struct startwin_datasetfound *datasetsfound;

extern void buildprintf(const char *fmt, ...);

static unsigned int crc32fh(int fh, struct startwin_import_meta *meta)
{
    unsigned int crc;
    unsigned char buf[4096];
    ssize_t br;

    crc32init(&crc);
    do {
        if (meta && (meta->wascancelled || meta->cancelled(meta->data))) { meta->wascancelled = 1; return 0; }
        br = read(fh, buf, sizeof(buf));
        if (br > 0) crc32block(&crc, buf, (int)br);
    } while (br == sizeof(buf));
    crc32finish(&crc);

    return crc;
}

enum {
    COPYFILE_OK = 0,
    COPYFILE_ERR_EXISTS = -1,
    COPYFILE_ERR_OPEN = -2,
    COPYFILE_ERR_SEEK = -3,
    COPYFILE_ERR_RW = -4,
    COPYFILE_ERR_CANCELLED = -5,
};

static int copy_file(int fh, size_t size, const char *fname, struct startwin_import_meta *meta)
{
    int ofh, rv = COPYFILE_OK;
    ssize_t b=0;
    char buf[4096];

    ofh = open(fname, O_WRONLY|O_BINARY|O_CREAT|O_EXCL, BS_IREAD|BS_IWRITE);
    if (ofh < 0) {
        if (errno == EEXIST) return COPYFILE_ERR_EXISTS;
        return COPYFILE_ERR_OPEN;
    }
    if (lseek(fh, 0, SEEK_SET) < 0) rv = COPYFILE_ERR_SEEK;
    else {
        do {
            if (meta->wascancelled || meta->cancelled(meta->data)) { meta->wascancelled = 1; rv = COPYFILE_ERR_CANCELLED; }
            else if ((b = read(fh, buf, sizeof(buf))) < 0) rv = COPYFILE_ERR_RW;
            else if (b > 0 && write(ofh, buf, b) != b) rv = COPYFILE_ERR_RW;
        } while (b == sizeof(buf) && rv == COPYFILE_OK);
    }
    close(ofh);
    if (rv == COPYFILE_OK && (off_t)size != lseek(fh, 0, SEEK_CUR)) rv = COPYFILE_ERR_RW;  // File not the expected length.
    if (rv != COPYFILE_OK) remove(fname);
    return rv;
}

static void load_cache(void)
{
    scriptfile *script;
    int numfiles = 0, numcache = 0, sane = 1;
    int fsize, fmtime, fcrc;
    char *fname;
    struct cache_meta *mp;

    if (cache || cachemeta) return;

    script = scriptfile_fromfile(CACHEFILE);
    if (!script) return;

    do {
        if (scriptfile_getnumber(script, &numcache)) { sane = 0; break; }
        if (!(cachemeta = (struct cache_meta *)calloc(numcache, sizeof(struct cache_meta)))) {
            debugprintf("startwin %s: could not allocate %d items\n", __func__, numcache);
            sane = 0;
            break;
        }

        while (!scriptfile_eof(script)) {
            if (scriptfile_getstring(script, &fname)) break;
            if (scriptfile_getnumber(script, &fsize)) break;
            if (scriptfile_getnumber(script, &fmtime)) break;
            if (scriptfile_getnumber(script, &fcrc)) break;

            if (numfiles<numcache) {
                mp = &cachemeta[numfiles];
                if (++numfiles < numcache) mp->next = &cachemeta[numfiles];
            } else {
                if (!(mp = (struct cache_meta *)calloc(1, sizeof(struct cache_meta)))) {
                    debugprintf("startwin %s: could not allocate 1 item\n", __func__);
                    sane = 0;
                    break;
                }
                mp->adhoc = 1;
                mp->next = cachemeta;
                cachemeta = mp;
                numfiles++;
            }
            mp->name = fname;
            mp->size = (unsigned)fsize;
            mp->mtime = (unsigned)fmtime;
            mp->crc = (unsigned)fcrc;
        }
    } while(0);

    if (sane) {
        cache = script;
        buildprintf("startwin: read %d cache entries\n", numfiles);
    } else {
        scriptfile_close(script);

        while (cachemeta && cachemeta->adhoc) {
            struct cache_meta *next = cachemeta->next;
            free(cachemeta);
            cachemeta = next;
        }
        free(cachemeta);
        cachemeta = NULL;
    }
}

static void write_cache(void)
{
    FILE *fp;
    CACHE1D_FIND_REC *fg;
    int numfiles = 0;

    fp = fopen(CACHEFILE, "wt");
    if (!fp) { debugprintf("startwin %s: could not open %s to write\n", __func__, CACHEFILE); return; }

    for (fg = files; fg; fg = fg->next) numfiles++;
    fprintf(fp, "%d\n", numfiles);

    for (fg = files; fg; fg = fg->next) {
        struct file_meta *filemeta = (struct file_meta *)fg->user;
        fprintf(fp, "\"%s\" %zu %d 0x%08x\n", fg->name, filemeta->size, (int)filemeta->mtime, filemeta->crc);
    }
    fclose(fp);
}

static int scan_files(void)
{
    // Prepare a list of every loose file of interest at the root of the virtual filesystem.
    files = klistpath("/", startwin_settings.game.gamedatafilepatterns,
        CACHE1D_FIND_FILE | CACHE1D_OPT_NOGRP | CACHE1D_OPT_NOZIP,
        sizeof(struct file_meta));
    return files != NULL;
}

static void analyse_files(void)
{
    CACHE1D_FIND_REC *file;
    int fh = -1;

    for (file = files; file; file = file->next) {
        struct file_meta *filemeta = (struct file_meta *)file->user;

        if (fh >= 0) close(fh);
        fh = openfrompath(file->name, BO_RDONLY|BO_BINARY, BS_IREAD);
        if (fh < 0) { debugprintf("startwin %s: could not open %s\n", __func__, file->name); continue; }

        struct Bstat st;
        if (fstat(fh, &st)) { debugprintf("startwin %s: could not stat %s\n", __func__, file->name); continue; }

        filemeta->size = st.st_size;
        filemeta->mtime = (unsigned)st.st_mtime;
        filemeta->crc = 0;

        struct cache_meta *cachep;
        for (cachep = cachemeta; cachep; cachep = cachep->next) {
            if (cachep->size == filemeta->size &&
                    cachep->mtime == filemeta->mtime &&
                    !strcmp(cachep->name, file->name))
                break;
        }
        if (cachep) filemeta->crc = cachep->crc;
        else {
            buildprintf("startwin: inspecting %s\n", file->name);
            filemeta->crc = crc32fh(fh, NULL);
        }
    }
    if (fh >= 0) close(fh);
}

static int identify_datasets(void)
{
    const struct startwin_dataset *datasetp;
    struct startwin_datasetfound *tail = NULL;

    // Match files to gamedata sets.
    for (datasetp = &startwin_settings.game.gamedata[0]; datasetp->name; datasetp++) {
        int numfiles = 0, except = 0;

        for (int i=0; datasetp->filespec[i].name; i++) {
            if (datasetp->filespec[i].presence == STARTWIN_PRESENCE_REQUIRED ||
                    datasetp->filespec[i].presence == STARTWIN_PRESENCE_GROUP)
                numfiles++;
        }

        // Allocate a startwin_datasetfound big enough to contain the presence-required files.
        struct startwin_datasetfound *newdatasetfound = (struct startwin_datasetfound *)calloc(1,
            sizeof(struct startwin_datasetfound) + numfiles * sizeof(struct startwin_datasetfoundfile));
        if (!newdatasetfound) { debugprintf("startwin %s: error allocating a %d-file dataset\n", __func__, numfiles); return 0; }

        newdatasetfound->dataset = datasetp;
        newdatasetfound->complete = 1; // Assume complete until known otherwise.

        for (int i=0; !except && datasetp->filespec[i].name; i++) {
            CACHE1D_FIND_REC *file = NULL;

            switch (datasetp->filespec[i].presence) {
                case STARTWIN_PRESENCE_REQUIRED:
                case STARTWIN_PRESENCE_GROUP: {
                    if (datasetp->filespec[i].alternate) {
                        debugprintf("startwin %s: dataset %d alternate filespec #%d should be optional\n", __func__, datasetp->id, i);
                        break;
                    }
                    for (file = files; file; file = file->next) {
                        struct file_meta *filemeta = (struct file_meta *)file->user;
                        if (filemeta->size == datasetp->filespec[i].size &&
                                filemeta->crc == datasetp->filespec[i].crc)
                            break;
                    }
                    if (!file) newdatasetfound->complete = 0;
                    break;
                }
                case STARTWIN_PRESENCE_OPTIONAL: {
                    int noni = i;
                    if (!datasetp->filespec[i].alternate) break;
                    for (file = files; file; file = file->next) {
                        struct file_meta *filemeta = (struct file_meta *)file->user;
                        if (filemeta->size == datasetp->filespec[i].size &&
                                filemeta->crc == datasetp->filespec[i].crc)
                            break;
                    }
                    if (!file) break;
                    // Find the non-alternate this relates to.
                    for (noni=i+1; datasetp->filespec[noni].name; noni++)
                        if (!datasetp->filespec[noni].alternate) {
                            #ifdef DEBUGGINGAIDS
                            if (strcmp(datasetp->filespec[i].name, datasetp->filespec[noni].name))
                                debugprintf("startwin %s: dataset %d alternate filespec #%d should name-match #%d\n", __func__, datasetp->id, i, noni);
                            #endif
                            break;
                        }
                    if (!datasetp->filespec[noni].name) {
                        debugprintf("startwin %s: dataset %d alternate filespec #%d has no relative\n", __func__, datasetp->id, i);
                        file = NULL;
                        break;
                    }
                    debugprintf("startwin %s: dataset %d alternate filespec #%d found for #%d\n", __func__, datasetp->id, i, noni);
                    i = noni; // Warp forward to the non-alternate so it gets claimed as being present.
                    break;
                }
                case STARTWIN_PRESENCE_EXCEPT: {
                    for (file = files; file; file = file->next) {
                        if (!strcmp(file->name, datasetp->filespec[i].name)) {
                            except = 1;
                            file = NULL;
                            break;
                        }
                    }
                    break;
                }
            }
            if (file) {
                struct file_meta *filemeta = (struct file_meta *)file->user;
                filemeta->claimed = 1;

                newdatasetfound->files[newdatasetfound->numfiles].filespec = &datasetp->filespec[i];
                newdatasetfound->files[newdatasetfound->numfiles].name = file->name;
                newdatasetfound->numfiles++;
            }
        }

        if (!except && newdatasetfound->numfiles > 0) {
            if (!datasetsfound) datasetsfound = tail = newdatasetfound;
            else {
                tail->next = newdatasetfound;
                tail = newdatasetfound;
            }
        } else {
            free(newdatasetfound);
        }
    }

    return 1;
}

static void make_pseudo_datasets(void)
{
    CACHE1D_FIND_REC *file = NULL;
    int id = STARTWIN_DATASET_EXTRA0;
    struct startwin_datasetfound *tail = NULL;

    for (file = files; file; file = file->next) {
        struct file_meta *filemeta = (struct file_meta *)file->user;
        if (filemeta->claimed || !Bwildmatch(file->name, "*.grp")) continue;

        // Allocate a startwin_datasetfound for one file, plus concocted dataset.
        struct startwin_datasetfound *newdatasetfound = (struct startwin_datasetfound *)calloc(1,
            sizeof(struct startwin_datasetfound) +
            sizeof(struct startwin_datasetfoundfile) +
            sizeof(struct startwin_dataset) +
            2 * sizeof(struct startwin_datasetfilespec) // 1 plus a null terminator.
        );
        if (!newdatasetfound) { debugprintf("startwin %s: error allocating a pseudo dataset\n", __func__); return; }

        struct startwin_dataset *fakedataset = (struct startwin_dataset *)(
            (char*)newdatasetfound +
            sizeof(struct startwin_datasetfound) +
            sizeof(struct startwin_datasetfoundfile));
        struct startwin_datasetfilespec *fakefilespec = (struct startwin_datasetfilespec *)(
            (char*)fakedataset +
            sizeof(struct startwin_dataset));

        newdatasetfound->dataset = fakedataset;
        newdatasetfound->complete = 1;
        newdatasetfound->numfiles = 1;
        newdatasetfound->files[0].filespec = fakefilespec;
        newdatasetfound->files[0].name = file->name;

        fakedataset->name = "-";
        fakedataset->id = id;
        fakedataset->type = STARTWIN_DATASET_UNIDENTIFIED;
        fakedataset->filespec = fakefilespec;

        fakefilespec->name = file->name;
        fakefilespec->size = filemeta->size;
        fakefilespec->crc  = filemeta->crc;
        fakefilespec->presence = STARTWIN_PRESENCE_GROUP;

        if (!datasetsfound) datasetsfound = tail = newdatasetfound;
        else {
            if (!tail) for (tail = datasetsfound; tail->next; tail = tail->next) {}
            tail->next = newdatasetfound;
            tail = newdatasetfound;
        }
        id++;
    }
}

const struct startwin_datasetfound * startwin_scan_gamedata(void)
{
    if (!startwin_settings.game.gamedata || !startwin_settings.game.gamedatafilepatterns) return NULL;
    if (datasetsfound) return datasetsfound;

    buildprintf("startwin: identifying data files\n");

    if (!scan_files()) { buildprintf("startwin: no files found\n"); return NULL; }
    load_cache();
    analyse_files();
    if (!identify_datasets()) { buildprintf("startwin: error identifying datasets\n"); return NULL; }
    make_pseudo_datasets();
    write_cache();

    return datasetsfound;
}

void startwin_free_gamedata(void)
{
    if (files) {
        klistfree(files);
        files = NULL;
    }

    if (datasetsfound) {
        struct startwin_datasetfound *next;
        while (datasetsfound) {
            next = datasetsfound->next;
            free(datasetsfound);
            datasetsfound = next;
        }
    }

    if (cachemeta) {
        while (cachemeta && cachemeta->adhoc) {
            struct cache_meta *next = cachemeta->next;
            free(cachemeta);
            cachemeta = next;
        }
        free(cachemeta);
        cachemeta = NULL;
    }
    if (cache) {
        scriptfile_close(cache);
        cache = NULL;
    }
}

const struct startwin_datasetfound * startwin_find_id(int id)
{
    const struct startwin_datasetfound *datasetp;
    for (datasetp = datasetsfound; datasetp; datasetp = datasetp->next) {
        if (datasetp->dataset->id == id && datasetp->complete) return datasetp;
    }
    return NULL;
}

const struct startwin_datasetfound * startwin_find_type(int type)
{
    const struct startwin_datasetfound *datasetp;
    for (datasetp = datasetsfound; datasetp; datasetp = datasetp->next) {
        if (datasetp->dataset->type == type && datasetp->complete) return datasetp;
    }
    return NULL;
}

const struct startwin_datasetfound * startwin_find_filename(const char *filename)
{
    const struct startwin_datasetfound *datasetp;
    for (datasetp = datasetsfound; datasetp; datasetp = datasetp->next) {
        if (!datasetp->complete) continue;
        for (int i=0; i<datasetp->numfiles; i++) {
            if (!strcasecmp(datasetp->files[i].name, filename)) return datasetp;
        }
    }
    return NULL;
}

const struct startwin_datasetfoundfile * startwin_find_dataset_group(const struct startwin_datasetfound *dataset)
{
    for (int i=0; i<dataset->numfiles; i++) {
        if (dataset->files[i].filespec->presence == STARTWIN_PRESENCE_GROUP) {
            return &dataset->files[i];
        }
    }
    return NULL;
}

const struct startwin_datasetfoundfile * startwin_find_id_group(int id)
{
    const struct startwin_datasetfound *datasetp = startwin_find_id(id);
    if (datasetp) return startwin_find_dataset_group(datasetp);
    return NULL;
}

const struct startwin_datasetfoundfile * startwin_find_type_group(int type)
{
    const struct startwin_datasetfound *datasetp = startwin_find_type(type);
    if (datasetp) return startwin_find_dataset_group(datasetp);
    return NULL;
}

static int startwin_import_file(const char *filepath, struct startwin_import_meta *meta)
{
    int i, interest = 0, fh, rv;
    unsigned int crc;
    off_t size;
    const struct startwin_dataset *datasetp;
    const char *outfname = NULL;

    fh = open(filepath, O_RDONLY|O_BINARY, S_IREAD);
    if (fh < 0) return STARTWIN_IMPORT_OK;
    meta->progress(meta->data, filepath);

    crc = crc32fh(fh, meta);
    if (meta->wascancelled) { close(fh); return STARTWIN_IMPORT_OK; }
    size = lseek(fh, 0, SEEK_CUR);
    if (size < 0) { close(fh); return STARTWIN_IMPORT_OK; };

    for (datasetp = &startwin_settings.game.gamedata[0]; !interest && datasetp->name; datasetp++) {
        for (i=0; !interest && datasetp->filespec[i].name; i++) {
            switch (datasetp->filespec[i].presence) {
                case STARTWIN_PRESENCE_OPTIONAL:
                {
                    // Wildcard match using the filespec name.
                    outfname = filepath;
                    for (const char *p = filepath; *p; p++) if (*p == '/' || *p == '\\') outfname = p+1;
                    interest = Bwildmatch(outfname, datasetp->filespec[i].name);
                    break;
                }
                case STARTWIN_PRESENCE_REQUIRED:
                case STARTWIN_PRESENCE_GROUP:
                    // Compare size and crc.
                    interest = (datasetp->filespec[i].size == (size_t)size && datasetp->filespec[i].crc == crc);
                    outfname = datasetp->filespec[i].storename;
                    if (!outfname) outfname = datasetp->filespec[i].name;
                    break;
            }
        }
    }
    if (!interest) return STARTWIN_IMPORT_OK;

    rv = STARTWIN_IMPORT_OK;
    switch (copy_file(fh, (size_t)size, outfname, meta)) {
        case COPYFILE_ERR_CANCELLED: buildprintf("startwin: copy cancelled during %s\n", filepath); break;
        case COPYFILE_ERR_OPEN: buildprintf("startwin: error creating %s\n", outfname); break;
        case COPYFILE_ERR_SEEK: buildprintf("startwin: error accessing %s\n", filepath); break;
        case COPYFILE_ERR_RW: rv = STARTWIN_IMPORT_ERROR; buildprintf("startwin: error copying from %s\n", filepath); break;
        case COPYFILE_ERR_EXISTS: rv = STARTWIN_IMPORT_SKIPPED; buildprintf("startwin: skipped existing %s\n", filepath); break;
        case COPYFILE_OK: rv = STARTWIN_IMPORT_COPIED; buildprintf("startwin: copied %s\n", filepath); break;
    }
    close(fh);
    return rv;
}

static int grow_dirpath(char **dirpath, size_t *dirpathsiz, size_t growby)
{
    size_t newsiz = *dirpathsiz + growby;
    char *newdirpath = realloc(*dirpath, newsiz);

    if (!newdirpath) { debugprintf("startwin %s: error growing to %zu\n", __func__, newsiz); return 0; }
    *dirpath = newdirpath;
    *dirpathsiz = newsiz;
    return 1;
}

static int startwin_import_dir(char **dirpath, size_t *dirpathsiz, struct startwin_import_meta *meta)
{
    CACHE1D_FIND_REC *importfiles, *importdirs, *rec;
    int found = 0, errors = 0, pathendoffs;

    meta->progress(meta->data, *dirpath);

    pathsearchmode = PATHSEARCH_SYSTEM;
    importfiles = klistpath(*dirpath, startwin_settings.game.gamedatafilepatterns,
        CACHE1D_FIND_FILE | CACHE1D_OPT_NOGRP | CACHE1D_OPT_NOZIP, 0);
    importdirs = klistpath(*dirpath, KLISTPATH_MASK(""),
        CACHE1D_FIND_DIR | CACHE1D_OPT_NOGRP | CACHE1D_OPT_NOZIP, 0);
    pathsearchmode = PATHSEARCH_GAME;

    for (pathendoffs = 0; (*dirpath)[pathendoffs]; pathendoffs++) {}
    (*dirpath)[pathendoffs++] = '/';
    (*dirpath)[pathendoffs] = 0;

    // Inspect the files at this level.
    for (rec = importfiles; !meta->wascancelled && rec; rec = rec->next) {
        size_t namelen = strlen(rec->name);
        if (pathendoffs + namelen + 1 > *dirpathsiz)
            if (!grow_dirpath(dirpath, dirpathsiz, max(BMAX_PATH, namelen + 1)))
                break;
        strcpy(&(*dirpath)[pathendoffs], rec->name);
        switch (startwin_import_file(*dirpath, meta)) {
            case STARTWIN_IMPORT_COPIED: found = 1; break;
            case STARTWIN_IMPORT_ERROR: errors = 1; break;
        }
    }
    klistfree(importfiles);

    // Now recurse into directories.
    for (rec = importdirs; !meta->wascancelled && rec; rec = rec->next) {
        if (!strcmp(rec->name, "..")) continue;
        size_t namelen = strlen(rec->name);
        if (pathendoffs + namelen + 1 > *dirpathsiz)
            if (!grow_dirpath(dirpath, dirpathsiz, max(BMAX_PATH, namelen + 1)))
                break;
        strcpy(&(*dirpath)[pathendoffs], rec->name);
        switch (startwin_import_dir(dirpath, dirpathsiz, meta)) {
            case STARTWIN_IMPORT_COPIED: found = 1; break;
            case STARTWIN_IMPORT_ERROR: errors = 1; break;
        }
    }
    klistfree(importdirs);

    if (found) return STARTWIN_IMPORT_COPIED;   // Finding anything is considered fine.
    else if (errors) return STARTWIN_IMPORT_ERROR; // Finding nothing but errors reports back errors.
    return STARTWIN_IMPORT_OK;
}

int startwin_import_path(const char *path, struct startwin_import_meta *meta)
{
    struct stat st;
    int found = 0, errors = 0;
    size_t wpathsiz;
    char *wpath;

    if (stat(path, &st) < 0) {
        debugprintf("startwin %s: error stat'ing path %s\n", __func__, path);
        return STARTWIN_IMPORT_ERROR;
    }

    if (st.st_mode & S_IFREG) {
        switch (startwin_import_file(path, meta)) {
            case STARTWIN_IMPORT_COPIED: found = 1; break;
            case STARTWIN_IMPORT_ERROR: errors = 1; break;
        }
    } else if (st.st_mode & S_IFDIR) {
        // The startwin_import_dir subfunction will modify the path it is passed, appending filenames
        // and directories as it recurses. If the workspace needs to grow, it will realloc() it.
        wpathsiz = strlen(path) + BMAX_PATH + 1;
        wpath = (char *)malloc(wpathsiz);
        if (!wpath) {
            debugprintf("startwin %s: error allocating %zu-byte work path\n", __func__, wpathsiz);
            return STARTWIN_IMPORT_ERROR;
        }
        strcpy(wpath, path);

        switch (startwin_import_dir(&wpath, &wpathsiz, meta)) {
            case STARTWIN_IMPORT_COPIED: found = 1; break;
            case STARTWIN_IMPORT_ERROR: errors = 1; break;
        }

        free(wpath);
    }

    if (found) {
        meta->progress(meta->data, "...");
        startwin_free_gamedata();
        startwin_scan_gamedata();
        return STARTWIN_IMPORT_COPIED;
    }
    else if (errors) return STARTWIN_IMPORT_ERROR;
    return STARTWIN_IMPORT_OK;
}

#if defined(RENDERTYPEWIN)
# define HAVE_STARTWIN
#elif defined(RENDERTYPESDL) && defined(__APPLE__) && defined(HAVE_OSX_FRAMEWORKS)
# define HAVE_STARTWIN
#elif defined(RENDERTYPESDL) && defined(HAVE_GTK)
# define HAVE_STARTWIN
#endif

#if !defined(HAVE_STARTWIN)
int startwin_open(void) { return 0; }
int startwin_close(void) { return 0; }
int startwin_puts(const char *s) { (void)s; return 0; }
int startwin_idle(void *s) { (void)s; return 0; }
int startwin_settitle(const char *s) { (void)s; return 0; }
int startwin_run(void) { return STARTWIN_RUN; }
#endif
