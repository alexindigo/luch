pragma ComponentBehavior: Bound

import QtQuick
import "decompose.js" as Decompose

// Bottom panel — URL anatomy for the presented URL: scheme / host /
// domain (eTLD+1, via the engine) / port / path / query / fragment.
// host renders as the subdomain PREFIX of the eTLD+1 ("debounce." for
// debouce.example.com — never the domain itself); absent query /
// fragment render as dim "none"; the query keeps its "?" prefix; a
// scheme-default port renders as the number, dimmed. Hidden by
// default; auto-shows on a red verdict; pinnable "always show" via
// settings.
//
// Inputs only: the presented URL, the red-verdict flag, and the pin.
Item {
    id: dissection

    property string presentedUrl: ""
    property bool pinned: false

    readonly property var parts: Decompose.decompose(presentedUrl)
    readonly property string domain:
        parts.host !== "" ? urlTools.eTLDPlusOne(parts.host) : ""
    // Subdomain prefix of the eTLD+1 — "debounce." for
    // debounce.example.com; "" when the host IS the bare domain.
    readonly property string hostPrefix:
        parts.host !== "" && domain !== ""
        && parts.host.endsWith("." + domain)
        ? parts.host.substring(0, parts.host.length - domain.length)
        : ""
    readonly property bool hostIsBare:
        parts.host !== "" && domain !== "" && parts.host === domain
    readonly property bool portIsDefault:
        parts.port === -1
        || (parts.scheme === "https://" && parts.port === 443)
        || (parts.scheme === "http://" && parts.port === 80)
    readonly property string portValue: {
        if (parts.port !== -1)
            return String(parts.port)
        if (parts.scheme === "https://")
            return "443"
        if (parts.scheme === "http://")
            return "80"
        return qsTr("none")
    }
    readonly property bool queryAbsent:
        parts.query === null || parts.query === ""
    readonly property bool fragmentAbsent:
        parts.fragment === null || parts.fragment === ""

    readonly property var rows: [
        { label: qsTr("scheme"), value: dissection.parts.scheme,
          dim: true },
        { label: qsTr("host"),
          value: dissection.hostIsBare
                 ? qsTr("none") : dissection.hostPrefix,
          dim: dissection.hostIsBare },
        { label: qsTr("domain"), value: dissection.domain,
          dim: dissection.domain === "" },
        { label: qsTr("port"), value: dissection.portValue,
          dim: dissection.portIsDefault },
        { label: qsTr("path"), value: dissection.parts.path,
          dim: false },
        { label: qsTr("query"),
          value: dissection.queryAbsent ? qsTr("none")
                                        : "?" + dissection.parts.query,
          dim: dissection.queryAbsent },
        { label: qsTr("fragment"),
          value: dissection.fragmentAbsent ? qsTr("none")
                                           : dissection.parts.fragment,
          dim: dissection.fragmentAbsent }
    ]

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
                      .arg(dissection.parts.scheme)
                      .arg(dissection.rows[1].value)
                      .arg(dissection.domain !== "" ? dissection.domain
                                                    : qsTr("unknown"))
                      .arg(dissection.portValue)
                      .arg(dissection.parts.path)
                      .arg(dissection.rows[5].value)
                      .arg(dissection.rows[6].value)

    Column {
        id: rows

        anchors.left: parent.left
        anchors.leftMargin: dissection.rowPad
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Repeater {
            model: dissection.rows

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
