// Common Start-up Window, macOS variant
// for the Build Engine

// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#import <Cocoa/Cocoa.h>
#include "startwin.h"
#include "startwin_priv.h"
#include "build.h"
#include "baselayer.h"

static void import_progress(void *data, const char *path);
static int import_cancelled(void *data);

@interface StartupWinController : NSWindowController <NSWindowDelegate>
{
    IBOutlet NSTabView *tabView;
    IBOutlet NSTabViewItem *tabConfig;
    IBOutlet NSTabViewItem *tabGame;
    IBOutlet NSTabViewItem *tabMessages;
    IBOutlet NSTextView *messagesView;

    IBOutlet NSView *featureVideoView;
    IBOutlet NSView *featureEditorView;
    IBOutlet NSView *featureAudioView;
    IBOutlet NSView *featureInputView;
    IBOutlet NSView *featureNetworkView;

    IBOutlet NSPopUpButton *videoMode2DPUButton;
    IBOutlet NSPopUpButton *videoMode3DPUButton;
    IBOutlet NSButton *fullscreenButton;
    IBOutlet NSPopUpButton *displayPUButton;

    IBOutlet NSPopUpButton *soundQualityPUButton;

    IBOutlet NSButton *useMouseButton;
    IBOutlet NSButton *useJoystickButton;

    IBOutlet NSButton *singlePlayerButton;
    IBOutlet NSButton *joinMultiButton;
    IBOutlet NSTextField *hostField;
    IBOutlet NSButton *hostMultiButton;
    IBOutlet NSTextField *numPlayersField;
    IBOutlet NSStepper *numPlayersStepper;

    IBOutlet NSScrollView *gameList;
    IBOutlet NSArrayController *gameArrayController;
    IBOutlet NSButton *chooseImportButton;
    IBOutlet NSButton *importInfoButton;

    IBOutlet NSTextField *appName;
    IBOutlet NSButton *alwaysShowButton;
    IBOutlet NSButton *cancelButton;
    IBOutlet NSButton *startButton;

    IBOutlet NSWindow *importStatusWindow;
    IBOutlet NSTextField *importStatusText;
    IBOutlet NSButton *importStatusCancel;

    BOOL quiteventonclose;
    BOOL inmodal;
    int startcode;
    NSThread *importthread;
}

- (int)modalRun;
- (void)closeQuietly;
- (void)populateVideoModes:(BOOL)firstTime;
- (void)populateSoundQuality:(BOOL)firstTime;
- (void)populateGameList:(BOOL)firstTime;

- (IBAction)fullscreenOrDisplayClicked:(id)sender;

- (IBAction)multiPlayerModeClicked:(id)sender;

- (IBAction)chooseImportClicked:(id)sender;
- (IBAction)importInfoClicked:(id)sender;
- (IBAction)importStatusCancelClicked:(id)sender;
- (void)updateImportStatusText:(NSString *)text;
- (void)doImport:(NSString *)path;
- (void)doneImport:(NSNumber *)result;
- (BOOL)isImportCancelled;

- (IBAction)cancel:(id)sender;
- (IBAction)start:(id)sender;

- (void)configure:(BOOL)firstTime;
- (void)setupConfigMode;
- (void)setupMessagesMode:(BOOL)allowCancel;
- (void)putsMessage:(NSString *)str;
- (void)setTitle:(NSString *)str;

@end

@implementation StartupWinController

- (void)windowDidLoad
{
    quiteventonclose = TRUE;
}

- (BOOL)windowShouldClose:(id)sender
{
    if (inmodal) {
        startcode = STARTWIN_CANCEL;
        [NSApp stop:nil];
    }
    quitevent = quitevent || quiteventonclose;
    return NO;
}

- (int)modalRun
{
    inmodal = YES;
    startcode = STARTWIN_CANCEL;
    [[self window] makeKeyAndOrderFront:nil];
    [NSApp run];
    inmodal = NO;

    return startcode;
}

// Close the window, but don't cause the quitevent flag to be set
// as though this is a cancel operation.
- (void)closeQuietly
{
    quiteventonclose = FALSE;
    [self close];
}

