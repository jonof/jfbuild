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

char * wmosx_filechooser(const char *initialdir, const char *initialfile, const char *type, int foropen, char **choice)
{
    NSSavePanel *panel = nil;
    NSModalResponse resp;
    NSArray *filetypes = [[NSArray alloc] initWithObjects:[NSString stringWithUTF8String:type], nil];
    NSURL *initialdirurl = [NSURL fileURLWithPath:[NSString stringWithUTF8String:initialdir]];

    *choice = NULL;

    if (foropen) {
        panel = [NSOpenPanel openPanel];    // Inherits from NSSavePanel.
    } else {
        panel = [NSSavePanel savePanel];
        if (initialfile) {
            [panel setNameFieldStringValue:[NSString stringWithUTF8String:initialfile]];
        }
    }
    [panel setAllowedFileTypes:filetypes];
    [panel setDirectoryURL:initialdirurl];

    resp = [panel runModal];
    if (resp == NSFileHandlingPanelOKButton) {
        NSURL *file = [panel URL];
        if ([file isFileURL]) {
            *choice = strdup([[file path] UTF8String]);
        }
    }

    [panel release];
    [filetypes release];
    [initialdirurl release];

    return resp == NSFileHandlingPanelOKButton ? 1 : 0;
}
