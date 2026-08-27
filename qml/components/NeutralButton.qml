import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Button {
    id: control
    required property var controller
    property bool prominent: false
    implicitWidth: Math.max(26, implicitContentWidth + 12)
    implicitHeight: 28
    padding: 6
    focusPolicy: Qt.NoFocus
    font.family: "Microsoft YaHei UI"
    scale: control.down ? 0.88 : 1
    contentItem: Text {
        text: control.text
        color: control.enabled ? control.controller.paletteText : control.controller.paletteHint
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        color: control.down || control.prominent ? control.controller.paletteHover : (control.hovered ? control.controller.palettePanel : "transparent")
        radius: 4
        border.width: 1
        border.color: control.controller.paletteLine
    }
    Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
}