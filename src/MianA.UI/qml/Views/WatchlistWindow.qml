pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Controls" as Components

ApplicationWindow {
    id: manager
    objectName: "watchlistWindow"
    required property AppController controller
    required property Window mainWindow
    property bool hideBorderShadow: controller.hideBorderShadow
    visible: false
    title: "编辑自选"
    color: "transparent"
    font.family: "Microsoft YaHei UI"
    font.pixelSize: 10
    width: 240
    readonly property int titleBarHeight: 32
    readonly property int visibleRows: Math.min(6, Math.max(1, controller.count))
    height: titleBarHeight + 17 + visibleRows * 36
    onHeightChanged: if (visible) y = Math.round(Math.max(Screen.virtualY + 8,
        Math.min(y, Screen.virtualY + Screen.desktopAvailableHeight - height - 8)))
    flags: Qt.FramelessWindowHint | Qt.Tool
        | (controller.topmost ? Qt.WindowStaysOnTopHint : 0)
        | (controller.hideBorderShadow ? Qt.NoDropShadowWindowHint : 0)
    transientParent: mainWindow
    palette.windowText: controller.paletteText
    palette.text: controller.paletteText

    function openForManagement() {
        x = Math.round(Math.max(Screen.virtualX + 8,
            Math.min(mainWindow.x + (mainWindow.width - width) / 2,
                     Screen.virtualX + Screen.desktopAvailableWidth - width - 8)))
        y = Math.round(Math.max(Screen.virtualY + 8,
            Math.min(mainWindow.y + 40, Screen.virtualY + Screen.desktopAvailableHeight - height - 8)))
        show()
        raise()
        requestActivate()
    }

    background: Rectangle {
        color: manager.controller.paletteBg
        radius: manager.controller.hideBorderShadow ? 8 : 0
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: manager.titleBarHeight
            DragHandler { target: null; onActiveChanged: if (active) manager.startSystemMove() }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 7
                Label { text: "编辑自选"; color: manager.controller.paletteText; font.pixelSize: 10; font.bold: true; Layout.fillWidth: true }
                Components.AppButton {
                    controller: manager.controller
                    text: "＋"
                    variant: Components.AppButton.Title
                    Accessible.name: "添加自选"
                    onClicked: editor.openForAdd()
                }
                Components.AppButton { controller: manager.controller; text: "×"; variant: Components.AppButton.Title; onClicked: manager.close() }
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.topMargin: 5
            Layout.bottomMargin: 16
            ListView {
                id: positionsList
                objectName: "watchlistList"
                anchors.fill: parent
                clip: true
                spacing: 4
                model: manager.controller.positions
                delegate: Rectangle {
                    id: row
                    required property string positionId
                    required property string symbol
                    required property string name
                    width: positionsList.width
                    height: 32
                    color: "transparent"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 12
                        Label { text: row.name; color: manager.controller.paletteText; font.pixelSize: 9; elide: Text.ElideRight; Layout.preferredWidth: Math.min(implicitWidth, 80) }
                        Label { text: row.symbol; color: manager.controller.paletteMuted; font.pixelSize: 9; elide: Text.ElideRight; Layout.preferredWidth: Math.min(implicitWidth, 72) }
                        Item { Layout.fillWidth: true }
                        Components.AppButton { objectName: "editPositionButton"; controller: manager.controller; text: "编辑"; padding: 0; implicitWidth: implicitContentWidth; onClicked: editor.openForEdit(row.positionId, row.symbol, row.name) }
                        Components.AppButton { objectName: "deletePositionButton"; controller: manager.controller; text: "删除"; padding: 0; implicitWidth: implicitContentWidth; onClicked: { deleteDialog.positionId = row.positionId; deleteDialog.open() } }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: manager.controller.count === 0
                    text: "暂无自选，点击右上角＋添加"
                    color: manager.controller.paletteMuted
                    font.pixelSize: 10
                }
            }
        }
    }
    PositionEditor { id: editor; controller: manager.controller; mainWindow: manager }

    Dialog {
        id: deleteDialog
        objectName: "deleteDialog"
        property string positionId: ""
        readonly property color panelColor: manager.controller.paletteBg
        readonly property color textColor: manager.controller.paletteText
        readonly property color mutedColor: manager.controller.paletteMuted
        readonly property color lineColor: manager.controller.paletteLine
        readonly property color hoverColor: manager.controller.paletteHover
        function solid(value) { return Qt.rgba(value.r, value.g, value.b, 1) }
        component ConfirmationButton: Components.AppButton {
            id: control
            controller: manager.controller
            variant: Components.AppButton.Neutral
            contentItem: Text {
                text: control.text
                font: control.font
                color: deleteDialog.solid(deleteDialog.textColor)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: control.hovered || control.down ? deleteDialog.solid(deleteDialog.hoverColor) : "transparent"
                radius: 4
                border.width: 1
                border.color: deleteDialog.solid(deleteDialog.lineColor)
            }
        }
        width: 208
        height: 80
        modal: true
        dim: true
        anchors.centerIn: parent
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: deleteDialog.solid(deleteDialog.panelColor); radius: 8; border.width: 1; border.color: deleteDialog.solid(deleteDialog.lineColor) }
        contentItem: ColumnLayout {
            spacing: 3
            Label { text: "删除自选"; color: deleteDialog.solid(deleteDialog.textColor); font.pixelSize: 11; font.bold: true }
            Label { text: "确定删除这只自选吗？"; color: deleteDialog.solid(deleteDialog.mutedColor); font.pixelSize: 10 }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                ConfirmationButton { text: "取消"; onClicked: deleteDialog.close() }
                ConfirmationButton {
                    text: "删除"
                    onClicked: { manager.controller.deletePosition(deleteDialog.positionId); deleteDialog.close() }
                }
            }
        }
    }

    Shortcut { sequences: [StandardKey.Cancel]; enabled: manager.visible && !deleteDialog.visible && !editor.visible; onActivated: manager.close() }
}
