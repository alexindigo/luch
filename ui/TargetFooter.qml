import QtQuick
import XdgIcon
import "decompose.js" as Decompose

Item {
    id: footer

    property color accent
    property color textStrong
    property color textFaint

    // The one URL the footer knows: a flat string, decomposed here
    // (QML-owned render function; C++ stops pre-decomposing).
    property string presentedUrl: ""
    // Detect verdict for the presented URL: "" | "amber" | "red"
    property string verdict: ""

    readonly property var parts: Decompose.decompose(presentedUrl)

    readonly property int dotSize: 6

    readonly property int copySize: 26
    readonly property int copyGap: 10
    readonly property int fontPx: 12
    // Below this, a middle fragment is illegible — fall back to the bare
    // " … " marker instead of a sliver of path.
    readonly property int minMiddleWidth: 48

    readonly property real naturalWidth: schemeText.implicitWidth
        + hostText.implicitWidth + midText.implicitWidth
        + tailText.implicitWidth + copyControl.width + copyGap

    // Stub line with the whole middle behind the marker — the degenerate
    // fallback, and the yardstick for when wrapping is unavoidable.
    readonly property real stubWidth: naturalWidth
        - midText.implicitWidth + markerText.implicitWidth

    readonly property real textAvailWidth:
        width - copyControl.width - copyGap

    // Room left for the middle once scheme, host and tail (never
    // truncated) have taken theirs.
    readonly property real midAvail: textAvailWidth
        - schemeText.implicitWidth - hostText.implicitWidth
        - tailText.implicitWidth

    property bool widthCapped: false

    readonly property bool overflow: naturalWidth > width
    readonly property bool wrapping: widthCapped && stubWidth > width
    // The container pushed back: elide the middle — only as much as
    // needed to fit, no more. The middle renders as two fragments split
    // around a visibly separated marker.
    readonly property bool collapsed: overflow && !wrapping

    // Room for the two fragments once the marker takes its seat.
    readonly property real fragBudget: Math.max(
        0, midAvail - markerText.implicitWidth)
    readonly property real headFragWidth: Math.floor(fragBudget / 2)
    readonly property real tailFragWidth: fragBudget - headFragWidth

    // host+tail+marker alone fill the line: no useful sliver of middle
    // to show.
    readonly property bool middleDoomed: collapsed
        && fragBudget < minMiddleWidth

    implicitHeight: Math.max(segFlow.height, copyControl.height)

    Accessible.role: Accessible.StaticText
    // The full untruncated presented URL (segments render with elision;
    // AT users get the whole thing).
    Accessible.name: footer.presentedUrl

    signal copyRequested()

    Flow {
        id: segFlow

        Accessible.ignored: true // announced via the parent's name

        anchors.verticalCenter: parent.verticalCenter
        // Overflow modes fill the container exactly; the full line gets
        // 1px of slack so rounding never wraps the tail onto a new row.
        width: footer.overflow
               ? footer.textAvailWidth
               : footer.naturalWidth - footer.copySize - footer.copyGap + 1
        spacing: 0

        Text {
            id: schemeText

            text: footer.parts.scheme
            visible: footer.parts.scheme !== ""
            color: footer.textFaint
            font.family: "Jura"
            font.pixelSize: footer.fontPx
            font.weight: Font.Light
        }

        Text {
            id: hostText

            text: footer.parts.hostOrDir
            visible: footer.parts.hostOrDir !== ""
            color: footer.textStrong
            font.family: "Jura"
            font.pixelSize: footer.fontPx
            font.weight: Font.Light
            width: footer.wrapping
                   ? Math.min(implicitWidth, segFlow.width)
                   : implicitWidth
            wrapMode: footer.wrapping ? Text.WrapAnywhere : Text.NoWrap
        }

        // Full middle — shown only when it fits as-is (or wraps).
        Text {
            id: midText

            text: footer.parts.middle
            visible: footer.parts.middle !== "" && !footer.collapsed
            color: footer.textFaint
            font.family: "Jura"
            font.pixelSize: footer.fontPx
            font.weight: Font.Light
            width: footer.wrapping
                   ? Math.min(implicitWidth, segFlow.width)
                   : implicitWidth
            wrapMode: footer.wrapping ? Text.WrapAnywhere : Text.NoWrap
        }

        // Collapsed: the middle renders as two plain fragments around a
        // spaced, dimmed marker. TextMetrics pre-elides each fragment to
        // its half of the leftover room; the elide glyph is stripped so
        // the marker stays the only "…" on the line.
        TextMetrics {
            id: headMetrics

            font: midHeadText.font
            text: footer.parts.middle
            elide: Text.ElideRight
            elideWidth: footer.headFragWidth
        }

        Text {
            id: midHeadText

            text: {
                const t = headMetrics.elidedText
                return t !== footer.parts.middle && t.endsWith("…")
                       ? t.slice(0, -1) : t
            }
            visible: footer.collapsed && !footer.middleDoomed
                && footer.parts.middle !== ""
            color: footer.textFaint
            font.family: "Jura"
            font.pixelSize: footer.fontPx
            font.weight: Font.Light
        }

        Text {
            id: markerText

            text: " … "
            visible: footer.collapsed && footer.parts.middle !== ""
            color: footer.accent
            font.family: "Jura"
            font.pixelSize: footer.fontPx
            font.weight: Font.Light
        }

        TextMetrics {
            id: tailMetrics

            font: midTailText.font
            text: footer.parts.middle
            elide: Text.ElideLeft
            elideWidth: footer.tailFragWidth
        }

        Text {
            id: midTailText

            text: {
                const t = tailMetrics.elidedText
                return t !== footer.parts.middle && t.startsWith("…")
                       ? t.slice(1) : t
            }
            visible: footer.collapsed && !footer.middleDoomed
                && footer.parts.middle !== ""
            color: footer.textFaint
            font.family: "Jura"
            font.pixelSize: footer.fontPx
            font.weight: Font.Light
        }

        Text {
            id: tailText

            text: footer.parts.tail
            visible: footer.parts.tail !== ""
            color: footer.textFaint
            font.family: "Jura"
            font.pixelSize: footer.fontPx
            font.weight: Font.Light
            width: footer.wrapping
                   ? Math.min(implicitWidth, segFlow.width)
                   : implicitWidth
            wrapMode: footer.wrapping ? Text.WrapAnywhere : Text.NoWrap
        }
    }

    Item {
        id: copyControl

        width: footer.copySize
        height: footer.copySize
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Copy target")
        Accessible.onPressAction: footer.copyRequested()

        // verdict dot — the Detect verdict that applies to the
        // presented URL, unmissable next to the copy control
        Rectangle {
            id: verdictDot

            visible: footer.verdict !== ""
            width: footer.dotSize
            height: footer.dotSize
            radius: footer.dotSize / 2
            anchors.left: parent.left
            anchors.leftMargin: -footer.dotSize - 4
            anchors.verticalCenter: parent.verticalCenter
            color: footer.verdict === "red" ? "#f87171" : "#fbbf24"

            SequentialAnimation on opacity {
                running: footer.verdict === "red"
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.35; duration: 500 }
                NumberAnimation { from: 0.35; to: 1.0; duration: 500 }
            }

            Accessible.role: Accessible.Indicator
            Accessible.name: footer.verdict === "red"
                             ? qsTr("dangerous")
                             : qsTr("flagged")
        }

        XdgIcon {
            id: copyIcon

            name: "edit-copy"
            size: 16
        }

        Rectangle {
            anchors.fill: parent
            radius: 8
            color: copyMouse.pressed ? Qt.alpha(footer.accent, 0.3)
                 : copyMouse.containsMouse ? "#26f2f4f6"
                 : "transparent"
        }

        Image {
            anchors.centerIn: parent
            width: 16
            height: 16
            source: copyIcon.path
            visible: copyIcon.found
            smooth: true
            mipmap: true
        }

        Item {
            anchors.centerIn: parent
            width: 15
            height: 17
            visible: !copyIcon.found

            property color glyphColor: copyMouse.pressed
                ? footer.accent
                : copyMouse.containsMouse ? footer.textStrong
                : footer.textFaint

            Rectangle {
                x: 0
                y: 4
                width: 10
                height: 12
                radius: 2
                color: "transparent"
                border.width: 1.5
                border.color: Qt.alpha(parent.glyphColor, 0.45)
            }

            Rectangle {
                x: 4
                y: 0
                width: 10
                height: 12
                radius: 2
                color: "transparent"
                border.width: 1.5
                border.color: parent.glyphColor
            }
        }

        MouseArea {
            id: copyMouse

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor

            onClicked: footer.copyRequested()
        }
    }
}
