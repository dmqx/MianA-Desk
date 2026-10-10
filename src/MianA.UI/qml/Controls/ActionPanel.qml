pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

Item {
    id: panel
    objectName: "actionsPanel"
    required property AppController controller
    signal manageRequested()
    implicitWidth: Math.ceil(menuTextMetrics.width) + 40
    implicitHeight: 136

    TextMetrics { id: menuTextMetrics; text: "锁定位置"; font.family: "Microsoft YaHei UI"; font.pixelSize: 10 }
    component ActionButton: Basic.Button {
        id: action
        property bool marked: false
        Layout.fillWidth: true
        implicitHeight: 24
        leftPadding: 10
        rightPadding: 20
        topPadding: 3
        bottomPadding: 3
        font.family: "Microsoft YaHei UI"
        font.pixelSize: 10
        contentItem: Text { text: action.text; font: action.font; color: panel.controller.paletteText; verticalAlignment: Text.AlignVCenter }
        Text {
            objectName: "actionCheck"
            x: action.width - width - 10
            y: (action.height - height) / 2
            width: 10
            text: action.marked ? "✓" : ""
            font.family: "Segoe UI Symbol"
            font.pixelSize: 10
            color: panel.controller.paletteText
            horizontalAlignment: Text.AlignHCenter
        }
        background: Rectangle { color: action.hovered || action.down ? panel.controller.paletteHover : "transparent"; radius: 4 }
    }
    ColumnLayout {
        x: 2
        y: Math.round((parent.height - height) / 2)
        width: parent.width - 4
        height: implicitHeight
        spacing: 0
        ActionButton { objectName: "floatingAction"; text: "浮窗模式"; onClicked: { panel.controller.toggle("floating") } }
        ActionButton { objectName: "lockedAction"; text: "锁定位置"; marked: panel.controller.locked; Accessible.checkable: true; Accessible.checked: marked; onClicked: { panel.controller.toggle("locked") } }
        ActionButton { objectName: "topmostAction"; text: "窗口置顶"; marked: panel.controller.topmost; Accessible.checkable: true; Accessible.checked: marked; onClicked: { panel.controller.toggle("topmost") } }
        Item { Layout.fillWidth: true; Layout.preferredHeight: 7; Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 1; color: panel.controller.paletteLine; opacity: panel.controller.autoTheme ? 0.2 : 1 } }
        ActionButton { objectName: "manageAction"; text: "编辑自选"; onClicked: panel.manageRequested() }
        ActionButton { objectName: "refreshAction"; text: "刷新行情"; onClicked: { panel.controller.manualRefresh() } }
    }
}
