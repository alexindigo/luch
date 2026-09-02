import QtQuick

// `luch --settings` — the minimal settings UI (escape hatch for
// watcher-less environments; the daemon tray exposes the same prefs).
Window {
    id: settingsWindow

    readonly property int contentPad: 18
    readonly property int rowHeight: 30

    width: 320
    height: contentPad * 2 + rowHeight * 2 + 12
    visible: true
    title: qsTr("luch — settings")
    color: "#1e293b"

    Column {
        anchors.fill: parent
        anchors.margins: settingsWindow.contentPad
        spacing: 12

        Item {
            width: parent.width
            height: settingsWindow.rowHeight

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                text: qsTr("Always show bottom panel")
                color: "#cbd5e1"
                font.family: "Jura"
                font.pixelSize: 13
                font.weight: Font.Light
            }

            Rectangle {
                id: dissectionSwitch

                property bool on: settings.showDissection

                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                width: 40
                height: 20
                radius: height / 2
                color: on ? "#3300e5ff" : "#22f2f4f6"
                border.width: 1
                border.color: on ? "#ff00e5ff" : "#4d94a3b8"

                Rectangle {
                    x: dissectionSwitch.on ? parent.width - width - 3 : 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: 14
                    height: 14
                    radius: 7
                    color: dissectionSwitch.on ? "#00e5ff" : "#94a3b8"

                    Behavior on x {
                        NumberAnimation { duration: 100 }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked:
                        settings.showDissection = !settings.showDissection
                }

                Accessible.role: Accessible.CheckBox
                Accessible.name: qsTr("Always show bottom panel")
                Accessible.checked: dissectionSwitch.on
                Accessible.onPressAction:
                    settings.showDissection = !settings.showDissection
            }
        }

        Item {
            width: parent.width
            height: settingsWindow.rowHeight

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                text: qsTr("Log payload")
                color: "#cbd5e1"
                font.family: "Jura"
                font.pixelSize: 13
                font.weight: Font.Light
            }

            Rectangle {
                id: logSwitch

                property bool on: settings.logPayload

                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                width: 40
                height: 20
                radius: height / 2
                color: on ? "#3300e5ff" : "#22f2f4f6"
                border.width: 1
                border.color: on ? "#ff00e5ff" : "#4d94a3b8"

                Rectangle {
                    x: logSwitch.on ? parent.width - width - 3 : 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: 14
                    height: 14
                    radius: 7
                    color: logSwitch.on ? "#00e5ff" : "#94a3b8"

                    Behavior on x {
                        NumberAnimation { duration: 100 }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settings.logPayload = !settings.logPayload
                }

                Accessible.role: Accessible.CheckBox
                Accessible.name: qsTr("Log payload")
                Accessible.checked: logSwitch.on
                Accessible.onPressAction:
                    settings.logPayload = !settings.logPayload
            }
        }
    }
}
