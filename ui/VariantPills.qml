pragma ComponentBehavior: Bound

import QtQuick

// Left-edge file tabs: one tab per URL variant the chain produced,
// stacked in chain order (original at top, newest at the bottom — the
// newest merges at the footer line it drives). The ACTIVE tab's right
// edge flows into the panel's left edge: one continuous frosted
// surface, and the merge IS the selection signal (no accent highlight).
// Inactive tabs recess behind — offset left, a hair darker, with a
// visible gap from the panel edge.
//
// Inputs only: variants [{url, label}], selection, the panel's left
// edge and the footer line's bottom (in parent coordinates); output:
// selectRequested(index). The component never reaches into parents.
Item {
    id: pills

    property var variants: []
    property int selection: 0
    // Where the tabs attach and which line the active tab meets.
    property real panelLeft: 0
    property real footerLineBottom: 0

    signal selectRequested(int index)

    readonly property int fontPx: 11
    readonly property int pillHPadding: 10
    readonly property int pillHeight: 20
    readonly property int pillSpacing: 6
    readonly property int pillRadius: 8
    readonly property int mergeDepth: 10  // active tab flows into the panel
    readonly property int recessGap: 6    // inactive: visible gap to the edge
    readonly property int maxLabelWidth: 140

    // How far left of the panel edge the tabs reach — the parent
    // reserves this much room in the window layout.
    property real leftOverhang: 0

    // Blur-region rects for every tab, in parent coordinates (the tabs
    // are translucent — without them they'd show sharp wallpaper).
    property var blurRectList: []

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("URL variants")

    function scheduleRecompute() {
        if (!recomputePending) {
            recomputePending = true
            Qt.callLater(recomputeGeometry)
        }
    }

    property bool recomputePending: false

    function recomputeGeometry() {
        recomputePending = false
        let overhang = 0
        const rects = []
        for (let i = 0; i < children.length; i++) {
            const slot = children[i]
            if (!slot || slot.bodyWidth === undefined)
                continue
            overhang = Math.max(overhang,
                                slot.bodyWidth + recessGap)
            const clip = slot.pillClipItem
            if (clip) {
                const r = clip.blurRects
                for (let j = 0; j < r.length; j++)
                    rects.push(r[j])
            }
        }
        leftOverhang = overhang
        blurRectList = rects
    }

    Repeater {
        model: pills.variants

        Item {
            id: slot

            required property int index
            required property var modelData
            readonly property bool active: pills.selection === index
            readonly property real bodyWidth:
                labelText.implicitWidth + 2 * pills.pillHPadding

            // Chain order: original at top, newest at the bottom, the
            // newest tab's bottom edge meeting the footer line.
            y: pills.footerLineBottom
               - (pills.variants.length - index) * pills.pillHeight
               - (pills.variants.length - 1 - index) * pills.pillSpacing
            height: pills.pillHeight
            // Non-zero width so the slot is a real hit-test target —
            // a 0-width parent can break input delivery to children on
            // some platforms even though they still render.
            width: slot.bodyWidth + pills.mergeDepth
            x: pills.panelLeft - slot.bodyWidth
               + (slot.active ? pills.mergeDepth : -pills.recessGap)
            opacity: 0

            property alias pillClipItem: pillClip

            Behavior on x {
                NumberAnimation {
                    duration: 160
                    easing.type: Easing.OutCubic
                }
            }

            Component.onCompleted: {
                opacity = 1
                pills.scheduleRecompute()
            }
            onActiveChanged: pills.scheduleRecompute()

            Behavior on opacity {
                NumberAnimation { duration: 160 }
            }

            // The tab's visible rect; the active tab's merge side runs
            // past the clip so its right border and corners are cut —
            // the surface flows into the panel with no seam. Positioned
            // at the slot's origin (the slot carries the panel-left
            // offset), width = body + merge depth for the active tab.
            Item {
                id: pillClip

                readonly property var blurRects:
                    slot.active
                    ? pills.roundedLeftRects(x + slot.x, y + slot.y,
                                             width, height,
                                             pills.pillRadius)
                    : pills.roundedAllRects(x + slot.x, y + slot.y,
                                            width, height,
                                            pills.pillRadius)

                x: 0
                y: 0
                width: slot.bodyWidth
                       + (slot.active ? pills.mergeDepth : 0)
                height: slot.height
                clip: true

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    width: slot.bodyWidth
                           + (slot.active
                              ? pills.mergeDepth + pills.pillRadius : 0)
                    height: slot.height
                    radius: pills.pillRadius
                    // Active: exactly the panel's frosted surface (the
                    // merge reads as one page). Inactive: a hair
                    // darker — recessed behind.
                    color: slot.active ? "#cc1e293b" : "#cc151c2a"
                    border.width: 1
                    border.color: slot.active ? "#1af2f4f6"
                                              : "#10f2f4f6"

                    Text {
                        id: labelText

                        anchors.left: parent.left
                        anchors.leftMargin: pills.pillHPadding
                        anchors.verticalCenter: parent.verticalCenter
                        text: slot.modelData.label
                        color: slot.active ? "#cbd5e1" : "#94a3b8"
                        font.family: "Jura"
                        font.pixelSize: pills.fontPx
                        font.weight: Font.Light
                        elide: Text.ElideRight
                        width: Math.min(implicitWidth,
                                        pills.maxLabelWidth)
                    }
                }

                MouseArea {
                    id: pillMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: pills.selectRequested(slot.index)
                }

                Accessible.role: Accessible.RadioButton
                Accessible.name: slot.modelData.url || ""
                Accessible.checked: slot.active
                Accessible.onPressAction: pills.selectRequested(slot.index)
            }

            onBodyWidthChanged: pills.scheduleRecompute()
        }
    }

    // Blur-region helpers (wl_region is axis-aligned rects; rounded
    // silhouettes stair-step, biased inside the shape). Duplicated
    // small from Main.qml — the component owns its own geometry.
    function roundedAllRects(x, y, w, h, r) {
        const rects = [
            { x: x, y: y + r, width: w, height: h - 2 * r },
            { x: x + r, y: y, width: w - 2 * r, height: r },
            { x: x + r, y: y + h - r, width: w - 2 * r, height: r }]
        const N = 4
        const s = r / N
        for (let i = 0; i < N; i++) {
            const t0 = i * s
            const inset = Math.ceil(
                r - Math.sqrt(r * r - Math.pow(r - t0, 2)))
            rects.push({ x: x + inset, y: y + t0,
                         width: r - inset, height: s })
            rects.push({ x: x + w - r, y: y + t0,
                         width: r - inset, height: s })
            rects.push({ x: x + inset, y: y + h - t0 - s,
                         width: r - inset, height: s })
            rects.push({ x: x + w - r, y: y + h - t0 - s,
                         width: r - inset, height: s })
        }
        return rects
    }

    // Left corners rounded, right side square (the active tab's merge
    // side — clipped flush into the panel).
    function roundedLeftRects(x, y, w, h, r) {
        const rects = [
            { x: x + r, y: y, width: w - r, height: h },
            { x: x, y: y + r, width: r, height: h - 2 * r }]
        const N = 4
        const s = r / N
        for (let i = 0; i < N; i++) {
            const d0 = i * s
            const inset = Math.ceil(
                r - Math.sqrt(r * r - Math.pow(r - d0, 2)))
            rects.push({ x: x + d0, y: y + inset,
                         width: s, height: r - inset })
            rects.push({ x: x + d0, y: y + h - r,
                         width: s, height: r - inset })
        }
        return rects
    }
}
