QT += core gui widgets

TARGET   = ScreenRecord
TEMPLATE = app
CONFIG  += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    recorder.cpp \
    settingsdialog.cpp

HEADERS += \
    mainwindow.h \
    recorder.h \
    settingsdialog.h

win32 {
    LIBS += -luser32          # SetWindowDisplayAffinity
    DEFINES += NOMINMAX
}

macx {
    HEADERS += machelper.h
    OBJECTIVE_SOURCES += machelper.mm   # NSWindowSharingNone (キャプチャ除外)
    LIBS += -framework AppKit
}
