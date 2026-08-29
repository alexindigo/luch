import QtQuick

Item {
    id: chrome

    property Item surface
    property color accent
    property color textStrong
    property color textFaint

    // cur/total badge — oval, half outside the surface's top-right edge
    Rectangle {
        id: badge

        width: badgeText.implicitWidth + 18
        height: badgeText.implicitHeight + 10
        radius: height / 2
        x: chrome.surface.x + chrome.surface.width - width + 6
        y: chrome.surface.y - height / 2
        color: "#d91e293b"
        border.width: 1
        border.color: "#26f2f4f6"

        Text {
            id: badgeText

            anchors.centerIn: parent
            text: (queue.cursor + 1) + "/" + queue.count
            color: chrome.textStrong
            font.family: "Jura"
            font.pixelSize: 13
            font.weight: Font.Light
        }
    }

    // chevrons — circled, half outside the surface's side edges, fading
    // at the queue ends (no wrap)
    Rectangle {
        id: chevLeft

        width: 28
        height: 28
        radius: 14
        x: chrome.surface.x - width / 2
        y: chrome.surface.y + 18 + 52 - 14
        color: chevLeftMouse.containsMouse ? "#26f2f4f6" : "#d91e293b"
        border.width: 1
        border.color: "#26f2f4f6"
        opacity: queue.cursor > 0 ? 1 : 0.35

        Text {
            anchors.centerIn: parent
            text: "\u2039"
            color: chrome.textStrong
            font.family: "Jura"
            font.pixelSize: 15
            font.weight: Font.Light
        }

        MouseArea {
            id: chevLeftMouse

            anchors.fill: parent
            hoverEnabled: true
            enabled: queue.cursor > 0
            onClicked: queue.moveCursor(-1)
        }
    }

    Rectangle {
        id: chevRight

        width: 28
        height: 28
        radius: 14
        x: chrome.surface.x + chrome.surface.width - width / 2
        y: chrome.surface.y + 18 + 52 - 14
        color: chevRightMouse.containsMouse ? "#26f2f4f6" : "#d91e293b"
        border.width: 1
        border.color: "#26f2f4f6"
        opacity: queue.cursor < queue.count - 1 ? 1 : 0.35

        Text {
            anchors.centerIn: parent
            text: "\u203a"
            color: chrome.textStrong
            font.family: "Jura"
            font.pixelSize: 15
            font.weight: Font.Light
        }

        MouseArea {
            id: chevRightMouse

            anchors.fill: parent
            hoverEnabled: true
            enabled: queue.cursor < queue.count - 1
            onClicked: queue.moveCursor(1)
        }
    }
}
