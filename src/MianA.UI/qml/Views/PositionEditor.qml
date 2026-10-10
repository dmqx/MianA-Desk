pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "../Controls" as Components

Window {
    id: dialog
    objectName: "positionEditor"
    required property AppController controller
    property bool hideBorderShadow: controller.hideBorderShadow
    required property Window mainWindow
    property string editingId: ""
    readonly property int pageWidth: mainWindow.width
    readonly property int pageHeight: titleBarHeight + formPanel.implicitHeight + 38
    readonly property int titleBarHeight: 32
    readonly property color inputBorderColor: "#A6A6A6"
    readonly property color inputFocusBorderColor: "#707A82"
    visible: false
    title: editingId ? "编辑自选" : "添加自选"
    color: "transparent"
    modality: Qt.ApplicationModal
    transientParent: mainWindow
    flags: Qt.FramelessWindowHint
        | (controller.hideBorderShadow ? Qt.NoDropShadowWindowHint : 0)
        | Qt.Tool | Qt.WindowStaysOnTopHint
    width: pageWidth
    height: pageHeight
    onHeightChanged: if (visible) y = Math.round(Math.max(Screen.virtualY + 8,
        Math.min(y, Screen.virtualY + Screen.desktopAvailableHeight - height - 8)))

    component PositionField: Basic.TextField {
        id: field
        Layout.fillWidth: true
        Layout.preferredHeight: 24
        placeholderTextColor: dialog.controller.paletteHint
        color: dialog.controller.paletteText
        font.family: "Microsoft YaHei UI"
        font.pixelSize: 9
        selectByMouse: true
        leftPadding: 8
        rightPadding: 8
        verticalAlignment: TextInput.AlignVCenter
        selectionColor: dialog.controller.paletteMuted
        selectedTextColor: dialog.controller.paletteBg
        background: Rectangle {
            color: dialog.controller.paletteBg
            radius: 7
            border.width: 1
            border.color: field.activeFocus ? dialog.inputFocusBorderColor : dialog.inputBorderColor
        }
        onAccepted: dialog.submit()
    }

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
        radius: dialog.controller.hideBorderShadow ? 8 : 0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.bottomMargin: 6
        anchors.topMargin: 0
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: dialog.titleBarHeight
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 7
                Label { text: dialog.editingId ? "编辑自选" : "添加自选"; color: dialog.controller.paletteText; font.family: "Microsoft YaHei UI"; font.pixelSize: 10; font.bold: true; Layout.fillWidth: true }
                Components.AppButton { variant: Components.AppButton.Title; controller: dialog.controller; text: "×"; onClicked: dialog.close() }
            }
            DragHandler { target: null; onActiveChanged: if (active) dialog.startSystemMove() }
        }

        Item { Layout.preferredHeight: 4 }
        Rectangle {
            id: formPanel
            implicitHeight: formFields.implicitHeight + 12
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.preferredHeight: implicitHeight
            radius: 8
            color: dialog.controller.palettePanel
            ColumnLayout {
                id: formFields
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 2
                Label { text: "股票代码"; color: dialog.controller.paletteText; font.family: "Microsoft YaHei UI"; font.pixelSize: 9; font.bold: true }
                PositionField { id: symbolField }
                Label { text: "股票名称（可留空）"; color: dialog.controller.paletteText; font.family: "Microsoft YaHei UI"; font.pixelSize: 9; font.bold: true; Layout.topMargin: 2 }
                PositionField { id: nameField }
                Label { id: errorText; color: dialog.controller.paletteWarning; visible: text.length > 0; Layout.fillWidth: true; font.pixelSize: 9; elide: Text.ElideRight }
            }
        }

        Item { Layout.preferredHeight: 4 }
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Item { Layout.fillWidth: true }
            Components.AppButton { variant: Components.AppButton.Neutral; controller: dialog.controller; implicitHeight: 24; text: "保存"; prominent: true; onClicked: dialog.submit() }
            Components.AppButton { variant: Components.AppButton.Neutral; controller: dialog.controller; implicitHeight: 24; text: "取消"; onClicked: dialog.close() }
        }
    }

    Shortcut { sequences: [StandardKey.Cancel]; onActivated: dialog.close() }
}
