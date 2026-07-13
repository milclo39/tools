import QtQuick 2.12
import QtQuick.Controls 2.12

ApplicationWindow {
    visible: true
    width: 620
    height: 420
    title: "Wake on LAN"

    ListModel {
        id: deviceModel
    }

    function refreshDevices() {
        var devices = wolHandler.networkDevices()
        deviceModel.clear()
        for (var i = 0; i < devices.length; ++i) {
            deviceModel.append(devices[i])
        }
    }

    Component.onCompleted: refreshDevices()

    Column {
        spacing: 10
        anchors.fill: parent
        anchors.margins: 12

        Row {
            spacing: 10
            width: parent.width

            TextField {
                id: macInput
                placeholderText: "XX:XX:XX:XX:XX:XX"
                font.pointSize: 14
                selectByMouse: true
                width: parent.width - port.width - sendButton.width - parent.spacing * 2
            }
            TextField {
                id: port
                placeholderText: "Port"
                font.pointSize: 14
                selectByMouse: true
                width: 70
                text: "7"
                validator: IntValidator {
                    bottom: 1
                    top: 65535
                }
            }
            Button {
                id: sendButton
                text: "Send WOL"
                font.pointSize: 14
                onClicked: wolHandler.sendWOL(macInput.text, parseInt(port.text))
            }
        }

        Row {
            spacing: 10
            width: parent.width

            Label {
                text: "Network devices"
                font.pointSize: 14
                font.bold: true
                verticalAlignment: Text.AlignVCenter
                height: refreshButton.height
                width: parent.width - refreshButton.width - parent.spacing
            }
            Button {
                id: refreshButton
                text: "Refresh"
                font.pointSize: 12
                onClicked: refreshDevices()
            }
        }

        Rectangle {
            width: parent.width
            height: parent.height - y
            border.color: "#c8c8c8"
            color: "#ffffff"

            Column {
                anchors.fill: parent

                Rectangle {
                    width: parent.width
                    height: 34
                    color: "#eeeeee"

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10

                        Label {
                            text: "IP Address"
                            font.bold: true
                            verticalAlignment: Text.AlignVCenter
                            width: 190
                            height: parent.height
                        }
                        Label {
                            text: "MAC Address"
                            font.bold: true
                            verticalAlignment: Text.AlignVCenter
                            width: 210
                            height: parent.height
                        }
                        Label {
                            text: "Type"
                            font.bold: true
                            verticalAlignment: Text.AlignVCenter
                            width: parent.width - 420
                            height: parent.height
                        }
                    }
                }

                ListView {
                    id: deviceList
                    width: parent.width
                    height: parent.height - 34
                    clip: true
                    model: deviceModel

                    delegate: Rectangle {
                        width: deviceList.width
                        height: 34
                        color: mouseArea.containsMouse ? "#e8f1ff" : (index % 2 === 0 ? "#ffffff" : "#f8f8f8")

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10

                            Label {
                                text: ipAddress
                                verticalAlignment: Text.AlignVCenter
                                width: 190
                                height: parent.height
                            }
                            Label {
                                text: macAddress
                                verticalAlignment: Text.AlignVCenter
                                width: 210
                                height: parent.height
                            }
                            Label {
                                text: type
                                verticalAlignment: Text.AlignVCenter
                                width: parent.width - 420
                                height: parent.height
                            }
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: macInput.text = macAddress
                        }
                    }
                }
            }
        }
    }
}
