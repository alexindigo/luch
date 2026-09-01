import QtQuick

Window {
    id: root

    readonly property int surfaceMargin: 12
    readonly property int contentPad: 18
    readonly property int cellWidth: 84
    readonly property int cellHeight: 104
    readonly property int rowSpacing: 6
    readonly property int maxVisible: 9
    readonly property int footerSpacing: 10
    readonly property real widthCapFactor: 0.8

    // Queue satellites (locked: only when more than one target):
    readonly property int satelliteOverhang: 14
    readonly property int badgeOverhang: 14
    readonly property int dotsRowHeight: 8
    readonly property int dotsSpacing: 6
    readonly property bool queued: queue.count > 1

    readonly property int visibleCells: Math.max(
        1, Math.min(browserView.count, maxVisible))
    readonly property int chrome: surfaceMargin * 2 + contentPad * 2
    readonly property int stripWidth: visibleCells * cellWidth
        + Math.max(0, visibleCells - 1) * rowSpacing
    readonly property real widthCap: widthCapFactor * Screen.width

    width: chrome + (queued ? 2 * satelliteOverhang : 0)
           + Math.ceil(Math.max(stripWidth,
                                Math.min(footerBar.naturalWidth + 2,
                                         widthCap)))
    height: chrome + (queued ? 2 * badgeOverhang : 0) + cellHeight
            + (footerBar.height > 0 ? footerSpacing + footerBar.height : 0)
            + (launchError !== "" ? errorLabel.height + 10 : 0)
            + (queued ? 10 + dotsRowHeight : 0)
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
        launcher.launch(exec, browserRegistry.idAt(index))
    }

    Connections {
        target: queue

        function onCurrentChanged() {
            root.selectedIndex = 0
        }
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
        anchors.leftMargin: root.surfaceMargin
            + (root.queued ? root.satelliteOverhang : 0)
        anchors.rightMargin: root.surfaceMargin
            + (root.queued ? root.satelliteOverhang : 0)
        anchors.topMargin: root.surfaceMargin
            + (root.queued ? root.badgeOverhang : 0)
        anchors.bottomMargin: root.surfaceMargin
            + (root.queued ? root.badgeOverhang : 0)
        radius: 16
        color: "#e61e293b"
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
                x: (parent.width - width) / 2
                height: root.cellHeight
                orientation: ListView.Horizontal
                interactive: contentWidth > width
                clip: true
                model: browserRegistry
                currentIndex: root.selectedIndex
                spacing: root.rowSpacing
                delegate: BrowserDelegate {}
                Accessible.role: Accessible.List
                Accessible.name: qsTr("Browsers")

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
                Accessible.role: Accessible.Alert
            }

            TargetFooter {
                id: footerBar

                width: parent.width
                accent: root.accent
                textStrong: root.textStrong
                textFaint: root.textFaint
                scheme: queue.currentScheme
                hostOrDir: queue.currentHostOrDir
                middle: queue.currentMiddle
                tail: queue.currentTail
                widthCapped: footerBar.naturalWidth > root.widthCap

                onCopyRequested: launcher.copyToClipboard(queue.currentRaw)
            }

            Row {
                id: dotsRow

                visible: root.queued
                height: visible ? root.dotsRowHeight : 0
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: root.dotsSpacing

                Repeater {
                    model: queue.count

                    Rectangle {
                        width: index === queue.cursor ? 18 : 8
                        height: root.dotsRowHeight
                        radius: 4
                        color: index === queue.cursor ? root.accent
                                                      : root.textFaint
                    }
                }
            }
        }
    }

    QueueChrome {
        id: queueChrome

        surface: surface
        visible: root.queued
        accent: root.accent
        textStrong: root.textStrong
        textFaint: root.textFaint
    }

    Item {
        id: keyHandler

        anchors.fill: parent
        focus: true

        Keys.onPressed: (event) => {
            if (event.modifiers & Qt.ControlModifier
                    && event.key === Qt.Key_C) {
                launcher.copyToClipboard(queue.currentRaw)
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_Escape) {
                if (event.modifiers & Qt.ShiftModifier)
                    queue.clear()
                else
                    queue.removeCurrent()
                event.accepted = true
                return
            }
            if (event.modifiers & Qt.ShiftModifier
                    && (event.key === Qt.Key_Right
                        || event.key === Qt.Key_Left)) {
                queue.moveCursor(event.key === Qt.Key_Right ? 1 : -1)
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
