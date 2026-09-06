import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: card
    required property var controller
    required property string positionId
    required property string symbol
    required property string name
    required property string priceText
    required property bool hasError
    required property double changePercent
    required property double previousClose
    required property bool hasChange
    required property var intraday

    height: 40

    Rectangle {
        id: surface
        anchors.fill: parent
        color: pointer.containsMouse
            ? card.controller.paletteHover : card.controller.palettePanel
        scale: pointer.pressed ? 0.985 : (pointer.containsMouse ? 1.006 : 1)

        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 120 } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 6

            Label {
                text: card.controller.hideStockCode ? card.name : card.symbol.replace(/\.[A-Za-z]+$/, "")
                color: card.hasError ? card.controller.paletteWarning : card.controller.paletteMuted
                font.pixelSize: card.controller.hideStockCode ? 9 : 10
                font.bold: card.controller.hideStockCode
                Layout.preferredWidth: 54
                elide: Text.ElideRight
            }
            Canvas {
                id: chart
                visible: card.controller.showIntraday
                onVisibleChanged: if (visible) requestPaint()
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                Layout.alignment: Qt.AlignVCenter
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
                    const raw = series
                    if (!raw || raw.length < 1)
                        return
                    // 5分钟聚合显示：每5个1分钟点取1个，减少窄列宽下的锯齿
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
            Label {
                text: card.priceText
                color: card.hasError ? card.controller.paletteWarning : card.controller.paletteMuted
                font.pixelSize: 10
                font.bold: true
                Layout.preferredWidth: 30
                horizontalAlignment: Text.AlignRight
            }
            Label {
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
                font.pixelSize: 10
                font.bold: true
                horizontalAlignment: Text.AlignRight
                Layout.preferredWidth: 38
            }
        }

        MouseArea {
            id: pointer
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            pressAndHoldInterval: 150
            preventStealing: true

            onClicked: card.controller.setFocus(card.positionId)
        }
    }
}
