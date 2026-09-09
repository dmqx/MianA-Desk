import QtQuick

Canvas {
    id: chart

    required property var series
    required property real previousClose
    required property color lineColor
    required property color hintColor
    property int aggregationSize: 5

    onVisibleChanged: if (visible) requestPaint()
    onPreviousCloseChanged: requestPaint()
    onSeriesChanged: requestPaint()
    onLineColorChanged: requestPaint()
    onHintColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        if (width <= 0 || height <= 0)
            return
        ctx.clearRect(0, 0, width, height)
        if (!series || series.length < 1)
            return

        const data = []
        if (series.length < aggregationSize * 2) {
            for (let i = 0; i < series.length; ++i)
                data.push(series[i])
        } else {
            for (let start = 0; start < series.length; start += aggregationSize) {
                let value = NaN
                for (let i = Math.min(start + aggregationSize, series.length) - 1;
                     i >= start; --i) {
                    if (typeof series[i] === "number" && isFinite(series[i])
                            && series[i] > 0) {
                        value = series[i]
                        break
                    }
                }
                data.push(value)
            }
        }

        let min = Infinity
        let max = -Infinity
        for (let i = 0; i < data.length; ++i) {
            if (typeof data[i] !== "number" || !isFinite(data[i]) || data[i] <= 0)
                continue
            min = Math.min(min, data[i])
            max = Math.max(max, data[i])
        }
        if (previousClose > 0 && isFinite(previousClose)) {
            min = Math.min(min, previousClose)
            max = Math.max(max, previousClose)
        }
        if (min === Infinity)
            return

        const range = max - min
        if (previousClose > 0) {
            const previousY = range > 0
                ? Math.max(0, Math.min(height - 1,
                    height - (previousClose - min) / range * (height - 2) - 1))
                : height / 2
            ctx.strokeStyle = hintColor
            ctx.lineWidth = 1
            ctx.setLineDash([3, 3])
            ctx.beginPath()
            ctx.moveTo(0, previousY)
            ctx.lineTo(width, previousY)
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
                if (!drawing)
                    ctx.moveTo(x, y)
                else
                    ctx.lineTo(x, y)
                drawing = true
            }
        }
        ctx.stroke()
    }
}
