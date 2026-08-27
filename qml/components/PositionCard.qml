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
    property bool reorderActive: false
    property real reorderOffsetY: 0

    signal selected()
    signal contextRequested(real x, real y)
    signal longPressed(real y)
    signal dragMoved(real y)
    signal dragFinished(bool commit)

    height: 55
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
            anchors.right: price.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            Text {
                width: parent.width
                height: 19
                text: card.name.length <= 14 ? card.name : card.name.slice(0, 13) + "…"
                color: card.controller.paletteText
                font.family: "Microsoft YaHei UI"
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                height: 16
                text: card.symbol
                color: card.hasError ? "#E88B00" : card.controller.paletteMuted
                font.family: "Microsoft YaHei UI"
                font.pixelSize: 9
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        Text {
            id: price
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            width: 88
            height: 24
            text: card.priceText
            color: card.hasError ? "#E88B00" : card.controller.paletteText
            font.family: "Microsoft YaHei UI"
            font.pixelSize: 12
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
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
