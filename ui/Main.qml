import QtQuick

Window {
    id: root

    readonly property int surfaceMargin: 12
    readonly property int contentPad: 18
    readonly property int cellWidth: 84
    readonly property int cellHeight: 104
    readonly property int rowSpacing: 6
    readonly property int maxVisible: 9

    readonly property int visibleCells: Math.max(
        1, Math.min(browserView.count, maxVisible))

    width: surfaceMargin * 2 + contentPad * 2
           + visibleCells * cellWidth
           + Math.max(0, visibleCells - 1) * rowSpacing
    height: surfaceMargin * 2 + contentPad * 2 + cellHeight
            + (launchError !== "" ? errorLabel.height + 10 : 0)
    visible: false
    color: "transparent"

    readonly property color accent: "#00e5ff"
    readonly property color textStrong: "#f2f4f6"
    readonly property color textSoft: "#cbd5e1"
    readonly property color textFaint: "#94a3b8"

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
        anchors.margins: root.surfaceMargin
        radius: 16
        color: "#d91e293b"
        border.width: 1
        border.color: "#26f2f4f6"

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: root.contentPad
            spacing: 10

            ListView {
                id: browserView

                width: Math.min(contentWidth, parent.width)
                height: root.cellHeight
                orientation: ListView.Horizontal
                interactive: contentWidth > width
                clip: true
                model: browserRegistry
                currentIndex: root.selectedIndex
                spacing: root.rowSpacing
                delegate: BrowserDelegate {}

                onCountChanged: {
                    if (root.selectedIndex >= count)
                        root.selectedIndex = Math.max(0, count - 1)
                }
            }

            Text {
                id: emptyLabel

                width: parent.width
                visible: browserView.count === 0
                text: qsTr("No browsers found")
                color: root.textFaint
                font.family: "Jura"
                font.pixelSize: 14
                font.weight: Font.Light
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                id: errorLabel

                width: parent.width
                visible: root.launchError !== ""
                text: root.launchError
                color: "#f87171"
                font.family: "Jura"
                font.pixelSize: 12
                font.weight: Font.Light
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
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
            } else if (event.key === Qt.Key_Right
                       || event.key === Qt.Key_Down) {
                root.selectedIndex =
                        Math.min(root.selectedIndex + 1,
                                 browserView.count - 1)
                if (browserView.count > root.maxVisible)
                    browserView.positionViewAtIndex(root.selectedIndex,
                                                    ListView.Contain)
                event.accepted = true
            } else if (event.key === Qt.Key_Left
                       || event.key === Qt.Key_Up) {
                root.selectedIndex = Math.max(root.selectedIndex - 1, 0)
                if (browserView.count > root.maxVisible)
                    browserView.positionViewAtIndex(root.selectedIndex,
                                                    ListView.Contain)
                event.accepted = true
            } else if (event.key === Qt.Key_Return
                       || event.key === Qt.Key_Enter
                       || event.key === Qt.Key_Space) {
                root.launchAt(root.selectedIndex)
                event.accepted = true
            }
        }
    }
}
