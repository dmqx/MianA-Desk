import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: card
    required property AppController controller
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
            Sparkline {
                id: chart
                visible: card.controller.showIntraday
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                Layout.alignment: Qt.AlignVCenter
                series: card.intraday
                previousClose: card.hasChange ? card.previousClose : 0
                hintColor: card.controller.paletteHint
                lineColor: card.hasError ? card.controller.paletteWarning
                    : card.hasChange
                        ? (card.changePercent > 0 ? card.controller.paletteUp
                            : card.changePercent < 0 ? card.controller.paletteDown
                            : card.controller.paletteMuted)
                        : card.controller.paletteMuted
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
