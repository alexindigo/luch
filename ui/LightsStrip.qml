pragma ComponentBehavior: Bound

import QtQuick

// Top subpanel: one cluster (label + light) per chain plugin, laid out
// in chain order. Light vocabulary: dim = queued, shimmer = dispatched
// (working, no slice yet), green = noop, amber = found/changed,
// red = dangerous. Trace-driven bulbs — every trace entry landing is a
// pulse; the resting color is the final entry's outcome. Inet plugins
// carry an "online" marker.
//
// Inputs only (component never reaches into parents): the parent wires
// the roster, the current trace, and dispatch events.
Item {
    id: lightsStrip

    // [{id, title, phase, inet, enabled, loaded}] in chain order
    property var roster: []
    // current payload trace entries [{plugin, iteration, data}]
    property var trace: []
    // ids dispatched with no slice landed yet (async stages)
    property var dispatched: ({})

    readonly property var chain: roster.filter(
        function (plugin) { return plugin.loaded === true })

    readonly property real lightSize: 8
    readonly property real labelGap: 5
    readonly property real clusterGap: 16
    readonly property int fontPx: 10

    implicitWidth: clusterRow.implicitWidth
    implicitHeight: clusterRow.implicitHeight

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Plugin lights")

    function reset() {
        dispatched = {}
    }

    // Parent passes dispatch events through; the component stays
    // unaware of where they come from.
    function noteDispatch(id) {
        const next = Object.assign({}, dispatched)
        next[id] = true
        dispatched = next
    }

    // Per-plugin resting state — the stage's outcome across ALL its
    // trace entries (a fixpoint's last entry is always the noop; the
    // resting color reflects whether the stage ever changed or found
    // anything).
    function stateFor(id) {
        const entries = trace.filter(
            function (entry) { return entry.plugin === id })
        if (entries.length === 0)
            return dispatched[id] ? "shimmer" : "dim"
        let changed = false
        let verdict = ""
        for (let i = 0; i < entries.length; i++) {
            const data = entries[i].data || {}
            const v = data.verdict !== undefined
                      && data.verdict !== null
                      ? String(data.verdict).toLowerCase() : ""
            if (v === "malicious" || v === "dangerous"
                    || v === "phishing")
                return "red" // unmissable
            if (v !== "")
                verdict = v
            if (data.url !== undefined && data.url !== "")
                changed = true
            if ((data.strippedCount || 0) > 0)
                changed = true
            if (data.debounced === true || data.shortener === true)
                changed = true
        }
        if (verdict !== "" || changed)
            return "amber"
        return "green"
    }

    function colorFor(state) {
        if (state === "red")
            return "#f87171"
        if (state === "amber")
            return "#fbbf24"
        if (state === "green")
            return "#4ade80"
        return "#3394a3b8" // dim
    }

    function nameFor(state) {
        if (state === "red")
            return qsTr("dangerous")
        if (state === "amber")
            return qsTr("found or changed")
        if (state === "green")
            return qsTr("clean")
        if (state === "shimmer")
            return qsTr("working")
        return qsTr("queued")
    }

    Row {
        id: clusterRow

        anchors.horizontalCenter: parent.horizontalCenter
        spacing: lightsStrip.clusterGap

        Repeater {
            id: clusterRepeater

            model: lightsStrip.chain

            Item {
                id: cluster

                required property var modelData
                readonly property string pluginId: modelData.id
                readonly property string lightState:
                    lightsStrip.stateFor(pluginId)
                // trace-driven bulb: a new entry pulses the light
                property int lastSeen: 0
                readonly property int entryCount: {
                    let n = 0
                    const trace = lightsStrip.trace
                    for (let i = 0; i < trace.length; i++)
                        if (trace[i].plugin === pluginId)
                            n++
                    return n
                }

                onEntryCountChanged: {
                    if (entryCount > lastSeen && lastSeen >= 0) {
                        pulseAnimation.restart()
                        const next = Object.assign(
                            {}, lightsStrip.dispatched)
                        delete next[pluginId]
                        lightsStrip.dispatched = next
                    }
                    lastSeen = entryCount
                }

                width: Math.max(labelText.width, lightRow.width)
                height: lightRow.height + labelText.height

                Row {
                    id: lightRow

                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 4

                    Rectangle {
                        id: light

                        width: lightsStrip.lightSize
                        height: lightsStrip.lightSize
                        radius: lightsStrip.lightSize / 2
                        color: lightsStrip.colorFor(cluster.lightState)
                        border.width:
                            cluster.lightState === "dim" ? 1 : 0
                        border.color: "#4d94a3b8"

                        SequentialAnimation {
                            id: pulseAnimation

                            loops: 1
                            NumberAnimation {
                                target: light
                                property: "opacity"
                                from: 1.0
                                to: 0.2
                                duration: 90
                            }
                            NumberAnimation {
                                target: light
                                property: "opacity"
                                to: 1.0
                                duration: 160
                            }
                        }

                        SequentialAnimation {
                            running:
                                cluster.lightState === "shimmer"
                            loops: Animation.Infinite
                            NumberAnimation {
                                target: light
                                property: "opacity"
                                from: 1.0
                                to: 0.3
                                duration: 450
                            }
                            NumberAnimation {
                                target: light
                                property: "opacity"
                                to: 1.0
                                duration: 450
                            }
                        }
                    }

                    // "online" marker for declared-online plugins —
                    // outside requests stay transparent.
                    Rectangle {
                        visible: cluster.modelData.inet === true
                        width: 4
                        height: 4
                        radius: 2
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#00e5ff"
                    }
                }

                Text {
                    id: labelText

                    anchors.top: lightRow.bottom
                    anchors.topMargin: lightsStrip.labelGap
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: cluster.modelData.title
                    color: cluster.lightState === "dim"
                           || cluster.lightState === "shimmer"
                           ? "#94a3b8" : "#cbd5e1"
                    font.family: "Jura"
                    font.pixelSize: lightsStrip.fontPx
                    font.weight: Font.Light
                }

                Accessible.role: Accessible.Indicator
                Accessible.name: cluster.modelData.title + " — "
                                 + lightsStrip.nameFor(cluster.lightState)
                                 + (cluster.modelData.inet === true
                                    ? qsTr(" (online)") : "")
            }
        }
    }
}
