pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "components" as Components

Window {
    id: dialog
    objectName: "positionEditor"
    required property var controller
    required property var mainWindow
    property string editingId: ""
    readonly property int pageWidth: 248
    readonly property int pageHeight: 260
    readonly property int titleBarHeight: 40
    readonly property color inputBorderColor: "#A6A6A6"
    readonly property color inputFocusBorderColor: "#707A82"
    visible: false
    title: editingId ? "编辑自选" : "添加自选"
    color: "transparent"
    modality: Qt.ApplicationModal
    transientParent: mainWindow
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: pageWidth
    height: pageHeight

    function openForAdd() {
        editingId = ""
        symbolField.text = ""
        nameField.text = ""
        errorText.text = ""
        placeNearMainWindow()
        show()
        raise()
        requestActivate()
        symbolField.forceActiveFocus()
    }
    function openForEdit(id, symbol, name) {
        editingId = id
        symbolField.text = symbol
        nameField.text = name
        errorText.text = ""
        placeNearMainWindow()
        show()
        raise()
        requestActivate()
        symbolField.forceActiveFocus()
    }

    function placeNearMainWindow() {
        const desiredX = mainWindow.x + (mainWindow.width - width) / 2
        const desiredY = mainWindow.y + 55
        x = Math.round(Math.max(Screen.virtualX + 8,
            Math.min(desiredX, Screen.virtualX + Screen.desktopAvailableWidth - width - 8)))
        y = Math.round(Math.max(Screen.virtualY + 8,
            Math.min(desiredY, Screen.virtualY + Screen.desktopAvailableHeight - height - 8)))
    }

    function submit() {
        const error = editingId
            ? dialog.controller.editPosition(editingId, symbolField.text, nameField.text)
            : dialog.controller.addPosition(symbolField.text, nameField.text)
        if (error.length > 0) {
            errorText.text = error
            symbolField.forceActiveFocus()
        } else {
            close()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: dialog.controller.paletteBg
        radius: 0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.bottomMargin: 14
        anchors.topMargin: 0
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: dialog.titleBarHeight
            RowLayout {
                anchors.fill: parent
                spacing: 6
                ColumnLayout {
                    spacing: 1
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    Label { text: dialog.editingId ? "编辑自选" : "添加自选"; color: dialog.controller.paletteText; font.family: "Microsoft YaHei UI"; font.pixelSize: 13; font.bold: true }
                    Label { text: "填写股票代码，名称可留空自动获取"; color: dialog.controller.paletteMuted; font.family: "Microsoft YaHei UI"; font.pixelSize: 9; Layout.fillWidth: true; elide: Text.ElideRight }
                }
                Components.TitleButton {
                    controller: dialog.controller
                    id: editorClose
                    text: "×"
                    Layout.alignment: Qt.AlignTop | Qt.AlignRight
                    Layout.topMargin: 2
                    Layout.rightMargin: -8
                    z: 2
                    onClicked: dialog.close()
                }
            }
            DragHandler { target: null; onActiveChanged: if (active) dialog.startSystemMove() }
        }

        Item { Layout.preferredHeight: 8 }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 158
            radius: 8
            color: dialog.controller.palettePanel
            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 11
                anchors.topMargin: 11
                anchors.bottomMargin: 11
                spacing: 4
                Label { text: "股票代码"; color: dialog.controller.paletteText; font.family: "Microsoft YaHei UI"; font.pixelSize: 12; font.bold: true }
                Basic.TextField {
                    id: symbolField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    placeholderText: "例如 600519、0700.HK、AAPL"
                    placeholderTextColor: dialog.controller.paletteHint
                    color: dialog.controller.paletteText
                    font.family: "Microsoft YaHei UI"
                    selectByMouse: true
                    leftPadding: 10
                    rightPadding: 10
                    verticalAlignment: TextInput.AlignVCenter
                    selectionColor: dialog.controller.paletteMuted
                    selectedTextColor: dialog.controller.paletteBg
                    background: Rectangle {
                        color: dialog.controller.paletteBg
                        radius: 7
                        border.width: 1
                        border.color: symbolField.activeFocus ? dialog.inputFocusBorderColor : dialog.inputBorderColor
                    }
                    onAccepted: dialog.submit()
                }
                Label { text: "股票名称"; color: dialog.controller.paletteText; font.family: "Microsoft YaHei UI"; font.pixelSize: 12; font.bold: true }
                Basic.TextField {
                    id: nameField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    placeholderText: "可留空，刷新后自动补全"
                    placeholderTextColor: dialog.controller.paletteHint
                    color: dialog.controller.paletteText
                    font.family: "Microsoft YaHei UI"
                    selectByMouse: true
                    leftPadding: 10
                    rightPadding: 10
                    verticalAlignment: TextInput.AlignVCenter
                    selectionColor: dialog.controller.paletteMuted
                    selectedTextColor: dialog.controller.paletteBg
                    background: Rectangle {
                        color: dialog.controller.paletteBg
                        radius: 7
                        border.width: 1
                        border.color: nameField.activeFocus ? dialog.inputFocusBorderColor : dialog.inputBorderColor
                    }
                    onAccepted: dialog.submit()
                }
                Label { id: errorText; color: "#E88B00"; visible: text.length > 0; Layout.fillWidth: true; font.pixelSize: 10; elide: Text.ElideRight }
            }
        }

        Item { Layout.fillHeight: true }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Components.NeutralButton { controller: dialog.controller; text: "保存"; prominent: true; onClicked: dialog.submit() }
            Components.NeutralButton { controller: dialog.controller; text: "取消"; onClicked: dialog.close() }
        }
    }

    Shortcut { sequences: [StandardKey.Cancel]; onActivated: dialog.close() }
}