#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ScreenRecord"));
    app.setOrganizationName(QStringLiteral("ScreenRecord"));
    app.setQuitOnLastWindowClosed(true);

    MainWindow w;
    w.show();

    return app.exec();
}
