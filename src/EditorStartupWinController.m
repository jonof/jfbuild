#import <Cocoa/Cocoa.h>

#include "build.h"
#include "baselayer.h"
#include "editor.h"

#include <stdlib.h>

@interface EditorStartupWinController : NSWindowController <NSWindowDelegate>
{
    BOOL quiteventonclose;
    BOOL inmodal;
    struct startwin_settings *settings;

    IBOutlet NSButton *alwaysShowButton;
    IBOutlet NSButton *fullscreenButton;
    IBOutlet NSTextView *messagesView;
    IBOutlet NSTabView *tabView;
    IBOutlet NSTabViewItem *tabConfig;
    IBOutlet NSTabViewItem *tabMessages;
    IBOutlet NSPopUpButton *videoMode2DPUButton;
    IBOutlet NSPopUpButton *videoMode3DPUButton;

    IBOutlet NSButton *cancelButton;
    IBOutlet NSButton *startButton;
}

- (int)modalRun;
- (void)closeQuietly;
- (void)populateVideoModes:(BOOL)firstTime;

- (IBAction)fullscreenClicked:(id)sender;

- (IBAction)cancel:(id)sender;
- (IBAction)start:(id)sender;

- (void)setupConfigMode;
- (void)setupMessagesMode:(BOOL)allowCancel;
- (void)putsMessage:(NSString *)str;
- (void)setTitle:(NSString *)str;
- (void)setSettings:(struct startwin_settings *)theSettings;

@end

@implementation EditorStartupWinController

- (void)windowDidLoad
{
    quiteventonclose = TRUE;
}

- (BOOL)windowShouldClose:(id)sender
{
    if (inmodal) {
        [NSApp stopModalWithCode:STARTWIN_CANCEL];
    }
    quitevent = quitevent || quiteventonclose;
    return NO;
}

- (int)modalRun
{
    int retval;

    inmodal = YES;
    switch ([NSApp runModalForWindow:[self window]]) {
        case STARTWIN_RUN: retval = STARTWIN_RUN; break;
        case STARTWIN_CANCEL: retval = STARTWIN_CANCEL; break;
        default: retval = -1;
    }
    inmodal = NO;

    return retval;
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
    int i, mode2d = -1, mode3d = -1;
    int idx2d = -1, idx3d = -1;
    int xdim = 0, ydim = 0, bpp = 0, fullscreen = 0;
    int xdim2d = 0, ydim2d = 0;
    int cd[] = {
#if USE_POLYMOST && USE_OPENGL
        32, 24, 16, 15,
#endif
        8, 0
    };
    NSMenu *menu3d = nil, *menu2d = nil;
    NSMenuItem *menuitem = nil;

    if (firstTime) {
        getvalidmodes();
        xdim = settings->xdim3d;
        ydim = settings->ydim3d;
        bpp  = settings->bpp3d;
        fullscreen = settings->fullscreen;

        xdim2d = settings->xdim2d;
        ydim2d = settings->ydim2d;
    } else {
        fullscreen = ([fullscreenButton state] == NSOnState);
        mode3d = [[videoMode3DPUButton selectedItem] tag];
        if (mode3d >= 0) {
            xdim = validmode[mode3d].xdim;
            ydim = validmode[mode3d].ydim;
            bpp = validmode[mode3d].bpp;
        }
        mode2d = [[videoMode2DPUButton selectedItem] tag];
        if (mode2d >= 0) {
            xdim2d = validmode[mode2d].xdim;
            ydim2d = validmode[mode2d].ydim;
        }
    }

    // Find an ideal match.
    mode3d = checkvideomode(&xdim, &ydim, bpp, fullscreen, 1);
    mode2d = checkvideomode(&xdim2d, &ydim2d, 8, fullscreen, 1);
    if (mode2d < 0) {
        mode2d = 0;
    }
    if (mode3d < 0) {
        for (i=0; cd[i]; ) { if (cd[i] >= bpp) i++; else break; }
        for ( ; cd[i]; i++) {
            mode3d = checkvideomode(&xdim, &ydim, cd[i], fullscreen, 1);
            if (mode3d < 0) continue;
            break;
        }
    }

    // Repopulate the lists.
    menu3d = [videoMode3DPUButton menu];
    menu2d = [videoMode2DPUButton menu];
    [menu3d removeAllItems];
    [menu2d removeAllItems];

    for (i = 0; i < validmodecnt; i++) {
        if (validmode[i].fs != fullscreen) continue;

        if (i == mode3d) idx3d = i;
        menuitem = [menu3d addItemWithTitle:[NSString stringWithFormat:@"%d %C %d %d-bpp",
                                          validmode[i].xdim, 0xd7, validmode[i].ydim, validmode[i].bpp]
                                     action:nil
                              keyEquivalent:@""];
        [menuitem setTag:i];

        if (validmode[i].bpp == 8 && validmode[i].xdim >= 640 && validmode[i].ydim >= 480) {
            if (i == mode2d) idx2d = i;
            menuitem = [menu2d addItemWithTitle:[NSString stringWithFormat:@"%d %C %d",
                                                 validmode[i].xdim, 0xd7, validmode[i].ydim]
                                         action:nil
                                  keyEquivalent:@""];
            [menuitem setTag:i];
        }
    }

    if (idx2d >= 0) [videoMode2DPUButton selectItemWithTag:idx2d];
    if (idx3d >= 0) [videoMode3DPUButton selectItemWithTag:idx3d];
}

