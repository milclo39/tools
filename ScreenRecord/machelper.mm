#include "machelper.h"

#import <AppKit/AppKit.h>

void macExcludeWindowFromCapture(WId wid)
{
    NSView *view = reinterpret_cast<NSView *>(wid);   // Qt の winId() は NSView*
    NSWindow *window = [view window];
    if (window)
        [window setSharingType:NSWindowSharingNone];
}