- (void)populateVideoModes:(BOOL)firstTime
{
    int i;
    int mode3d = -1, idx3d = -1;
    int xdim3d = 0, ydim3d = 0, bitspp = 0, display = 0, fullsc = 0;
    int mode2d = -1, idx2d = -1;
    int xdim2d = 0, ydim2d = 0;
    int cd[] = { 32, 24, 16, 15, 8, 0 };
    NSMenu *menu3d = nil, *menu2d = nil;
    NSMenuItem *menuitem = nil;

    if (firstTime) {
        getvalidmodes();
        if (startwin_settings.features.video) {
            xdim3d = startwin_settings.video.xdim;
            ydim3d = startwin_settings.video.ydim;
            bitspp = startwin_settings.video.bpp;
            fullsc = startwin_settings.video.fullscreen;
            display = min(displaycnt-1, max(0, startwin_settings.video.display));

            NSMenu *menu = [displayPUButton menu];
            [menu removeAllItems];
            for (int i = 0; i < displaycnt; i++) {
                menuitem = [menu addItemWithTitle:[NSString
                        stringWithFormat:@"Display %d \u2013 %s", i, getdisplayname(i)]
                                           action:nil
                                    keyEquivalent:@""];
                [menuitem setTag:i];
            }
            if (displaycnt < 2) [displayPUButton setHidden:YES];
        }
        if (startwin_settings.features.editor) {
            xdim2d = startwin_settings.editor.xdim;
            ydim2d = startwin_settings.editor.ydim;
        }
    } else {
        fullsc = ([fullscreenButton state] == NSControlStateValueOn);
        if (fullsc) display = max(0, (int)[displayPUButton selectedTag]);
        mode3d = (int)[videoMode3DPUButton selectedTag];
        if (mode3d >= 0) {
            xdim3d = validmode[mode3d].xdim;
            ydim3d = validmode[mode3d].ydim;
            bitspp = validmode[mode3d].bpp;
        }
        mode2d = (int)[videoMode2DPUButton selectedTag];
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

    // Repopulate the mode lists.
    menu3d = [videoMode3DPUButton menu];
    menu2d = [videoMode2DPUButton menu];
    [menu3d removeAllItems];
    [menu2d removeAllItems];

    for (i = 0; i < validmodecnt; i++) {
        if (validmode[i].fs != fullsc) continue;
        if (validmode[i].display != display) continue;

        if (i == mode3d || idx3d < 0) idx3d = i;
        menuitem = [menu3d addItemWithTitle:[NSString
                stringWithFormat:@"%d \u00d7 %d %d-bpp",
                    validmode[i].xdim, validmode[i].ydim, validmode[i].bpp]
                                     action:nil
                              keyEquivalent:@""];
        [menuitem setTag:i];

        if (validmode[i].bpp == 8 && validmode[i].xdim >= 640 && validmode[i].ydim >= 480) {
            if (i == mode2d || idx2d < 0) idx2d = i;
            menuitem = [menu2d addItemWithTitle:[NSString
                    stringWithFormat:@"%d \u00d7 %d",
                        validmode[i].xdim, validmode[i].ydim]
                                         action:nil
                                  keyEquivalent:@""];
            [menuitem setTag:i];
        }
    }
    if (idx2d >= 0) [videoMode2DPUButton selectItemWithTag:idx2d];
    if (idx3d >= 0) [videoMode3DPUButton selectItemWithTag:idx3d];

    [displayPUButton selectItemWithTag:validmode[mode3d].display];
    [displayPUButton setEnabled: validmode[mode3d].fs ? YES : NO];
    [fullscreenButton setState: validmode[mode3d].fs ? NSControlStateValueOn : NSControlStateValueOff];
}

- (void)populateSoundQuality:(BOOL)firstTime
{
    int i, curidx = -1;
    int samplerate = 0, bitspersample = 0, channels = 0;
    NSMenu *menu = nil;
    NSMenuItem *menuitem = nil;

    if (firstTime) {
        samplerate = startwin_settings.audio.samplerate;
        bitspersample = startwin_settings.audio.bitspersample;
        channels = startwin_settings.audio.channels;
    } else {
        curidx = (int)[soundQualityPUButton selectedTag];
        if (curidx >= 0) {
            samplerate = startwin_soundqualities[curidx].frequency;
            bitspersample = startwin_soundqualities[curidx].samplesize;
            channels = startwin_soundqualities[curidx].channels;
        }
    }

    menu = [soundQualityPUButton menu];
    [menu removeAllItems];

    for (i = 0; startwin_soundqualities[i].frequency; i++) {
        if ((samplerate == startwin_soundqualities[i].frequency &&
                bitspersample == startwin_soundqualities[i].samplesize &&
                channels == startwin_soundqualities[i].channels) || curidx < 0) curidx = i;
        menuitem = [menu addItemWithTitle:[NSString stringWithFormat:@"%d kHz, %d-bit, %s",
                                          startwin_soundqualities[i].frequency / 1000,
                                          startwin_soundqualities[i].samplesize,
                                          startwin_soundqualities[i].channels == 1 ? "Mono" : "Stereo"]
                                   action:nil
                            keyEquivalent:@""];
        [menuitem setTag:i];
    }
    if (curidx >= 0) [soundQualityPUButton selectItemAtIndex:curidx];
}

- (void)populateGameList:(BOOL)firstTime
{
    const struct startwin_datasetfound * datasetp;
    int sel = -1, oldid = -1, index;

    if (!firstTime) {
        if ([[gameArrayController selectedObjects] count] > 0) {
            oldid = [[gameArrayController selectedObjects][0][@"id"] intValue];
        }
    }

    [[gameArrayController content] removeAllObjects];
    for (datasetp = startwin_scan_gamedata(), index = 0; datasetp; datasetp = datasetp->next, index++) {
        NSString *filename = @"";
        NSString *gamename;

        if (!datasetp->complete) continue;

        gamename = [NSString stringWithCString:datasetp->dataset->name
                                      encoding:NSUTF8StringEncoding];

        const struct startwin_datasetfoundfile *grp = startwin_find_dataset_group(datasetp);
        if (grp) filename = [NSString stringWithCString:grp->name
                                               encoding:NSUTF8StringEncoding];

        [gameArrayController addObject:@{
            @"gamename":gamename,
            @"filename":filename,
            @"id":[NSNumber numberWithInt:datasetp->dataset->id]
        }];

        if (oldid == datasetp->dataset->id) sel = index;
        else if (sel < 0 && datasetp->dataset->id == startwin_settings.game.gamedataid) sel = index;
    }
    if (sel >= 0) [gameArrayController setSelectionIndex:sel];
    else if (index > 0) [gameArrayController setSelectionIndex:0];
}

- (IBAction)fullscreenOrDisplayClicked:(id)sender
{
    [self populateVideoModes:NO];
}

- (IBAction)multiPlayerModeClicked:(id)sender
{
    [singlePlayerButton setState:(sender == singlePlayerButton ? NSControlStateValueOn : NSControlStateValueOff)];

    [joinMultiButton setState:(sender == joinMultiButton ? NSControlStateValueOn : NSControlStateValueOff)];
    [hostField setEnabled:(sender == joinMultiButton)];

    [hostMultiButton setState:(sender == hostMultiButton ? NSControlStateValueOn : NSControlStateValueOff)];
    [numPlayersField setEnabled:(sender == hostMultiButton)];
    [numPlayersStepper setEnabled:(sender == hostMultiButton)];
}

- (IBAction)chooseImportClicked:(id)sender
{
    @autoreleasepool {
        NSArray *filetypes = [[NSArray alloc] initWithObjects:@"grp", @"app", nil];
        NSOpenPanel *panel = [NSOpenPanel openPanel];

        [panel setTitle:@"Import game data"];
        [panel setPrompt:@"Import"];
        [panel setMessage:@"Select a .grp file, an .app bundle, or choose a folder to search."];
        [panel setAllowedFileTypes:filetypes];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:YES];
        [panel setShowsHiddenFiles:YES];
        [panel beginSheetModalForWindow:[self window]
                      completionHandler:^void (NSModalResponse resp) {
            if (resp == NSModalResponseOK) {
                NSURL *file = [panel URL];
                if ([file isFileURL]) {
                    [self doImport:[file path]];
                }
            }
        }];
    }
}

