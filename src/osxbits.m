#include "osxbits.h"
#import <AppKit/AppKit.h>

int osx_msgbox(char *name, char *msg)
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

int osx_ynbox(char *name, char *msg)
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
