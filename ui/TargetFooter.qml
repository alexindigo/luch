import QtQuick
import XdgIcon

Item {
    id: footer

    property color accent
    property color textStrong
    property color textSoft
    property color textFaint
    property color textDimmest

    property string scheme: ""
    property string hostOrDir: ""
    property string middle: ""
    property string tail: ""

    readonly property int copySize: 26
    readonly property int copyGap: 10
    readonly property int fontPx: 12

    readonly property real naturalWidth: schemeText.implicitWidth
        + hostText.implicitWidth + midText.implicitWidth
        + tailText.implicitWidth + copyControl.width + copyGap

    readonly property real markerLineWidth: naturalWidth
        - midText.implicitWidth + markerText.implicitWidth

    readonly property real displayTextWidth: collapsed
        ? markerLineWidth - copyControl.width - copyGap
        : naturalWidth - copyControl.width - copyGap

    property bool widthCapped: false

    readonly property bool collapsed:
        !wrapping && naturalWidth > width
    readonly property bool wrapping:
        widthCapped && markerLineWidth > width

    readonly property real textAvailWidth:
        width - copyControl.width - copyGap

    implicitHeight: Math.max(segFlow.height, copyControl.height)

    signal copyRequested()

    Flow {
        id: segFlow

        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(displayTextWidth + 1, footer.textAvailWidth)
        spacing: 0

        Text {
            id: schemeText

            text: footer.scheme
            visible: footer.scheme !== ""
            color: footer.textFaint
            font.family: "Jura"
            font.pixelSize: footer.fontPx
            font.weight: Font.Light
        }

        Text {
            id: hostText

            text: footer.hostOrDir
            visible: footer.hostOrDir !== ""
            color: footer.textStrong
            font.family: "Jura"
            font.pixelSize: footer.fontPx
            font.weight: Font.Light
            width: footer.wrapping
                   ? Math.min(implicitWidth, segFlow.width)
                   : implicitWidth
            wrapMode: footer.wrapping ? Text.WrapAnywhere : Text.NoWrap
        }

        Text {
            id: midText

            text: footer.middle
            visible: !footer.collapsed && footer.middle !== ""
            color: footer.textFaint
            font.family: "Jura"
            font.pixelSize: footer.fontPx
            font.weight: Font.Light
            width: footer.wrapping
                   ? Math.min(implicitWidth, segFlow.width)
                   : implicitWidth
            wrapMode: footer.wrapping ? Text.WrapAnywhere : Text.NoWrap
        }

        Text {
            id: markerText

            text: " \u2026 "
            visible: footer.collapsed
            color: footer.textDimmest
            font.family: "Jura"
            font.pixelSize: footer.fontPx
            font.weight: Font.Light
        }

        Text {
            id: tailText

            text: footer.tail
            visible: footer.tail !== ""
            color: footer.textSoft
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
