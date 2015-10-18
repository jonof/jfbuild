#import <Cocoa/Cocoa.h>

#include "compat.h"
#include "baselayer.h"
#include "build.h"
#include "mmulti.h"

#include <stdlib.h>

static struct {
    int fullscreen;
    int xdim3d, ydim3d, bpp3d;
    int forcesetup;

    int multiargc;
    char const * *multiargv;
    char *multiargstr;
} settings;

@interface StartupWinController : NSWindowController <NSWindowDelegate>
{
    BOOL quiteventonclose;
    NSMutableArray *modeslist3d;

    IBOutlet NSButton *alwaysShowButton;
    IBOutlet NSButton *fullscreenButton;
    IBOutlet NSTextView *messagesView;
    IBOutlet NSTabView *tabView;
    IBOutlet NSTabViewItem *tabConfig;
    IBOutlet NSTabViewItem *tabMessages;
    IBOutlet NSPopUpButton *videoMode3DPUButton;

    IBOutlet NSButton *singlePlayerButton;
    IBOutlet NSButton *multiPlayerButton;
    IBOutlet NSTextField *numPlayersField;
    IBOutlet NSStepper *numPlayersStepper;
    IBOutlet NSTextField *playerHostsLabel;
    IBOutlet NSTextField *playerHostsField;
    IBOutlet NSButton *peerToPeerButton;

    IBOutlet NSButton *cancelButton;
    IBOutlet NSButton *startButton;
}

- (void)dealloc;
- (void)closeQuietly;
- (void)populateVideoModes:(BOOL)firstTime;

- (IBAction)alwaysShowClicked:(id)sender;
- (IBAction)fullscreenClicked:(id)sender;

- (IBAction)multiPlayerModeClicked:(id)sender;
- (IBAction)peerToPeerModeClicked:(id)sender;

- (IBAction)cancel:(id)sender;
- (IBAction)start:(id)sender;

- (void)setupRunMode;
- (void)setupMessagesMode:(BOOL)allowCancel;
- (void)putsMessage:(NSString *)str;
- (void)setTitle:(NSString *)str;

- (void)setupPlayerHostsField;
@end

@implementation StartupWinController

- (void)windowDidLoad
{
    quiteventonclose = TRUE;
}

- (void)dealloc
{
    [modeslist3d release];
    [super dealloc];
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
    int i, mode3d, fullscreen = ([fullscreenButton state] == NSOnState);
    int idx3d = -1;
    int xdim, ydim, bpp = 8;

    if (firstTime) {
        xdim = settings.xdim3d;
        ydim = settings.ydim3d;
        bpp  = settings.bpp3d;
    } else {
        mode3d = [[modeslist3d objectAtIndex:[videoMode3DPUButton indexOfSelectedItem]] intValue];
        if (mode3d >= 0) {
            xdim = validmode[mode3d].xdim;
            ydim = validmode[mode3d].ydim;
            bpp = validmode[mode3d].bpp;
        }

    }
    mode3d = checkvideomode(&xdim, &ydim, bpp, fullscreen, 1);
    if (mode3d < 0) {
        int i, cd[] = { 32, 24, 16, 15, 8, 0 };
        for (i=0; cd[i]; ) { if (cd[i] >= bpp) i++; else break; }
        for ( ; cd[i]; i++) {
            mode3d = checkvideomode(&xdim, &ydim, cd[i], fullscreen, 1);
            if (mode3d < 0) continue;
            break;
        }
    }

    [modeslist3d release];
    [videoMode3DPUButton removeAllItems];

    modeslist3d = [[NSMutableArray alloc] init];

    for (i = 0; i < validmodecnt; i++) {
        if (fullscreen == validmode[i].fs) {
            if (i == mode3d) idx3d = [modeslist3d count];
            [modeslist3d addObject:[NSNumber numberWithInt:i]];
            [videoMode3DPUButton addItemWithTitle:[NSString stringWithFormat:@"%d %C %d %d-bpp",
                                                   validmode[i].xdim, 0xd7, validmode[i].ydim, validmode[i].bpp]];
        }
    }

    if (idx3d >= 0) [videoMode3DPUButton selectItemAtIndex:idx3d];
}

