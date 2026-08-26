import QtQuick

Window {
    id: root

    width: 480
    height: 220
    visible: false
    color: "transparent"

    Rectangle {
        id: surface

        anchors.fill: parent
        anchors.margins: 12
        radius: 16
        color: "#ebf2f4f6"
        border.width: 1
        border.color: "#4064748b"

        Column {
            anchors.centerIn: parent
            spacing: 10

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Luch")
                color: "#1e293b"
                font.family: "Jura"
                font.pixelSize: 26
                font.weight: Font.Light
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: incomingUrl
                color: "#475569"
                font.family: "Jura"
                font.pixelSize: 14
                font.weight: Font.Light
                elide: Text.ElideMiddle
                width: Math.min(implicitWidth, surface.width - 48)
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Esc to dismiss")
                color: "#94a3b8"
                font.family: "Jura"
                font.pixelSize: 12
                font.weight: Font.Light
            }
        }
    }
}
