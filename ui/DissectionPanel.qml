pragma ComponentBehavior: Bound

import QtQuick
import "decompose.js" as Decompose

// Bottom panel — URL anatomy for the presented URL: scheme / host /
// domain (eTLD+1, via the engine) / port / path / query / fragment.
// Hidden by default; auto-shows on a red verdict; pinnable "always
// show" via settings.
//
// Inputs only: the presented URL, the red-verdict flag, and the pin.
Item {
    id: dissection

    property string presentedUrl: ""
    property bool visibleByRed: false
    property bool pinned: false

    readonly property var parts: Decompose.decompose(presentedUrl)
    readonly property string domain:
        parts.host !== "" ? urlTools.eTLDPlusOne(parts.host) : ""
    readonly property bool portIsDefault:
        parts.port === -1
        || (parts.scheme === "https://" && parts.port === 443)
        || (parts.scheme === "http://" && parts.port === 80)
    readonly property int fontPx: 11
    readonly property int rowHeight: 18
    readonly property int rowGap: 12     // label→value column gap
    readonly property int labelCol: 56   // right-aligned label column
    readonly property int rowPad: 6
    // Content width when the parent gives us the drawer's inner width —
    // the table fills the drawer horizontally (no void).
    readonly property real contentWidth: width - 2 * rowPad

    // The drawer is geometry-driven and never sizes from this; the
    // constant just names a sensible natural width for standalone use.
    implicitWidth: 2 * rowPad + labelCol + rowGap + 340
    implicitHeight: rows.implicitHeight + 8

    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("URL anatomy: scheme %1, host %2, domain %3, "
                         + "port %4, path %5, query %6, fragment %7")
                      .arg(parts.scheme).arg(parts.host)
                      .arg(domain !== "" ? domain : qsTr("unknown"))
                      .arg(parts.port === -1 ? qsTr("default")
                                             : String(parts.port))
                      .arg(parts.path)
                      .arg(parts.query !== null ? parts.query
                                                : qsTr("none"))
                      .arg(parts.fragment !== null ? parts.fragment
                                                   : qsTr("none"))

    Column {
        id: rows

        anchors.left: parent.left
        anchors.leftMargin: dissection.rowPad
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Repeater {
            model: [
                { label: qsTr("scheme"), value: dissection.parts.scheme,
                  dim: true },
                { label: qsTr("host"), value: dissection.parts.host,
                  dim: false },
                { label: qsTr("domain"), value: dissection.domain,
                  dim: dissection.domain === "" },
                { label: qsTr("port"),
                  value: dissection.parts.port === -1
                         ? qsTr("default")
                         : String(dissection.parts.port),
                  dim: dissection.portIsDefault },
                { label: qsTr("path"), value: dissection.parts.path,
                  dim: false },
                { label: qsTr("query"),
                  value: dissection.parts.query !== null
                         ? dissection.parts.query : qsTr("—"),
                  dim: dissection.parts.query === null },
                { label: qsTr("fragment"),
                  value: dissection.parts.fragment !== null
                         ? dissection.parts.fragment : qsTr("—"),
                  dim: dissection.parts.fragment === null }
            ]

            Row {
                id: row

                required property var modelData

                spacing: dissection.rowGap

                Text {
                    width: dissection.labelCol
                    height: dissection.rowHeight
                    text: row.modelData.label
                    color: "#64748b"
                    font.family: "Jura"
                    font.pixelSize: dissection.fontPx
                    font.weight: Font.Light
                    horizontalAlignment: Text.AlignRight
                }

                Text {
                    width: dissection.contentWidth - dissection.labelCol
                           - dissection.rowGap
                    height: dissection.rowHeight
                    text: row.modelData.value !== ""
                          ? row.modelData.value : "—"
                    color: row.modelData.dim ? "#64748b" : "#cbd5e1"
                    font.family: "Jura"
                    font.pixelSize: dissection.fontPx
                    font.weight: Font.Light
                    elide: Text.ElideMiddle
                }
            }
        }
    }
}
