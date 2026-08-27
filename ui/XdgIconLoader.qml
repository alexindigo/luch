import QtQuick
import XdgIcon

Item {
    id: iconRoot

    property string iconName: ""
    property string fallbackText: ""

    XdgIcon {
        id: xdgIcon
        name: iconRoot.iconName
        size: 48
    }

    Image {
        anchors.fill: parent
        source: xdgIcon.path
        fillMode: Image.PreserveAspectFit
        visible: xdgIcon.found
        smooth: true
        mipmap: true
    }

    Rectangle {
        anchors.fill: parent
        visible: !xdgIcon.found
        radius: width / 2
        color: "#e2e8f0"

        Text {
            anchors.centerIn: parent
            text: iconRoot.fallbackText
            color: "#475569"
            font.family: "Jura"
            font.pixelSize: 18
            font.weight: Font.Light
        }
    }
}
