import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Button {
    id: control
    required property var controller
    property bool active: false
    implicitHeight: 26
    padding: 4
    flat: true
    focusPolicy: Qt.NoFocus
    hoverEnabled: true
    scale: down ? 0.88 : 1
    font.family: "Microsoft YaHei UI"
    font.pixelSize: 9
    contentItem: Text {
        text: control.text
        color: control.active ? "#0A84FF" : control.controller.paletteMuted
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        color: control.hovered ? control.controller.palettePanel : "transparent"
        radius: 4
    }
    Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
}
