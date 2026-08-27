import QtQuick

Item {
    id: delegateRoot

    width: root.cellWidth
    height: root.cellHeight

    readonly property bool selected: index === root.selectedIndex

    Text {
        id: shortcutHint

        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        text: model.shortcutHint
        color: delegateRoot.selected ? root.accent : root.textFaint
        font.family: "Jura"
        font.pixelSize: 12
        font.weight: Font.Light
        horizontalAlignment: Text.AlignHCenter
    }

    Rectangle {
        id: tile

        anchors.top: shortcutHint.bottom
        anchors.topMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter
        width: 56
        height: 56
        radius: 14
        color: mouse.containsMouse ? "#ffffff" : "#eef1f5"
        border.width: delegateRoot.selected ? 2 : 1
        border.color: delegateRoot.selected ? root.accent : "#33f2f4f6"

        Item {
            anchors.fill: parent
            anchors.margins: 7

            Loader {
                id: iconLoader

                anchors.fill: parent
                source: "XdgIconLoader.qml"

                onLoaded: {
                    item.iconName = model.iconName
                    item.fallbackText = model.name.charAt(0).toUpperCase()
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: iconLoader.status !== Loader.Ready
                radius: width / 2
                color: "#e2e8f0"

                Text {
                    anchors.centerIn: parent
                    text: model.name.charAt(0).toUpperCase()
                    color: "#475569"
                    font.family: "Jura"
                    font.pixelSize: 18
                    font.weight: Font.Light
                }
            }
        }
    }

    Text {
        id: nameLabel

        anchors.top: tile.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.right: parent.right
        text: model.name
        color: delegateRoot.selected ? root.textStrong : root.textSoft
        font.family: "Jura"
        font.pixelSize: 11
        font.weight: Font.Light
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        hoverEnabled: true

        onEntered: root.selectedIndex = index
        onClicked: root.launchAt(index)
    }
}
