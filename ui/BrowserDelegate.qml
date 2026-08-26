import QtQuick

Item {
    id: delegateRoot

    width: ListView.view ? ListView.view.width : 0
    height: root.rowHeight

    readonly property bool selected: ListView.isCurrentItem

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        radius: 8
        color: delegateRoot.selected ? "#1a00e5ff"
                                     : (mouse.containsMouse ? "#0d64748b"
                                                            : "transparent")
        border.width: delegateRoot.selected ? 1 : 0
        border.color: "#6600e5ff"
    }

    Item {
        id: iconSlot

        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: 32
        height: 32

        Loader {
            id: iconLoader

            anchors.fill: parent
            source: "XdgIconLoader.qml"

            onLoaded: item.iconName = model.iconName
        }

        Rectangle {
            anchors.fill: parent
            visible: iconLoader.status !== Loader.Ready
            radius: 16
            color: "#e2e8f0"

            Text {
                anchors.centerIn: parent
                text: model.name.charAt(0).toUpperCase()
                color: "#475569"
                font.family: "Jura"
                font.pixelSize: 16
                font.weight: Font.Light
            }
        }
    }

    Text {
        id: shortcutHint

        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: 12
        text: model.shortcutHint
        color: "#94a3b8"
        font.family: "Jura"
        font.pixelSize: 13
        font.weight: Font.Light
        horizontalAlignment: Text.AlignRight
    }

    Text {
        id: nameLabel

        anchors.left: iconSlot.right
        anchors.leftMargin: 12
        anchors.right: shortcutHint.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: model.name
        color: delegateRoot.selected ? "#0c4a6e" : "#1e293b"
        font.family: "Jura"
        font.pixelSize: 15
        font.weight: Font.Light
        elide: Text.ElideRight
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        hoverEnabled: true

        onEntered: root.selectedIndex = index
        onClicked: root.launchAt(index)
    }
}
