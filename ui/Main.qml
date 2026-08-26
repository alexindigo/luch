import QtQuick

Window {
    id: root

    width: 480
    height: Math.min(24 + 64 + browserView.contentHeight + 20,
                     24 + 64 + 9 * rowHeight + 20)
    visible: false
    color: "transparent"

    readonly property int rowHeight: 48
    readonly property color accent: "#00e5ff"
    readonly property color ink: "#1e293b"
    readonly property color inkSoft: "#475569"
    readonly property color inkFaint: "#94a3b8"

    property int selectedIndex: 0
    property string launchError: ""

    function launchAt(index: int) {
        const exec = browserRegistry.execAt(index)
        if (exec === "")
            return
        launchError = ""
        launcher.launch(exec, incomingUrl)
    }

    Connections {
        target: launcher

        function onLaunchFailed(message: string) {
            root.launchError = message
        }
    }

    Rectangle {
        id: surface

        anchors.fill: parent
        anchors.margins: 12
        radius: 16
        color: "#ebf2f4f6"
        border.width: 1
        border.color: "#4064748b"

        Column {
            id: content

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            spacing: 12

            Text {
                id: urlBanner

                width: parent.width
                text: incomingUrl
                color: root.inkSoft
                font.family: "Jura"
                font.pixelSize: 14
                font.weight: Font.Light
                elide: Text.ElideMiddle
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                width: parent.width
                visible: root.launchError !== ""
                text: root.launchError
                color: "#b91c1c"
                font.family: "Jura"
                font.pixelSize: 12
                font.weight: Font.Light
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                width: parent.width
                height: 1
                color: "#2064748b"
            }

            ListView {
                id: browserView

                width: parent.width
                height: contentHeight
                interactive: false
                model: browserRegistry
                currentIndex: root.selectedIndex
                delegate: BrowserDelegate {}

                onCountChanged: {
                    if (root.selectedIndex >= count)
                        root.selectedIndex = Math.max(0, count - 1)
                }
            }
        }
    }

    Item {
        id: keyHandler

        anchors.fill: parent
        focus: true

        Keys.onPressed: (event) => {
            if (event.modifiers & Qt.ControlModifier
                    && event.key === Qt.Key_C) {
                launcher.copyToClipboard(incomingUrl)
                root.close()
                event.accepted = true
                return
            }
            if (event.key >= Qt.Key_1 && event.key <= Qt.Key_9) {
                const index = event.key - Qt.Key_1
                if (index < browserView.count)
                    root.launchAt(index)
                event.accepted = true
            } else if (event.key === Qt.Key_Down) {
                root.selectedIndex =
                        Math.min(root.selectedIndex + 1,
                                 browserView.count - 1)
                event.accepted = true
            } else if (event.key === Qt.Key_Up) {
                root.selectedIndex = Math.max(root.selectedIndex - 1, 0)
                event.accepted = true
            } else if (event.key === Qt.Key_Return
                       || event.key === Qt.Key_Enter) {
                root.launchAt(root.selectedIndex)
                event.accepted = true
            }
        }
    }
}
