import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: card
    required property AppController controller
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

    signal depthRequested(string positionId)
    signal longPressed(real y)
    signal dragMoved(real y)
    signal dragFinished(bool commit)

    height: 40
    z: reorderActive ? 20 : 0
    transform: Translate { y: card.reorderOffsetY }

    Rectangle {
        id: surface
        readonly property color hoverColor: card.controller.paletteHover
        anchors.fill: parent
        color: card.controller.autoTheme
            ? (pointer.containsMouse ? Qt.rgba(hoverColor.r, hoverColor.g, hoverColor.b, 0.06) : "transparent")
            : (pointer.containsMouse ? card.controller.paletteHover : card.controller.palettePanel)
        scale: card.reorderActive ? 1.008
            : pointer.pressed ? 0.985 : (pointer.containsMouse ? 1.006 : 1)

        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 120 } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 6

            ColumnLayout {
                Layout.preferredWidth: 54
                Layout.minimumWidth: 54
                Layout.maximumWidth: 54
                Layout.alignment: Qt.AlignVCenter
                spacing: 1
                Label {
                    Layout.fillWidth: true
                    text: card.name
                    color: card.hasError ? card.controller.paletteWarning : card.controller.paletteMuted
                    font.pixelSize: 9
                    horizontalAlignment: Text.AlignLeft
                    font.bold: true
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    visible: !card.controller.hideStockCode
                    text: card.symbol.replace(/\.[A-Za-z]+$/, "")
                    color: card.hasError ? card.controller.paletteWarning : card.controller.paletteMuted
                    font.pixelSize: 8
                    horizontalAlignment: Text.AlignLeft
                    font.bold: true
                    elide: Text.ElideRight
                }
            }
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                Layout.alignment: Qt.AlignVCenter
                Sparkline {
                    id: chart
                    anchors.fill: parent
                    visible: card.controller.showIntraday
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
            pressAndHoldInterval: 200
            preventStealing: card.reorderActive

            onPressed: card.controller.setFocus(card.positionId)
            onDoubleClicked: function(mouse) {
                if (mouse.button === Qt.LeftButton && !card.reorderActive)
                    card.depthRequested(card.positionId)
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
