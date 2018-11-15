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
    if (url) {
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
    if (url) {
        return strdup([[url path] UTF8String]);
    }
    return NULL;
}

int osx_msgbox(const char *name, const char *msg)
{
	NSString *mmsg = [[NSString alloc] initWithUTF8String: msg];

#if defined(MAC_OS_X_VERSION_10_3) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_3)
	NSAlert *alert = [[NSAlert alloc] init];
	[alert addButtonWithTitle: @"OK"];
	[alert setInformativeText: mmsg];
	[alert setAlertStyle: NSInformationalAlertStyle];

	[alert runModal];

	[alert release];

#else
	NSRunAlertPanel(nil, mmsg, @"OK", nil, nil);
#endif

	[mmsg release];
	return 0;
}

int osx_ynbox(const char *name, const char *msg)
{
	NSString *mmsg = [[NSString alloc] initWithUTF8String: msg];
	int r;

#if defined(MAC_OS_X_VERSION_10_3) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_3)
	NSAlert *alert = [[NSAlert alloc] init];

	[alert addButtonWithTitle:@"Yes"];
	[alert addButtonWithTitle:@"No"];
	[alert setInformativeText: mmsg];
	[alert setAlertStyle: NSInformationalAlertStyle];

	r = ([alert runModal] == NSAlertFirstButtonReturn);

	[alert release];
#else
	r = (NSRunAlertPanel(nil, mmsg, @"Yes", @"No", nil) == NSAlertDefaultReturn);
#endif

	[mmsg release];
	return r;
}
