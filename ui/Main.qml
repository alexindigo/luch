import QtQuick
import QtQuick.Effects

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

    // Drawer/subpanel geometry (physics-mapped, binding per the plan):
    // a drawer is narrower than its cabinet and sticks out from behind
    // it — the cabinet overlaps the drawer's inner edge and shadows it;
    // contents live fully inside the visible (peeking) part.
    readonly property int shadowPad: 28     // window room for the shadow
    readonly property int drawerInset: 28   // drawers strictly narrower
    readonly property int drawerOverlap: 14 // cabinet overlaps inner edge
    readonly property int drawerVPad: 10    // content padding in drawer
    readonly property int drawerHPad: 14

    // Visible drawer portions above/below the main panel's edges — the
    // overlap rides behind the cabinet, never eating content budget.
    readonly property real lightsPeek: lightsVisible
        ? lightsStrip.implicitHeight + 2 * drawerVPad : 0
    readonly property real dissectionPeek: dissectionVisible
        ? dissectionPanel.implicitHeight + 2 * drawerVPad
        : 0
    // Queue satellites (locked: only when more than one target):
    readonly property int satelliteOverhang: 14
    readonly property int badgeOverhang: 14
    readonly property int dotsRowHeight: 8
    readonly property int dotsSpacing: 6
    readonly property bool queued: queue.count > 1

    readonly property int visibleCells: Math.max(
        1, Math.min(browserView.count, maxVisible))
    readonly property int chrome: shadowPad * 2 + surfaceMargin * 2
        + contentPad * 2
    readonly property int stripWidth: visibleCells * cellWidth
        + Math.max(0, visibleCells - 1) * rowSpacing
    readonly property real widthCap: widthCapFactor * Screen.width
    readonly property bool lightsVisible: lightsStrip.chain.length > 0

    width: chrome + (queued ? 2 * satelliteOverhang : 0)
           + Math.ceil(Math.max(stripWidth,
                                Math.min(footerBar.naturalWidth + 2,
                                         widthCap),
                                lightsVisible
                                    ? lightsStrip.implicitWidth
                                      + 2 * (drawerInset + drawerHPad)
                                      : 0))
    height: chrome + (queued ? 2 * badgeOverhang : 0) + cellHeight
            + (footerBar.height > 0 ? footerSpacing + footerBar.height : 0)
            + (launchError !== "" ? errorLabel.height + 10 : 0)
            + (redVerdict ? warningLine.height + 10 : 0)
            + (queued ? 10 + dotsRowHeight : 0)
            + (lightsVisible ? lightsPeek : 0)
            + (dissectionVisible ? dissectionPeek : 0)
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

    // The one URL the main panel knows: the final effective URL (the
    // payload's trace tail; pills are hidden this round, so there is
    // no variant selection). Footer, launch and copy consume it.
    property string presentedUrl: ""
    // Current payload trace entries — feed the lights subpanel.
    property var payloadTrace: []
    // Worst Detect verdict for the current payload ("", "amber", "red")
    // and the source that flagged it — drives the verdict dot, the
    // warning line and the dissection auto-show.
    property string worstVerdict: ""
    property string verdictSource: ""
    readonly property bool redVerdict: worstVerdict === "red"

    function refreshFromPayload() {
        const payload = queue.currentPayload
        presentedUrl = (payload && payload.url)
                       ? String(payload.url) : queue.currentRaw
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

    Item {
        id: stage

        anchors.fill: parent
        anchors.margins: root.shadowPad

        // Lights drawer — narrower than the cabinet, sliding out from
        // behind its top edge; the cabinet overlaps the drawer's inner
        // edge and shadows it. Contents sit fully inside the visible
        // part (labels clear of the cabinet edge).
        Rectangle {
            id: lightsDrawer

            visible: root.lightsVisible
            anchors.left: surface.left
            anchors.right: surface.right
            anchors.leftMargin: root.drawerInset
            anchors.rightMargin: root.drawerInset
            anchors.bottom: surface.top
            anchors.bottomMargin: -root.drawerOverlap
            height: root.lightsPeek + root.drawerOverlap
            radius: 12
            // Tonal family of the main surface, a hair darker for
            // recess — the separation itself is the main panel's
            // shadow, not color contrast.
            color: "#f016233a"
            border.width: 1
            border.color: "#1af2f4f6"

            LightsStrip {
                id: lightsStrip

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: root.drawerHPad
                anchors.rightMargin: root.drawerHPad
                anchors.topMargin: root.drawerVPad
                roster: pluginRoster
                trace: root.payloadTrace
            }
        }

        // Dissection drawer — same physics at the bottom edge: narrower,
        // from behind, cabinet overlapping its inner edge; the anatomy
        // table fills the drawer (no void).
        Rectangle {
            id: dissectionDrawer

            visible: root.dissectionVisible
            anchors.left: surface.left
            anchors.right: surface.right
            anchors.leftMargin: root.drawerInset
            anchors.rightMargin: root.drawerInset
            anchors.top: surface.bottom
            anchors.topMargin: -root.drawerOverlap
            height: root.dissectionPeek + root.drawerOverlap
            radius: 12
            color: "#f016233a"
            border.width: 1
            border.color: "#1af2f4f6"

            DissectionPanel {
                id: dissectionPanel

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: root.drawerHPad
                anchors.rightMargin: root.drawerHPad
                anchors.bottomMargin: root.drawerVPad
                presentedUrl: root.presentedUrl
                pinned: settings.showDissection
            }
        }

        // The elevated main panel casts a shadow over both drawers —
        // the depth cue, per the mock. Symmetric blur so both the top
        // and the bottom drawer catch it.
        MultiEffect {
            anchors.fill: surface
            source: surface
            shadowEnabled: true
            shadowBlur: 1.0
            shadowVerticalOffset: 0
            shadowColor: "#b0000000"
        }

        Rectangle {
            id: surface
            z: 1

            anchors.fill: parent
            anchors.leftMargin: root.surfaceMargin
                + (root.queued ? root.satelliteOverhang : 0)
            anchors.rightMargin: root.surfaceMargin
                + (root.queued ? root.satelliteOverhang : 0)
            anchors.topMargin: root.surfaceMargin
                + (root.queued ? root.badgeOverhang : 0)
                + (root.lightsVisible ? root.lightsPeek : 0)
            anchors.bottomMargin: root.surfaceMargin
                + (root.queued ? root.badgeOverhang : 0)
                + (root.dissectionVisible ? root.dissectionPeek : 0)
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
                    presentedUrl: root.presentedUrl
                    verdict: root.worstVerdict
                    widthCapped: footerBar.naturalWidth > root.widthCap

                    onCopyRequested:
                        launcher.copyToClipboard(root.presentedUrl)
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
