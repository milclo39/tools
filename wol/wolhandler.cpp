#include "wolhandler.h"
#include <QProcess>
#include <QRegularExpression>
#include <QVariantMap>
#include <QtNetwork/QUdpSocket>

WOLHandler::WOLHandler(QObject *parent) : QObject(parent)
{
}

void WOLHandler::sendWOL(QString macAddress, int port)
{
    QByteArray mac = QByteArray::fromHex(macAddress.remove(":").toLatin1());
    if (mac.size() != 6) {
        qDebug() << "Invalid MAC address";
        return;
    }
    QByteArray magicPacket(6, char(-1));
    for (int i = 0; i < 16; ++i){
        magicPacket.append(mac);
    }
    QUdpSocket udpSocket;
    udpSocket.writeDatagram(magicPacket, QHostAddress::Broadcast, port);
    qDebug() << "Sent WOL packet to" << macAddress;
}

QVariantList WOLHandler::networkDevices()
{
    QVariantList devices;
    QProcess arp;
    arp.start("arp", QStringList() << "-a");
    if (!arp.waitForFinished(5000)) {
        qDebug() << "Failed to run arp -a";
        return devices;
    }

    const QString output = QString::fromLocal8Bit(arp.readAllStandardOutput());
    const QRegularExpression linePattern(
        "\\b(\\d{1,3}(?:\\.\\d{1,3}){3})\\s+"
        "([0-9a-fA-F]{2}(?:-[0-9a-fA-F]{2}){5})\\s+"
        "(\\S+)");
    QRegularExpressionMatchIterator matches = linePattern.globalMatch(output);

    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        QVariantMap device;
        device.insert("ipAddress", match.captured(1));
        device.insert("macAddress", match.captured(2).replace("-", ":").toUpper());
        device.insert("type", match.captured(3));
        devices.append(device);
    }

    return devices;
}