- (IBAction)importInfoClicked:(id)sender
{
    @autoreleasepool {
        NSAlert *alert = [[NSAlert alloc] init];
        NSInteger resp;

        [alert setAlertStyle:NSAlertStyleInformational];
        [alert setMessageText:[NSString
            stringWithCString:startwin_settings.game.moreinfobrief
                     encoding:NSUTF8StringEncoding]];
        [alert setInformativeText:[NSString
            stringWithCString:startwin_settings.game.moreinfodetail
                     encoding:NSUTF8StringEncoding]];
        [alert addButtonWithTitle:@"OK"];
        if (startwin_settings.game.demourl) {
            [alert addButtonWithTitle:@"Download Demo"];
        }
        resp = [alert runModal];
        if (resp == NSAlertSecondButtonReturn) {
            NSURL *demourl = [NSURL URLWithString:[NSString
                stringWithCString:startwin_settings.game.demourl
                         encoding:NSUTF8StringEncoding]];
            LSOpenCFURLRef((CFURLRef)demourl, nil);
        }
    }
}

- (IBAction)importStatusCancelClicked:(id)sender
{
    [importthread cancel];
}

- (void)updateImportStatusText:(NSString *)text
{
    [importStatusText setStringValue:text];
}

- (void)doImport:(NSString *)path
{
    if ([importthread isExecuting]) {
        NSLog(@"import thread is already executing");
        return;
    }

    // Put up the status sheet which becomes modal.
    [[self window] beginSheet:importStatusWindow
            completionHandler:nil];

    // Spawn a thread to do the scan.
    importthread = [[NSThread alloc] initWithBlock:^void(void) {
        struct startwin_import_meta meta = {
            (void *)self,
            0,
            import_progress,
            import_cancelled
        };
        int result = startwin_import_path([path UTF8String], &meta);
        [self performSelectorOnMainThread:@selector(doneImport:)
                               withObject:[NSNumber numberWithInt:result]
                            waitUntilDone:NO];
    }];
    [importthread start];
}

