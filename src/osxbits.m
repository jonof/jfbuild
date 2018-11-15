#include "osxbits.h"
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

char *osx_gethomedir(void)
{
    NSURL *url = [[NSFileManager defaultManager] URLForDirectory:NSUserDirectory
                                                        inDomain:NSUserDomainMask
                                               appropriateForURL:nil
                                                          create:FALSE
                                                           error:nil];
    if (url && [url isFileURL]) {
        return strdup([[url path] UTF8String]);
    }
    return NULL;
}

char *osx_getappdir(void)
{
    NSString *path = [[NSBundle mainBundle] resourcePath];
    if (path) {
        return strdup([path cStringUsingEncoding:NSUTF8StringEncoding]);
    }
    return NULL;
}

char *osx_getsupportdir(int global)
{
    NSURL *url = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory
                                                        inDomain:global ? NSLocalDomainMask : NSUserDomainMask
                                               appropriateForURL:nil
                                                          create:FALSE
                                                           error:nil];
    if (url && [url isFileURL]) {
        return strdup([[url path] UTF8String]);
    }
    return NULL;
}

int osx_msgbox(const char *name, const char *msg)
{
	NSString *mmsg = [[NSString alloc] initWithUTF8String: msg];
	NSAlert *alert = [[NSAlert alloc] init];

    [alert addButtonWithTitle: @"OK"];
	[alert setInformativeText: mmsg];
	[alert setAlertStyle: NSInformationalAlertStyle];

    [alert runModal];

    [alert release];
	[mmsg release];
	return 0;
}

int osx_ynbox(const char *name, const char *msg)
{
	NSString *mmsg = [[NSString alloc] initWithUTF8String: msg];
    NSAlert *alert = [[NSAlert alloc] init];
	int r;

	[alert addButtonWithTitle:@"Yes"];
	[alert addButtonWithTitle:@"No"];
	[alert setInformativeText: mmsg];
	[alert setAlertStyle: NSInformationalAlertStyle];

	r = ([alert runModal] == NSAlertFirstButtonReturn);

	[alert release];
	[mmsg release];
	return r;
}

char * osx_filechooser(const char *initialdir, const char *type, int foropen)
{
    NSSavePanel *panel = nil;
    NSModalResponse resp;
    NSArray *filetypes = [[NSArray alloc] initWithObjects:[NSString stringWithUTF8String:type], nil];
    NSURL *initialurl = [NSURL fileURLWithPath:[NSString stringWithUTF8String:initialdir]];
    char * ret;

    if (foropen) {
        panel = [NSOpenPanel openPanel];    // Inherits from NSSavePanel.
    } else {
        panel = [NSSavePanel savePanel];
    }
    [panel setAllowedFileTypes:filetypes];
    [panel setDirectoryURL:[initialurl baseURL]];
    [panel setNameFieldStringValue:[initialurl lastPathComponent]];

    resp = [panel runModal];
    if (resp == NSFileHandlingPanelOKButton) {
        NSURL *file = [panel URL];
        if ([file isFileURL]) {
            ret = strdup([[file path] UTF8String]);
        }
    } else {
        ret = strdup("");
    }

    [panel release];
    [filetypes release];
    [initialurl release];

    return ret;
}
