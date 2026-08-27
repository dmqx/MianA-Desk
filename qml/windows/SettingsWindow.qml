pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "../components" as Components

Window {
    id: settings
    objectName: "settingsWindow"
    required property var controller
    required property var mainWindow
    readonly property int pageWidth: 288
    readonly property int pageHeight: 440
    readonly property int titleBarHeight: 40
    width: pageWidth
    height: pageHeight
    minimumWidth: pageWidth
    maximumWidth: pageWidth
    visible: false
    title: "设置"
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool
    modality: Qt.NonModal
    transientParent: mainWindow

    readonly property color bg: settings.controller.paletteBg
    readonly property color panel: settings.controller.palettePanel
    readonly property color muted: settings.controller.paletteMuted
    readonly property color textColor: settings.controller.paletteText
    property bool initializing: false
    palette.windowText: settings.textColor
    palette.text: settings.textColor
    TextMetrics { id: emailMetrics; text: "dmqx@foxmail.com"; font.family: "Microsoft YaHei UI"; font.pixelSize: 12 }

    component CompactCheckBox: Basic.CheckBox {
        id: control
        padding: 0
        spacing: 6
        focusPolicy: Qt.NoFocus
        palette.windowText: settings.textColor
        indicator: Rectangle {
            implicitWidth: 16
            implicitHeight: 16
            x: control.leftPadding
            y: control.topPadding + (control.availableHeight - height) / 2
            radius: 4
            border.width: 1
            border.color: control.hovered ? settings.textColor : control.palette.mid
            color: control.hovered ? settings.controller.paletteHover : settings.panel
            scale: control.hovered ? 1.12 : 1.0
            transformOrigin: Item.Center
            Behavior on border.color { ColorAnimation { duration: 120 } }
            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            Text {
                anchors.centerIn: parent
                text: "✔"
                font.family: "Segoe UI Symbol"
                font.pixelSize: 12
                color: settings.textColor
                opacity: control.checked ? 1 : 0
                scale: control.checked ? 1 : 0
                transformOrigin: Item.Center
                Behavior on opacity { NumberAnimation { duration: 100 } }
                Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutBack } }
            }
        }
    }

    function openForSettings() {
        show()
        raise()
        requestActivate()
    }

    function applySettings() {
        if (initializing)
            return
        settings.controller.saveSettings(paused.checked, floating.checked,
            tray.checked, autostart.checked, autoTheme.checked,
            themeColor.text, Math.round(opacitySlider.value))
    }

    function closeSettings() {
        applySettings()
        hide()
    }

    onVisibleChanged: {
        if (visible) {
            initializing = true
            paused.checked = settings.controller.paused
            floating.checked = settings.controller.floating
            tray.checked = settings.controller.minimizeToTray
            autostart.checked = settings.controller.autostart
            autoTheme.checked = settings.controller.autoTheme
            themeColor.text = settings.controller.themeColor
            opacitySlider.value = settings.controller.themeOpacity
            initializing = false
            const desiredX = mainWindow.x + (mainWindow.width - width) / 2
            const desiredY = mainWindow.y + 30
            x = Math.max(Screen.virtualX + 8, Math.min(desiredX, Screen.virtualX + Screen.desktopAvailableWidth - width - 8))
            y = Math.max(Screen.virtualY + 8, Math.min(desiredY, Screen.virtualY + Screen.desktopAvailableHeight - height - 8))
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 0
        color: settings.bg
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 16
        anchors.bottomMargin: 12
        anchors.topMargin: 0
        spacing: 2

            Item {
                Layout.fillWidth: true
                // Title bar matches the 40 px height of the other windows;
                // the title text sits at the bottom, next to the divider.
                Layout.preferredHeight: settings.titleBarHeight
                RowLayout {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3
                    spacing: 6
                    Label { text: "设置"; color: settings.textColor; font.family: "Microsoft YaHei UI"; font.pixelSize: 16; font.bold: true; Layout.alignment: Qt.AlignBottom }
                    Label { text: "MianA Desk — A股桌面行情助手"; color: settings.muted; font.family: "Microsoft YaHei UI"; font.pixelSize: 11; Layout.alignment: Qt.AlignBottom }
                }
                Components.TitleButton {
                    controller: settings.controller
                    id: settingsClose
                    text: "×"
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.bottomMargin: 3
                    anchors.rightMargin: -10
                    z: 2
                    onClicked: settings.closeSettings()
                }
                DragHandler { target: null; onActiveChanged: if (active) settings.startSystemMove() }
            }


            Rectangle {
                id: appearanceGroup
                Layout.fillWidth: true
                Layout.preferredHeight: appearanceContent.implicitHeight + 8
                color: settings.panel
                radius: 6
                ColumnLayout {
                    id: appearanceContent
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.topMargin: 4
                    anchors.bottomMargin: 4
                    spacing: 3
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 25
                        Label { text: "外观"; color: settings.textColor; font.bold: true }
                        Item { Layout.fillWidth: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 25
                        Label { text: "主题颜色"; color: settings.textColor; Layout.fillWidth: true }
                        Components.NeutralButton {
                            controller: settings.controller
                            id: themeColor
                            text: settings.controller.themeColor
                            enabled: !autoTheme.checked
                            Layout.preferredWidth: 78
                            onClicked: colorPopup.openForColor(themeColor.text)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 25
                        Label { text: "透明度 " + Math.round(opacitySlider.value) + "%"; color: settings.textColor; Layout.preferredWidth: 100 }
                        Basic.Slider {
                            id: opacitySlider
                            from: 40
                            to: 95
                            stepSize: 1
                            focusPolicy: Qt.NoFocus
                            Layout.fillWidth: true
                            onMoved: settings.controller.previewAppearance(autoTheme.checked, themeColor.text, Math.round(value))
                            onPressedChanged: if (!pressed && !settings.initializing) settings.applySettings()
                            background: Rectangle {
                                x: opacitySlider.leftPadding
                                y: opacitySlider.topPadding + (opacitySlider.availableHeight - height) / 2
                                implicitWidth: 120
                                implicitHeight: 4
                                width: opacitySlider.availableWidth
                                height: implicitHeight
                                radius: height / 2
                                color: settings.controller.paletteLine
                                Rectangle {
                                    width: opacitySlider.visualPosition * parent.width
                                    height: parent.height
                                    radius: parent.radius
                                    color: settings.textColor
                                }
                            }
                            handle: Rectangle {
                                x: opacitySlider.leftPadding + opacitySlider.visualPosition * (opacitySlider.availableWidth - width)
                                y: opacitySlider.topPadding + (opacitySlider.availableHeight - height) / 2
                                implicitWidth: 16
                                implicitHeight: 16
                                radius: width / 2
                                color: settings.bg
                                border.width: 2
                                border.color: settings.textColor
                            }
                        }
                    }
                    CompactCheckBox {
                        Layout.preferredHeight: 25
                        id: autoTheme
                        text: "根据组件下方背景自动改变主题色"
                        onToggled: settings.applySettings()
                    }
                }
            }

            Rectangle {
                id: generalGroup
                Layout.fillWidth: true
                Layout.preferredHeight: generalContent.implicitHeight + 8
                color: settings.panel
                radius: 6
                ColumnLayout {
                    id: generalContent
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.topMargin: 4
                    anchors.bottomMargin: 4
                    spacing: 3
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 25
                        Label { text: "常规"; color: settings.textColor; font.bold: true }
                        Item { Layout.fillWidth: true }
                    }
                    CompactCheckBox { Layout.preferredHeight: 25; id: paused; text: "暂停行情刷新"; onToggled: settings.applySettings() }
                    CompactCheckBox { Layout.preferredHeight: 25; id: floating; text: "使用可自由移动的迷你浮窗"; onToggled: settings.applySettings() }
                    CompactCheckBox { Layout.preferredHeight: 25; id: autostart; text: "Windows 开机自动启动"; onToggled: settings.applySettings() }
                    CompactCheckBox { Layout.preferredHeight: 25; id: tray; text: "关闭时最小化到系统托盘"; onToggled: settings.applySettings() }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                radius: 6
                color: settings.panel
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    Label { text: "快捷键"; color: settings.textColor }
                    Item { Layout.fillWidth: true }
                    Label { text: "Alt + Q"; color: settings.muted }
                }
            }

            Rectangle {
                id: aboutGroup
                Layout.fillWidth: true
                Layout.preferredHeight: aboutContent.implicitHeight + 8
                color: settings.panel
                radius: 6
                ColumnLayout {
                    id: aboutContent
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.topMargin: 4
                    anchors.bottomMargin: 4
                    spacing: 3
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 25
                        Label { id: aboutLabel; text: "关于"; color: settings.textColor; font.bold: true }
                        Label { text: "v" + Qt.application.version; color: settings.controller.paletteHint; font.pixelSize: aboutLabel.font.pixelSize - 2; Layout.leftMargin: 6 }
                        Item { Layout.fillWidth: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 25
                        Label { text: "开发者"; color: settings.muted; Layout.fillWidth: true }
                        Label { text: "dmqx"; color: settings.textColor; Layout.preferredWidth: emailMetrics.width; horizontalAlignment: Text.AlignLeft }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 25
                        Label { text: "联系邮箱"; color: settings.muted; Layout.fillWidth: true }
                        Label { text: "dmqx@foxmail.com"; color: settings.textColor; Layout.preferredWidth: emailMetrics.width; horizontalAlignment: Text.AlignLeft }
                    }
                }
            }

    }

    Popup {
        id: colorPopup
        objectName: "colorPopup"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 250
        height: 178
        modal: true
        dim: true
        padding: 12
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        property var presets: ["#FFFFFF", "#000000", "#F1E4D2", "#E7DDF1", "#DDE5F4", "#CFE8E8", "#E5E5E5", "#D8DEE8"]

        function isSelected(value) {
            return colorField.text.toUpperCase() === value
        }

        function openForColor(value) {
            colorField.text = value
            open()
            colorField.forceActiveFocus()
        }

        background: Rectangle {
            color: settings.bg
            radius: 8
            border.width: 1
            border.color: settings.controller.paletteLine
        }

        contentItem: ColumnLayout {
            spacing: 8
            Label { text: "选择主题颜色"; color: settings.textColor; font.bold: true }
            GridLayout {
                columns: 8
                columnSpacing: 5
                rowSpacing: 5
                Repeater {
                    model: colorPopup.presets
                    delegate: Basic.Button {
                        id: presetButton
                        required property string modelData
                        implicitWidth: 23
                        implicitHeight: 23
                        padding: 0
                        focusPolicy: Qt.NoFocus
                        scale: presetButton.down ? 0.94
                              : (colorPopup.isSelected(presetButton.modelData) ? 1.1
                                 : (presetButton.hovered ? 1.06 : 1))
                        background: Rectangle {
                            color: "transparent"
                            radius: 6
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 1
                                radius: 4
                                color: presetButton.modelData
                                border.width: 1
                                border.color: presetButton.modelData === "#FFFFFF"
                                              ? settings.muted
                                              : settings.controller.paletteLine
                            }
                        }
                        onClicked: colorField.text = presetButton.modelData
                        Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
                    }
                }
            }
            Basic.TextField {
                id: colorField
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                placeholderText: "#RRGGBB"
                placeholderTextColor: settings.muted
                color: settings.textColor
                selectByMouse: true
                leftPadding: 38
                rightPadding: 10
                verticalAlignment: TextInput.AlignVCenter
                selectionColor: settings.muted
                selectedTextColor: settings.bg
                maximumLength: 7
                validator: RegularExpressionValidator { regularExpression: /^#[0-9A-Fa-f]{6}$/ }
                background: Rectangle {
                    color: settings.panel
                    radius: 7
                    border.width: 1
                    border.color: colorField.activeFocus ? settings.muted : settings.controller.paletteLine
                    Rectangle {
                        x: 8
                        anchors.verticalCenter: parent.verticalCenter
                        width: 20
                        height: 20
                        radius: 4
                        color: colorField.acceptableInput ? colorField.text : "transparent"
                        border.width: 1
                        border.color: settings.muted
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Components.NeutralButton { controller: settings.controller; text: "取消"; onClicked: colorPopup.close() }
                Components.NeutralButton {
                    controller: settings.controller
                    text: "应用"
                    prominent: true
                    enabled: colorField.acceptableInput
                    onClicked: {
                        themeColor.text = colorField.text.toUpperCase()
                        settings.controller.previewAppearance(autoTheme.checked, themeColor.text, Math.round(opacitySlider.value))
                        settings.applySettings()
                        colorPopup.close()
                    }
                }
            }
        }
    }

    Shortcut { sequences: [StandardKey.Cancel]; onActivated: settings.closeSettings() }
}
