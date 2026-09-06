import QtQuick
import QtQuick.Effects

Item {
    id: card
    required property var controller
    required property int index
    required property string positionId
    required property string symbol
    required property string name
    required property string priceText
    required property bool hasError
    required property double changePercent
    required property double previousClose
    required property bool hasChange
    required property var intraday
    property bool reorderActive: false
    property real reorderOffsetY: 0

    signal selected()
    signal contextRequested(real x, real y)
    signal longPressed(real y)
    signal dragMoved(real y)
    signal dragFinished(bool commit)

    height: card.controller.showIntraday ? 84 : 55
    z: reorderActive ? 20 : 0
    transform: Translate { y: card.reorderOffsetY }

    Rectangle {
        id: surface
        anchors.fill: parent
        radius: 7
        color: pointer.containsMouse
            ? card.controller.paletteHover : card.controller.palettePanel
        border.width: 1
        border.color: card.controller.paletteLine
        scale: card.reorderActive ? 1.008
            : pointer.pressed ? 0.985 : (pointer.containsMouse ? 1.006 : 1)
        layer.enabled: card.reorderActive
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#42000000"
            shadowBlur: 0.16
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 1
            shadowScale: 0.995
        }

        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }

        Column {
            id: identity
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.right: quote.left
            anchors.rightMargin: 8
            anchors.top: card.controller.showIntraday ? parent.top : undefined
            anchors.topMargin: 8
            anchors.verticalCenter: card.controller.showIntraday ? undefined : parent.verticalCenter
            spacing: 0

            Text {
                width: parent.width
                height: 19
                text: card.name.length <= 14 ? card.name : card.name.slice(0, 13) + "…"
                color: card.controller.paletteText
                font.family: "Microsoft YaHei UI"
                font.pixelSize: 12
                font.bold: true
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                height: 16
                text: card.symbol
                visible: !card.controller.hideStockCode
                color: card.hasError ? card.controller.paletteWarning : card.controller.paletteMuted
                font.family: "Microsoft YaHei UI"
                font.pixelSize: 9
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        Column {
            id: quote
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.top: card.controller.showIntraday ? parent.top : undefined
            anchors.topMargin: 8
            anchors.verticalCenter: card.controller.showIntraday ? undefined : parent.verticalCenter
            spacing: 0

            Text {
                id: price
                width: 88
                height: 19
                text: card.priceText
                color: card.hasError ? card.controller.paletteWarning : card.controller.paletteText
                font.family: "Microsoft YaHei UI"
                font.pixelSize: 12
                font.bold: true
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                id: change
                width: 88
                height: 16
                text: card.hasChange && !card.hasError
                    ? (card.changePercent > 0
                        ? "+" + card.changePercent.toFixed(2)
                        : card.changePercent < 0
                            ? "-" + Math.abs(card.changePercent).toFixed(2)
                            : card.changePercent.toFixed(2)) + "%"
                    : "--"
                color: card.hasError ? card.controller.paletteWarning
                    : card.hasChange
                        ? (card.changePercent > 0 ? card.controller.paletteUp
                            : card.changePercent < 0 ? card.controller.paletteDown
                            : card.controller.paletteMuted)
                        : card.controller.paletteMuted
                font.family: "Microsoft YaHei UI"
                font.pixelSize: 9
                font.bold: true
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }
        }

        Canvas {
            id: chart
            visible: card.controller.showIntraday
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 3
            height: 28
            property var series: card.intraday
            property color lineColor: card.hasError ? card.controller.paletteWarning
                : card.hasChange
                    ? (card.changePercent > 0 ? card.controller.paletteUp
                        : card.changePercent < 0 ? card.controller.paletteDown
                        : card.controller.paletteMuted)
                    : card.controller.paletteMuted
            property real previousCloseForPaint: card.previousClose
            onPreviousCloseForPaintChanged: requestPaint()
            onSeriesChanged: requestPaint()
            onLineColorChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                if (width <= 0 || height <= 0)
                    return
                ctx.clearRect(0, 0, width, height)
                // 5分钟聚合显示，减少窄列宽下的锯齿。
                const raw = series
                if (!raw || raw.length < 1)
                    return
                const data = []
                if (raw.length < 10) {
                    for (let i = 0; i < raw.length; ++i)
                        data.push(raw[i])
                } else {
                    for (let start = 0; start < raw.length; start += 5) {
                        let value = NaN
                        for (let i = Math.min(start + 5, raw.length) - 1; i >= start; --i) {
                            if (typeof raw[i] === "number" && isFinite(raw[i]) && raw[i] > 0) {
                                value = raw[i]
                                break
                            }
                        }
                        data.push(value)
                    }
                }
                let min = Infinity, max = -Infinity
                for (let i = 0; i < data.length; ++i) {
                    if (typeof data[i] !== "number" || !isFinite(data[i]) || data[i] <= 0) continue
                    if (data[i] < min) min = data[i]
                    if (data[i] > max) max = data[i]
                }
                if (card.previousClose > 0 && isFinite(card.previousClose)) {
                    min = Math.min(min, card.previousClose)
                    max = Math.max(max, card.previousClose)
                }
                if (min === Infinity) return
                const range = max - min
                if (card.previousClose > 0 && card.hasChange) {
                    const prevClose = card.previousClose
                    const py = range > 0
                        ? Math.max(0, Math.min(height - 1,
                            height - (prevClose - min) / range * (height - 2) - 1))
                        : height / 2
                    ctx.strokeStyle = card.controller.paletteHint
                    ctx.lineWidth = 1
                    ctx.setLineDash([3, 3])
                    ctx.beginPath()
                    ctx.moveTo(0, py)
                    ctx.lineTo(width, py)
                    ctx.stroke()
                    ctx.setLineDash([])
                }
                ctx.strokeStyle = lineColor
                ctx.lineWidth = 1
                ctx.beginPath()
                if (data.length === 1) {
                    const x = width / 2
                    const y = range > 0
                        ? height - (data[0] - min) / range * (height - 2) - 1
                        : height / 2
                    ctx.moveTo(x - 1, y)
                    ctx.lineTo(x + 1, y)
                } else {
                    let drawing = false
                    for (let i = 0; i < data.length; ++i) {
                        if (typeof data[i] !== "number" || !isFinite(data[i]) || data[i] <= 0) {
                            drawing = false
                            continue
                        }
                        const x = i / (data.length - 1) * width
                        const y = range > 0
                            ? height - (data[i] - min) / range * (height - 2) - 1
                            : height / 2
                        if (!drawing) ctx.moveTo(x, y)
                        else ctx.lineTo(x, y)
                        drawing = true
                    }
                }
                ctx.stroke()
            }
        }

        MouseArea {
            id: pointer
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            pressAndHoldInterval: 150
            preventStealing: true

            onPressed: function(mouse) {
                if (mouse.button === Qt.LeftButton)
                    card.selected()
                else if (mouse.button === Qt.RightButton)
                    card.contextRequested(mouse.x, mouse.y)
            }
            onPressAndHold: function(mouse) {
                if (mouse.button !== Qt.LeftButton)
                    return
                card.reorderActive = true
                card.longPressed(mouse.y)
            }
            onPositionChanged: function(mouse) {
                if (card.reorderActive)
                    card.dragMoved(mouse.y)
            }
            onReleased: {
                if (!card.reorderActive)
                    return
                card.reorderActive = false
                card.dragFinished(true)
            }
            onCanceled: {
                if (!card.reorderActive)
                    return
                card.reorderActive = false
                card.dragFinished(false)
            }
        }
    }
}