- (IBAction)fullscreenClicked:(id)sender
{
    [self populateVideoModes:NO];
}

- (IBAction)cancel:(id)sender
{
    if (inmodal) {
        [NSApp stopModalWithCode:STARTWIN_CANCEL];
    }
    quitevent = quitevent || quiteventonclose;
}

- (IBAction)start:(id)sender
{
    int mode = -1;

    mode = [[videoMode3DPUButton selectedItem] tag];
    if (mode >= 0) {
        settings->xdim3d = validmode[mode].xdim;
        settings->ydim3d = validmode[mode].ydim;
        settings->bpp3d = validmode[mode].bpp;
        settings->fullscreen = validmode[mode].fs;
    }

    mode = [[videoMode2DPUButton selectedItem] tag];
    if (mode >= 0) {
        settings->xdim2d = validmode[mode].xdim;
        settings->ydim2d = validmode[mode].ydim;
    }

    settings->forcesetup = [alwaysShowButton state] == NSOnState;

    if (inmodal) {
        [NSApp stopModalWithCode:STARTWIN_RUN];
    }
}

- (void)setupConfigMode
{
    [alwaysShowButton setState: (settings->forcesetup ? NSOnState : NSOffState)];
    [alwaysShowButton setEnabled:YES];

    [videoMode2DPUButton setEnabled:YES];
    [videoMode3DPUButton setEnabled:YES];
    [self populateVideoModes:YES];
    [fullscreenButton setEnabled:YES];
    [fullscreenButton setState: (settings->fullscreen ? NSOnState : NSOffState)];

    [cancelButton setEnabled:YES];
    [startButton setEnabled:YES];

    [tabView selectTabViewItem:tabConfig];
    [NSCursor unhide];  // Why should I need to do this?
}

- (void)setupMessagesMode:(BOOL)allowCancel
{
    [tabView selectTabViewItem:tabMessages];

    // disable all the controls on the Configuration page
    NSEnumerator *enumerator = [[[tabConfig view] subviews] objectEnumerator];
    NSControl *control;
    while (control = [enumerator nextObject]) {
        [control setEnabled:false];
    }

    [alwaysShowButton setEnabled:NO];

    [cancelButton setEnabled:allowCancel];
    [startButton setEnabled:false];
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

- (void)setSettings:(struct startwin_settings *)theSettings
{
    settings = theSettings;
}

@end

static EditorStartupWinController *startwin = nil;

int startwin_open(void)
{
    if (startwin != nil) return 1;

    @autoreleasepool {
        startwin = [[EditorStartupWinController alloc] initWithWindowNibName:@"startwin.editor"];
        if (startwin == nil) return -1;

        [startwin window];  // Forces the window controls on the controller to be initialised.
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
        [startwin putsMessage:[NSString stringWithUTF8String:s]];

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

int startwin_run(struct startwin_settings *settings)
{
    int retval;

    if (startwin == nil) return 0;

    @autoreleasepool {
        [startwin setSettings:settings];

        [startwin setupConfigMode];
        retval = [startwin modalRun];
        [startwin setupMessagesMode:NO];

        return retval;
    }
}