- (IBAction)alwaysShowClicked:(id)sender
{
}

- (IBAction)fullscreenClicked:(id)sender
{
    [self populateVideoModes:NO];
}

- (IBAction)multiPlayerModeClicked:(id)sender
{
    [singlePlayerButton setState:(sender == singlePlayerButton ? NSOnState : NSOffState)];
    [multiPlayerButton setState:(sender == multiPlayerButton ? NSOnState : NSOffState)];

    [peerToPeerButton setEnabled:(sender == multiPlayerButton)];
    [playerHostsField setEnabled:(sender == multiPlayerButton)];
    [self setupPlayerHostsField];

    BOOL enableNumPlayers = (sender == multiPlayerButton) && ([peerToPeerButton state] == NSOffState);
    [numPlayersField setEnabled:enableNumPlayers];
    [numPlayersStepper setEnabled:enableNumPlayers];
}

- (IBAction)peerToPeerModeClicked:(id)sender
{
    BOOL enableNumPlayers = ([peerToPeerButton state] == NSOffState);
    [numPlayersField setEnabled:enableNumPlayers];
    [numPlayersStepper setEnabled:enableNumPlayers];
    [self setupPlayerHostsField];
}

- (void)setupPlayerHostsField
{
    if ([peerToPeerButton state] == NSOffState) {
        [playerHostsLabel setStringValue:@"Host address:"];
        [playerHostsField setPlaceholderString:@"Leave empty to host the game."];
    } else {
        [playerHostsLabel setStringValue:@"Player addresses:"];
        [playerHostsField setPlaceholderString:@"List each address in order. Use * to indicate this machine's position."];
    }
}

- (void)windowWillClose:(NSNotification *)notification
{
    [NSApp stopModalWithCode:STARTWIN_CANCEL];
    quitevent = quitevent || quiteventonclose;
}

- (IBAction)cancel:(id)sender
{
    [NSApp stopModalWithCode:STARTWIN_CANCEL];
    quitevent = quitevent || quiteventonclose;
}