// Finish up after the import thread returns.
- (void)doneImport:(NSNumber *)result
{
    if ([result intValue] >= STARTWIN_IMPORT_OK) {
        [self populateGameList:NO];
    }
    [importStatusWindow orderOut:nil];
    [[self window] endSheet:importStatusWindow
                 returnCode:1];
}

// Report on whether the import thread has been been cancelled early.
- (BOOL)isImportCancelled
{
    return [importthread isCancelled];
}

- (IBAction)cancel:(id)sender
{
    if (inmodal) {
        startcode = STARTWIN_CANCEL;
        [NSApp stop:nil];
    }
    quitevent = quitevent || quiteventonclose;
}

- (IBAction)start:(id)sender
{
    int mode = -1;

    if (startwin_settings.features.video) {
        mode = (int)[videoMode3DPUButton selectedTag];
        if (mode >= 0) {
            startwin_settings.video.xdim = validmode[mode].xdim;
            startwin_settings.video.ydim = validmode[mode].ydim;
            startwin_settings.video.bpp = validmode[mode].bpp;
            startwin_settings.video.fullscreen = validmode[mode].fs;
            startwin_settings.video.display = validmode[mode].display;
        }
    }
    if (startwin_settings.features.editor) {
        mode = (int)[videoMode2DPUButton selectedTag];
        if (mode >= 0) {
            startwin_settings.editor.xdim = validmode[mode].xdim;
            startwin_settings.editor.ydim = validmode[mode].ydim;
        }
    }
    if (startwin_settings.features.input) {
        startwin_settings.input.mouse = [useMouseButton state] == NSControlStateValueOn;
        startwin_settings.input.controller = [useJoystickButton state] == NSControlStateValueOn;
    }
    if (startwin_settings.features.audio) {
        mode = (int)[soundQualityPUButton selectedTag];
        if (mode >= 0) {
            startwin_settings.audio.samplerate = startwin_soundqualities[mode].frequency;
            startwin_settings.audio.bitspersample = startwin_soundqualities[mode].samplesize;
            startwin_settings.audio.channels = startwin_soundqualities[mode].channels;
        }
    }
    if (startwin_settings.features.network) {
        startwin_settings.network.numplayers = 0;
        startwin_settings.network.joinhost = NULL;
        if ([singlePlayerButton state] == NSControlStateValueOn) {
            startwin_settings.network.numplayers = 1;
        } else if ([joinMultiButton state] == NSControlStateValueOn) {
            NSString *host = [hostField stringValue];
            startwin_settings.network.numplayers = 2;
            startwin_settings.network.joinhost = strdup([host cStringUsingEncoding:NSUTF8StringEncoding]);
        } else if ([hostMultiButton state] == NSControlStateValueOn) {
            startwin_settings.network.numplayers = [numPlayersField intValue];
        }
    }
    if (startwin_settings.features.game) {
        if ([[gameArrayController arrangedObjects] count] > 0) {
            startwin_settings.game.gamedataid = [[gameArrayController selectedObjects][0][@"id"] intValue];
        } else {
            startwin_settings.game.gamedataid = 0;
        }
    }

    startwin_settings.alwaysshow = [alwaysShowButton state] == NSControlStateValueOn;

    if (inmodal) {
        startcode = STARTWIN_RUN;
        [NSApp stop:nil];
    }
}

