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

    // Drawer/subpanel geometry (physics-mapped, binding per the plan):
    // a drawer is narrower than its cabinet and sticks out from behind
    // it; the tucked part is CUT at the cabinet's edge (translucent
    // surfaces must not stack — nothing renders behind the panel), so
    // the seam end is square-cut and the free end keeps its rounding.
    readonly property int windowMargin: 6   // clear of screen edges
    readonly property int drawerInset: 28   // drawers strictly narrower
    readonly property int drawerVPad: 12    // the drawer's frame —
    readonly property int drawerHPad: 12    // uniform on every edge
    readonly property int drawerRadius: 12

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
    readonly property int chrome: windowMargin * 2 + surfaceMargin * 2
        + contentPad * 2
    readonly property int stripWidth: visibleCells * cellWidth
        + Math.max(0, visibleCells - 1) * rowSpacing
    readonly property real widthCap: widthCapFactor * Screen.width
    readonly property bool lightsVisible: lightsStrip.chain.length > 0

    readonly property bool dissectionVisible:
        dissectionPanel.pinned || redVerdict

    // The drawers are asymmetric (a thin lights peek above, a tall
    // dissection peek below) — without compensation the cabinet's
    // center rides above the screen's center. Growing the window by the
    // full imbalance moves its center down half as much, so the bias
    // is the whole difference: the MAIN PANEL (the furniture's face)
    // ends up centered on screen.
    readonly property int panelBias:
        Math.max(0, dissectionPeek - lightsPeek)

    width: chrome + (queued ? 2 * satelliteOverhang : 0)
           + Math.ceil(Math.max(stripWidth,
                                Math.min(footerBar.naturalWidth
                                         + footerBar.dotShift + 2,
                                         widthCap),
                                lightsVisible
                                    ? lightsStrip.implicitWidth
                                      + 2 * (drawerInset + drawerHPad)
                                      : 0))
    height: chrome + panelBias
            + (queued ? 2 * badgeOverhang : 0) + cellHeight
            + (footerBar.height > 0 ? footerSpacing + footerBar.height : 0)
            + (launchError !== "" ? errorLabel.height + 10 : 0)
            + (redVerdict ? warningLine.height + 10 : 0)
            + (queued ? 10 + dotsRowHeight : 0)
            + (lightsVisible ? lightsPeek : 0)
            + (dissectionVisible ? dissectionPeek : 0)
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

    // Translucent areas of the picker, in surface-local coordinates —
    // the main panel plus any open drawers. The compositor blurs the
    // background behind exactly these rects (frosted glass via
    // ext-background-effect-v1; silent plain-alpha fallback elsewhere).
    readonly property var blurRects: {
        const rects = [{ x: stage.x + surface.x, y: stage.y + surface.y,
                         width: surface.width, height: surface.height }]
        if (lightsVisible)
            rects.push({ x: stage.x + lightsDrawer.x,
                         y: stage.y + lightsDrawer.y,
                         width: lightsDrawer.width,
                         height: lightsDrawer.height })
        if (dissectionVisible)
            rects.push({ x: stage.x + dissectionDrawer.x,
                         y: stage.y + dissectionDrawer.y,
                         width: dissectionDrawer.width,
                         height: dissectionDrawer.height })
        return rects
    }
    onBlurRectsChanged: backgroundEffect.setBlurRects(blurRects)

    Component.onCompleted: {
        refreshFromPayload()
        launcher.setPresentedUrl(presentedUrl)
        backgroundEffect.setBlurRects(blurRects)
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
        anchors.leftMargin: root.windowMargin
        anchors.rightMargin: root.windowMargin
        anchors.topMargin: root.windowMargin + root.panelBias
        anchors.bottomMargin: root.windowMargin

        // Lights drawer — narrower than the cabinet, sticking out of
        // its top edge. The container ends exactly at the cabinet's
        // edge and clips: the tucked part is cut away (no translucent
        // stacking under the panel), so the seam end is square and the
        // free end keeps its rounding. Same translucency as the panel.
        Item {
            id: lightsDrawer

            visible: root.lightsVisible
            anchors.left: surface.left
            anchors.right: surface.right
            anchors.leftMargin: root.drawerInset
            anchors.rightMargin: root.drawerInset
            anchors.bottom: surface.top
            height: root.lightsPeek
            clip: true

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: parent.height + root.drawerRadius
                radius: root.drawerRadius
                color: "#cc1e293b"
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
        }

        // Dissection drawer — same physics at the bottom edge: square
        // cut at the seam (clipped), rounded at the free end.
        Item {
            id: dissectionDrawer

            visible: root.dissectionVisible
            anchors.left: surface.left
            anchors.right: surface.right
            anchors.leftMargin: root.drawerInset
            anchors.rightMargin: root.drawerInset
            anchors.top: surface.bottom
            height: root.dissectionPeek
            clip: true

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.height + root.drawerRadius
                radius: root.drawerRadius
                color: "#cc1e293b"
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
            color: "#cc1e293b"
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
                    widthCapped: footerBar.naturalWidth
                                 + footerBar.dotShift > root.widthCap

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