- (IBAction)start:(id)sender
{
    int retval;
    int mode = [[modeslist3d objectAtIndex:[videoMode3DPUButton indexOfSelectedItem]] intValue];
    if (mode >= 0) {
        settings.xdim3d = validmode[mode].xdim;
        settings.ydim3d = validmode[mode].ydim;
        settings.bpp3d = validmode[mode].bpp;
        settings.fullscreen = validmode[mode].fs;
    }

    if ([multiPlayerButton state] == NSOnState) {
        @autoreleasepool {
            NSMutableArray *args = [[[NSMutableArray alloc] init] autorelease];
            NSString *str;
            int argstrlen = 0, i;
            char *argstrptr;

            // The game type parameter.
            if ([peerToPeerButton state] == NSOnState) {
                str = [[NSString alloc] initWithString:@"-n1"];
            } else {
                str = [[NSString alloc] initWithFormat:@"-n0:%d", [numPlayersField intValue]];
            }
            [str autorelease];
            [args addObject:str];
            argstrlen += [str lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1;

            // The peer list.
            NSCharacterSet *split = [NSCharacterSet characterSetWithCharactersInString:@" ,\n\t"];
            NSArray *parts = [[playerHostsField stringValue] componentsSeparatedByCharactersInSet:split];
            NSEnumerator *iterparts = [parts objectEnumerator];

            while (str = (NSString *)[iterparts nextObject]) {
                if ([str length] == 0) continue;
                [args addObject:str];
                argstrlen += [str lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1;
            }

            settings.multiargc = [args count];
            settings.multiargv = (char const * *)malloc(settings.multiargc * sizeof(char *));
            settings.multiargstr = (char *)malloc(argstrlen);

            argstrptr = settings.multiargstr;
            for (i = 0; i < settings.multiargc; i++) {
                str = (NSString *)[args objectAtIndex:i];

                strcpy(argstrptr, [str cStringUsingEncoding:NSUTF8StringEncoding]);
                settings.multiargv[i] = argstrptr;
                argstrptr += strlen(argstrptr) + 1;
            }
        }

        retval = STARTWIN_RUN_MULTI;
    } else {
        settings.multiargc = 0;
        settings.multiargv = NULL;
        settings.multiargstr = NULL;

        retval = STARTWIN_RUN;
    }

    settings.forcesetup = [alwaysShowButton state] == NSOnState;

    [NSApp stopModalWithCode:retval];
}

- (void)setupRunMode
{
    [alwaysShowButton setState: (settings.forcesetup ? NSOnState : NSOffState)];

    getvalidmodes();
    [videoMode3DPUButton setEnabled:YES];
    [self populateVideoModes:YES];
    [fullscreenButton setEnabled:YES];
    [fullscreenButton setState: (settings.fullscreen ? NSOnState : NSOffState)];

    [singlePlayerButton setEnabled:YES];
    [singlePlayerButton setState:NSOnState];
    [multiPlayerButton setEnabled:YES];
    [multiPlayerButton setState:NSOffState];
    [numPlayersField setEnabled:NO];
    [numPlayersField setIntValue:2];
    [numPlayersStepper setEnabled:NO];
    [numPlayersStepper setMaxValue:MAXPLAYERS];
    [peerToPeerButton setEnabled:NO];
    [playerHostsField setEnabled:NO];
    [self setupPlayerHostsField];

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

@end

static StartupWinController *startwin = nil;

int startwin_open(void)
{
    if (startwin != nil) return 1;

    startwin = [[StartupWinController alloc] initWithWindowNibName:@"startwin.game"];
    if (startwin == nil) return -1;

    [startwin window];  // Forces the window controls on the controller to be initialised.
    [startwin setupMessagesMode:YES];
    [startwin showWindow:nil];

    return 0;
}

int startwin_close(void)
{
    if (startwin == nil) return 1;

    [startwin closeQuietly];
    [startwin release];
    startwin = nil;

    return 0;
}

int startwin_puts(const char *s)
{
    NSString *ns;

    if (!s) return -1;
    if (startwin == nil) return 1;

    ns = [[NSString alloc] initWithUTF8String:s];
    [startwin putsMessage:ns];
    [ns release];

    return 0;
}

int startwin_settitle(const char *s)
{
    NSString *ns;

    if (!s) return -1;
    if (startwin == nil) return 1;

    ns = [[NSString alloc] initWithUTF8String:s];
    [startwin setTitle:ns];
    [ns release];

    return 0;
}

int startwin_idle(void *v)
{
    return 0;
}

extern int xdimgame, ydimgame, bppgame, forcesetup;

int startwin_run(void)
{
    int retval;

    if (startwin == nil) return 0;

    settings.fullscreen = fullscreen;
    settings.xdim3d = xdimgame;
    settings.ydim3d = ydimgame;
    settings.bpp3d = bppgame;
    settings.forcesetup = forcesetup;

    [startwin setupRunMode];

    switch ([NSApp runModalForWindow:[startwin window]]) {
        case STARTWIN_RUN_MULTI: retval = STARTWIN_RUN_MULTI; break;
        case STARTWIN_RUN: retval = STARTWIN_RUN; break;
        case STARTWIN_CANCEL: retval = STARTWIN_CANCEL; break;
        default: retval = -1;
    }

    [startwin setupMessagesMode:(retval == STARTWIN_RUN_MULTI)];

    if (retval) {
        fullscreen = settings.fullscreen;
        xdimgame = settings.xdim3d;
        ydimgame = settings.ydim3d;
        bppgame = settings.bpp3d;
        forcesetup = settings.forcesetup;

        if (retval == STARTWIN_RUN_MULTI) {
            if (!initmultiplayersparms(settings.multiargc, settings.multiargv)) {
                retval = STARTWIN_RUN;
            }
            free(settings.multiargv);
            free(settings.multiargstr);
        }
    }

    return retval;
}