- (void)configure:(BOOL)firstTime
{
    CGFloat adjust = 0;
    NSRect frame;

    if (firstTime) {
        // On initial creation, take the config and game tabs out of the view.
        [tabConfig retain]; // Don't let go of the tab items as they'll be reinstated later.
        [tabGame retain];
        [tabView removeTabViewItem:tabConfig];
        [tabView removeTabViewItem:tabGame];

        [appName setStringValue:[[NSRunningApplication currentApplication] localizedName]];
        return;
    }

    [tabView insertTabViewItem:tabConfig atIndex:0];

    const struct {
        NSView *view;
        int enabled;
    } views[] = {
        { featureVideoView,   startwin_settings.features.video   },
        { featureEditorView,  startwin_settings.features.editor  },
        { featureAudioView,   startwin_settings.features.audio   },
        { featureInputView,   startwin_settings.features.input   },
        { featureNetworkView, startwin_settings.features.network },
    };
    for (size_t i=0; i<Barraylen(views); i++) {
        frame = [views[i].view frame];
        if (!views[i].enabled) {
            adjust += NSHeight(frame) + 8.0;
            [views[i].view setHidden:YES];
        } else {
            [views[i].view setFrameOrigin:NSMakePoint(NSMinX(frame), NSMinY(frame) + adjust)];
        }
    }

    if (startwin_settings.features.game) {
        [tabView insertTabViewItem:tabGame atIndex:1];
    }
}

- (void)setupConfigMode
{
    [alwaysShowButton setState: (startwin_settings.alwaysshow ? NSControlStateValueOn : NSControlStateValueOff)];
    [alwaysShowButton setEnabled:YES];

    if (startwin_settings.features.video || startwin_settings.features.editor) {
        if (startwin_settings.features.video) {
            [videoMode3DPUButton setEnabled:YES];
            [fullscreenButton setEnabled:YES];
            [displayPUButton setEnabled:YES];
        }
        if (startwin_settings.features.editor) {
            [videoMode2DPUButton setEnabled:YES];
        }
        [self populateVideoModes:YES];
    }
    if (startwin_settings.features.audio) {
        [soundQualityPUButton setEnabled:YES];
        [self populateSoundQuality:YES];
    }
    if (startwin_settings.features.input) {
        [useMouseButton setEnabled:YES];
        [useMouseButton setState: (startwin_settings.input.mouse ?
            NSControlStateValueOn : NSControlStateValueOff)];
        [useJoystickButton setEnabled:YES];
        [useJoystickButton setState: (startwin_settings.input.controller ?
            NSControlStateValueOn : NSControlStateValueOff)];
    }
    if (startwin_settings.features.network) {
        if (!startwin_settings.network.netoverride) {
            [singlePlayerButton setEnabled:YES];
            [singlePlayerButton setState:NSControlStateValueOn];

            [hostMultiButton setEnabled:YES];
            [hostMultiButton setState:NSControlStateValueOff];
            [numPlayersField setEnabled:NO];
            [numPlayersField setIntValue:2];
            [numPlayersStepper setEnabled:NO];
            [numPlayersStepper setMaxValue:MAXPLAYERS];

            [joinMultiButton setEnabled:YES];
            [joinMultiButton setState:NSControlStateValueOff];
            [hostField setEnabled:NO];
        } else {
            [singlePlayerButton setEnabled:NO];
            [hostMultiButton setEnabled:NO];
            [numPlayersField setEnabled:NO];
            [numPlayersStepper setEnabled:NO];
            [joinMultiButton setEnabled:NO];
            [hostField setEnabled:NO];
        }
    }
    if (startwin_settings.features.game) {
        [self populateGameList:YES];
        [[gameList documentView] setEnabled:YES];
        [chooseImportButton setEnabled:YES];
        [importInfoButton setEnabled:YES];
    }

    if (startwin_settings.features.game && !startwin_settings.game.gamedataid) {
        [tabView selectTabViewItem:tabGame];
    } else {
        [tabView selectTabViewItem:tabConfig];
    }

    [cancelButton setEnabled:YES];
    [startButton setEnabled:YES];

    [NSCursor unhide];  // Why should I need to do this?
}

