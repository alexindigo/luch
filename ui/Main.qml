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
    readonly property bool lightsVisible: lightsStrip.chain.length > 0
    readonly property bool pillsVisible: payloadVariants.length > 1

    width: chrome + (queued ? 2 * satelliteOverhang : 0)
           + Math.ceil(Math.max(stripWidth,
                                Math.min(footerBar.naturalWidth + 2,
                                         widthCap),
                                lightsVisible ? lightsStrip.implicitWidth
                                              : 0)
                        + (pillsVisible ? variantPills.implicitWidth
                                        + 12 : 0))
    height: chrome + (queued ? 2 * badgeOverhang : 0) + cellHeight
            + (lightsVisible ? lightsStrip.implicitHeight + 10 : 0)
            + (footerBar.height > 0 ? footerSpacing + footerBar.height : 0)
            + (launchError !== "" ? errorLabel.height + 10 : 0)
            + (redVerdict ? warningLine.height + 10 : 0)
            + (dissectionVisible ? dissectionSpacing
                                  + dissectionPanel.implicitHeight : 0)
            + (queued ? 10 + dotsRowHeight : 0)
    readonly property int dissectionSpacing: 10
    readonly property bool dissectionVisible:
        dissectionPanel.pinned || redVerdict
    visible: false
    color: "transparent"

    readonly property color accent: "#00e5ff"
    readonly property color textStrong: "#f2f4f6"
    readonly property color textSoft: "#cbd5e1"
    readonly property color textFaint: "#94a3b8"

    property int selectedIndex: 0
    property string launchError: ""

    // The one URL the main panel knows. Variants = the original plus
    // every distinct URL the chain produced; the last one auto-selects
    // as it materializes. Footer, launch and copy all consume the
    // presented URL.
    property var payloadVariants: []
    property int selectedVariant: 0
    // Current payload trace entries — feed the lights subpanel.
    property var payloadTrace: []
    // Worst Detect verdict for the current payload ("", "amber", "red")
    // and the source that flagged it — drives the verdict dot, the
    // warning line and the dissection auto-show.
    property string worstVerdict: ""
    property string verdictSource: ""
    readonly property bool redVerdict: worstVerdict === "red"
    readonly property string presentedUrl: payloadVariants.length > 0
        ? String(payloadVariants[Math.max(0, Math.min(
              selectedVariant, payloadVariants.length - 1))])
        : queue.currentRaw

    function refreshFromPayload() {
        const payload = queue.currentPayload
        const list = []
        if (payload && payload.original && payload.original.url)
            list.push(String(payload.original.url))
        if (payload && payload.trace) {
            for (let i = 0; i < payload.trace.length; i++) {
                const data = payload.trace[i].data
                const url = data ? data.url : ""
                if (url && list.indexOf(String(url)) === -1)
                    list.push(String(url))
            }
        }
        if (list.length > root._previousVariantCount)
            selectedVariant = list.length - 1 // last auto-selected
        root._previousVariantCount = list.length
        payloadVariants = list
        payloadTrace = (payload && payload.trace) ? payload.trace : []
        refreshVerdict(payload && payload.detected
                       ? payload.detected : [])
    }

    function refreshVerdict(detected) {
        worstVerdict = ""
        verdictSource = ""
        for (let i = 0; i < detected.length; i++) {
            const v = String(detected[i].verdict || "").toLowerCase()
            if (v === "")
                continue
            if (v === "malicious" || v === "dangerous"
                    || v === "phishing") {
                worstVerdict = "red" // worst — stop here
                verdictSource = String(detected[i].source || "")
                return
            }
            if (worstVerdict === "") {
                worstVerdict = "amber"
                verdictSource = String(detected[i].source || "")
            }
        }
    }

    property int _previousVariantCount: 0

    onPresentedUrlChanged: launcher.setPresentedUrl(presentedUrl)

    Component.onCompleted: {
        refreshFromPayload()
        launcher.setPresentedUrl(presentedUrl)
    }

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
            root._previousVariantCount = 0
            lightsStrip.reset()
            root.refreshFromPayload()
        }

        function onPayloadChanged() {
            root.refreshFromPayload()
        }
    }

    Connections {
        target: pipeline

        function onStageDispatched(id) {
            lightsStrip.noteDispatch(id)
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
            id: contentColumn

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: root.contentPad
            anchors.leftMargin: root.contentPad
                + (root.pillsVisible ? variantPills.width + 12 : 0)
            spacing: 10

            LightsStrip {
                id: lightsStrip

                visible: root.lightsVisible
                roster: pluginRoster
                trace: root.payloadTrace
            }

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
                presentedUrl: root.presentedUrl
                verdict: root.worstVerdict
                widthCapped: footerBar.naturalWidth > root.widthCap

                onCopyRequested: launcher.copyToClipboard(root.presentedUrl)
            }

            Text {
                id: warningLine

                visible: root.redVerdict
                text: qsTr("flagged by %1").arg(root.verdictSource)
                color: "#f87171"
                font.family: "Jura"
                font.pixelSize: 12
                font.weight: Font.Light
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
                Accessible.role: Accessible.Alert
            }

            DissectionPanel {
                id: dissectionPanel

                visible: root.dissectionVisible
                width: parent.width
                presentedUrl: root.presentedUrl
                pinned: false // settings pin wires in with the daemon
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

        VariantPills {
            id: variantPills

            visible: root.pillsVisible
            anchors.left: parent.left
            anchors.leftMargin: root.contentPad
            anchors.verticalCenter: parent.verticalCenter
            variants: root.payloadVariants
            selection: root.selectedVariant

            onSelectRequested: (index) => root.selectedVariant = index
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
                launcher.copyToClipboard(root.presentedUrl)
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
