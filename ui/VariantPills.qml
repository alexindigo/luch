pragma ComponentBehavior: Bound

import QtQuick
import "decompose.js" as Decompose

// Left-edge radio pills: one pill per URL variant the chain produced.
// Radio semantics — exactly one selected; selection drives the main
// panel's presented URL. Pills materialize as variants land; the
// parent auto-selects the last one.
//
// Inputs only: variants (ordered URL strings) + selection index;
// output: selectRequested(index). The component never reaches into
// its parents.
Column {
    id: pills

    property var variants: []
    property int selection: 0

    signal selectRequested(int index)

    readonly property int pillHeight: 20
    readonly property int fontPx: 11

    spacing: 6

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("URL variants")

    Repeater {
        model: pills.variants.length

        Rectangle {
            id: pill

            required property int index

            readonly property bool selected: pills.selection === index
            readonly property var parts:
                Decompose.decompose(pills.variants[index])

            width: pillRow.implicitWidth + 16
            height: pills.pillHeight
            radius: height / 2
            color: selected ? "#3300e5ff" : "#22f2f4f6"
            border.width: selected ? 1 : 0
            border.color: "#8000e5ff"

            Row {
                id: pillRow

                anchors.centerIn: parent
                spacing: 5

                Rectangle {
                    width: 6
                    height: 6
                    radius: 3
                    anchors.verticalCenter: parent.verticalCenter
                    color: pill.selected ? "#00e5ff"
                                         : "#6694a3b8"
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: pill.parts.hostOrDir
                          + (pill.parts.tail !== ""
                             ? pill.parts.tail : "")
                    color: pill.selected ? "#00e5ff" : "#94a3b8"
                    font.family: "Jura"
                    font.pixelSize: pills.fontPx
                    font.weight: Font.Light
                    elide: Text.ElideRight
                    width: Math.min(implicitWidth, 150)
                }
            }

            MouseArea {
                id: pillMouse

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: pills.selectRequested(pill.index)
            }

            Accessible.role: Accessible.RadioButton
            Accessible.name: pills.variants[index] || ""
            Accessible.checked: pill.selected
            Accessible.onPressAction:
                pills.selectRequested(pill.index)
        }
    }
}
