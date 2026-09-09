import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Button {
    id: control

    enum Variant { Flat, Neutral, Title }
    required property AppController controller
    property int variant: AppButton.Flat
    property bool prominent: false

    readonly property bool neutral: variant === AppButton.Neutral
    readonly property bool titleButton: variant === AppButton.Title

    implicitWidth: titleButton ? 26
        : neutral ? Math.max(26, implicitContentWidth + 12)
        : implicitContentWidth + 8
    implicitHeight: neutral ? 28 : 26
    padding: neutral ? 6 : 4
    flat: !neutral
    focusPolicy: Qt.NoFocus
    hoverEnabled: true
    scale: down ? 0.88 : 1
    font.family: "Microsoft YaHei UI"
    font.pixelSize: titleButton ? 14 : 9

    contentItem: Text {
        text: control.text
        color: !control.enabled ? control.controller.paletteHint
            : control.titleButton && control.hovered ? control.controller.paletteText
            : control.neutral ? control.controller.paletteText
            : control.controller.paletteMuted
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        color: control.neutral
            ? (control.down || control.prominent ? control.controller.paletteHover
               : control.hovered ? control.controller.palettePanel : "transparent")
            : control.hovered ? control.controller.palettePanel : "transparent"
        radius: control.titleButton ? 5 : 4
        border.width: control.neutral ? 1 : 0
        border.color: control.controller.paletteLine
    }
    Behavior on scale {
        NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
    }
}