- (void)setupMessagesMode:(BOOL)allowCancel
{
    NSEnumerator *enumerator;

    [tabView selectTabViewItem:tabMessages];

    // Disable all the controls in each feature view of the Configuration page.
    const NSView *views[] = {
        featureVideoView, featureEditorView, featureAudioView,
        featureInputView, featureNetworkView
    };
    for (size_t i=0; i<Barraylen(views); i++) {
        enumerator = [[views[i] subviews] objectEnumerator];
        for (NSControl *control = [enumerator nextObject]; control; control = [enumerator nextObject])
            [control setEnabled:NO];
    }

    [[gameList documentView] setEnabled:NO];
    [chooseImportButton setEnabled:NO];
    [importInfoButton setEnabled:NO];

    [alwaysShowButton setEnabled:NO];

    [cancelButton setEnabled:allowCancel];
    [startButton setEnabled:NO];
}

- (void)putsMessage:(NSString *)str
{
    NSRange end;
    NSTextStorage *text = [messagesView textStorage];
    BOOL shouldAutoScroll;

    shouldAutoScroll = ((int)NSMaxY([messagesView bounds]) == (int)NSMaxY([messagesView visibleRect]));

    end.location = [text length];
    end.length = 0;

    [text beginEditing];
    [messagesView replaceCharactersInRange:end withString:str];
    [text endEditing];

    if (shouldAutoScroll) {
        end.location = [text length];
        end.length = 0;
        [messagesView scrollRangeToVisible:end];
    }
}

- (void)setTitle:(NSString *)str
{
    [[self window] setTitle:str];
}

@end

static StartupWinController *startwin = nil;

int startwin_open(void)
{
    if (startwin != nil) return 1;

    @autoreleasepool {
        startwin = [[StartupWinController alloc] initWithWindowNibName:@"StartupWin"];
        if (startwin == nil) return -1;

        NSWindow *win = [startwin window];  // Forces the window controls on the controller to be initialised.
        if (win == nil) {
            [startwin closeQuietly];
            [startwin release];
            startwin = nil;
            return -1;
        }

        [startwin configure:YES];
        [startwin setupMessagesMode:YES];
        [startwin showWindow:nil];

        return 0;
    }
}

int startwin_close(void)
{
    if (startwin == nil) return 1;

    @autoreleasepool {
        [startwin closeQuietly];
        [startwin release];
        startwin = nil;

        return 0;
    }
}

int startwin_puts(const char *s)
{
    if (!s) return -1;
    if (startwin == nil) return 1;

    @autoreleasepool {
        NSString *str = [NSString stringWithUTF8String:s];
        if ([NSThread isMainThread]) {
            [startwin putsMessage:str];
        } else {
            [startwin performSelectorOnMainThread:@selector(putsMessage:)
                                       withObject:str
                                    waitUntilDone:YES];
        }

        return 0;
    }
}

int startwin_settitle(const char *s)
{
    if (!s) return -1;
    if (startwin == nil) return 1;

    @autoreleasepool {
        [startwin setTitle:[NSString stringWithUTF8String:s]];

        return 0;
    }
}

int startwin_idle(void *v)
{
    (void)v;
    return 0;
}

int startwin_run(void)
{
    if (startwin == nil) return STARTWIN_RUN;

    @autoreleasepool {
        [startwin configure:NO];
        [startwin setupConfigMode];

        int retval = [startwin modalRun];
        [startwin setupMessagesMode: startwin_settings.features.network &&
            (startwin_settings.network.numplayers > 1)];

        return retval;
    }
}

static void import_progress(void *data, const char *path)
{
    StartupWinController *control = (StartupWinController *)data;

    @autoreleasepool {
        [control performSelectorOnMainThread:@selector(updateImportStatusText:)
                                  withObject:[NSString stringWithUTF8String:path]
                               waitUntilDone:FALSE];
    }
}

static int import_cancelled(void *data)
{
    StartupWinController *control = (StartupWinController *)data;
    return [control isImportCancelled];
}
