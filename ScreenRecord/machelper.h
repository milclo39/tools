#ifndef MACHELPER_H
#define MACHELPER_H

#include <QtGui/qwindowdefs.h>

// macOS: NSWindow.sharingType = NSWindowSharingNone を設定し、
// このウィンドウを画面キャプチャ (avfoundation / スクリーンショット) から除外する。
// Windows の SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE) に相当。
void macExcludeWindowFromCapture(WId wid);

#endif // MACHELPER_H
