#ifndef WOLHANDLER_H
#define WOLHANDLER_H

#include <QObject>
#include <QString>
#include <QVariantList>

class WOLHandler : public QObject
{
    Q_OBJECT
public:
    explicit WOLHandler(QObject *parent = nullptr);
    Q_INVOKABLE QVariantList networkDevices();

public slots:
    void sendWOL(QString macAddress, int port);
};

#endif // WOLHANDLER_H
