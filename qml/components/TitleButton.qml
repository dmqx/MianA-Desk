import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Button {
    id: control
    required property var controller
    implicitWidth: 26
    implicitHeight: 26
    padding: 4
    flat: true
    focusPolicy: Qt.NoFocus
    scale: control.down ? 0.88 : 1
    font.family: "Microsoft YaHei UI"
    font.pixelSize: 14
    contentItem: Text {
        text: control.text
        color: control.hovered ? control.controller.paletteText : control.controller.paletteMuted
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        color: control.hovered ? control.controller.palettePanel : "transparent"
        radius: 5
    }
    Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
}