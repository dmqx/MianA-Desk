pragma ComponentBehavior: Bound
// Loader.item is a dynamically created SettingsWindow, although qmllint sees QObject.
// qmllint disable missing-property

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "windows" as Windows

ApplicationWindow {
    id: root
    required property var appController
    visible: true
    title: "MianA Desk · 股票桌面小组件"
    color: "transparent"
    readonly property int pageWidth: appController.floating ? 150 : 288
    readonly property int pageHeight: appController.floating ? 34
        : appController.compact ? Math.min(340, 48 + Math.max(1, appController.count) * 34) : 340
    flags: Qt.FramelessWindowHint
        | (appController.topmost ? Qt.Window
           : (appController.floating ? Qt.Tool : Qt.Window))
        | (appController.topmost ? Qt.WindowStaysOnTopHint : 0)
    x: appController.savedX
    y: appController.savedY
    width: pageWidth
    height: pageHeight
    minimumWidth: pageWidth
    maximumWidth: pageWidth
    minimumHeight: pageHeight
    maximumHeight: pageHeight
    opacity: appController.themeOpacity / 100

    function ensureVisible() {
        const left = Screen.virtualX
        const top = Screen.virtualY
        const right = left + Screen.desktopAvailableWidth
        const bottom = top + Screen.desktopAvailableHeight
        if (x + width <= left || x >= right || y + 40 <= top || y >= bottom) {
            x = Math.round(Math.max(left, Math.min(x, right - width)))
            y = Math.round(Math.max(top, Math.min(y, bottom - height)))
        }
    }
    function showActive() { show(); raise(); requestActivate(); Qt.callLater(ensureVisible) }
    function openSettings() {
        if (settingsLoader.item) settingsLoader.item["openForSettings"]()
        else settingsLoader.active = true
    }

    onXChanged: appController.setWindowPosition(Math.round(x), Math.round(y))
    onYChanged: appController.setWindowPosition(Math.round(x), Math.round(y))
    onWidthChanged: if (visible) Qt.callLater(ensureVisible)
    onHeightChanged: if (visible) Qt.callLater(ensureVisible)
    onClosing: function(close) {
        if (appController.minimizeToTray) { close.accepted = false; hide() }
        else appController.saveAndShutdown()
    }
    Component.onCompleted: Qt.callLater(ensureVisible)

    Connections {
        target: root.appController
        function onShowMainRequested() {
            if (settingsLoader.item) settingsLoader.item["closeSettings"]()
            root.showActive()
        }
        function onToggleMainRequested() { root.visible ? root.hide() : root.showActive() }
        function onSettingsRequested() { root.openSettings() }
    }

    Timer {
        interval: 1200
        repeat: true
        running: root.visible && root.appController.autoTheme
        triggeredOnStart: true
        onTriggered: root.appController.sampleTheme(root.x, root.y, root.width, root.height)
    }

    background: Rectangle {
        color: root.appController.floating ? root.appController.palettePanel : root.appController.paletteBg
    }

    Loader {
        id: modeLoader
        anchors.fill: parent
        sourceComponent: root.appController.floating ? floatingComponent
            : root.appController.compact ? compactComponent : fullComponent
    }
    Component {
        id: fullComponent
        Windows.MainWindow {
            controller: root.appController
            mainWindow: root
            onAddRequested: editor.openForAdd()
            onEditRequested: function(positionId, symbol, name) { editor.openForEdit(positionId, symbol, name) }
            onDeleteRequested: function(positionId) { deleteDialog.positionId = positionId; deleteDialog.open() }
        }
    }
    Component {
        id: compactComponent
        Windows.CompactWindow { controller: root.appController; mainWindow: root }
    }
    Component {
        id: floatingComponent
        Windows.FloatingWindow { controller: root.appController; mainWindow: root }
    }

    PositionEditor { id: editor; controller: root.appController; mainWindow: root }

    Dialog {
        id: deleteDialog
        property string positionId: ""
        width: 224
        height: 120
        modal: true
        dim: true
        anchors.centerIn: parent
        padding: 14
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: root.appController.paletteBg; radius: 8; border.width: 1; border.color: root.appController.paletteLine }
        contentItem: ColumnLayout {
            spacing: 5
            Label { text: "删除自选"; color: root.appController.paletteText; font.pixelSize: 12; font.bold: true }
            Label { text: "确定删除这只自选吗？"; color: root.appController.paletteMuted; font.pixelSize: 10 }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Basic.Button { text: "取消"; onClicked: deleteDialog.close() }
                Basic.Button {
                    text: "删除"
                    onClicked: { root.appController.deletePosition(deleteDialog.positionId); deleteDialog.close() }
                }
            }
        }
    }

    Component {
        id: settingsComponent
        Windows.SettingsWindow { controller: root.appController; mainWindow: root }
    }
    Loader {
        id: settingsLoader
        active: false
        sourceComponent: settingsComponent
        onLoaded: item["openForSettings"]()
    }
}
