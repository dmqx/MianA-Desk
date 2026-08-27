pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as Components

ColumnLayout {
    id: view
    required property var controller
    required property var mainWindow
    spacing: 0

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: 40
        DragHandler { target: null; enabled: !view.controller.locked; onActiveChanged: if (active) view.mainWindow.startSystemMove() }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 11
            anchors.rightMargin: 7
            spacing: 5
            Image { source: view.controller.iconUrl; fillMode: Image.PreserveAspectFit; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            Label { text: "●"; color: view.controller.statusColor; font.family: "Arial"; font.pixelSize: 8 }
            Label { text: view.controller.statusText; color: view.controller.paletteMuted; font.pixelSize: 9; elide: Text.ElideRight; Layout.fillWidth: true }
            Components.FlatButton { controller: view.controller; text: view.controller.locked ? "锁" : "移"; onClicked: view.controller.toggle("locked") }
            Components.FlatButton { controller: view.controller; text: view.controller.topmost ? "顶" : "层"; onClicked: view.controller.toggle("topmost") }
            Components.FlatButton { controller: view.controller; text: "展"; onClicked: view.controller.toggle("compact") }
            Components.FlatButton { controller: view.controller; text: "浮"; onClicked: view.controller.toggle("floating") }
            Components.FlatButton { controller: view.controller; text: "×"; font.pixelSize: 14; onClicked: view.mainWindow.close() }
        }
    }
    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: view.controller.positions
        ScrollBar.vertical: ScrollBar { width: 7; policy: list.contentHeight > list.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }
        delegate: Rectangle {
            id: row
            required property string positionId
            required property string symbol
            required property string priceText
            required property bool hasError
            width: list.width - (list.contentHeight > list.height ? 7 : 0)
            height: 34
            color: mouse.containsMouse ? view.controller.paletteHover : view.controller.palettePanel
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16
                Label { text: row.symbol; color: row.hasError ? "#E88B00" : view.controller.paletteMuted; font.pixelSize: 10; Layout.fillWidth: true }
                Label { text: row.priceText; color: row.hasError ? "#E88B00" : view.controller.paletteMuted; font.pixelSize: 10 }
            }
            MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: view.controller.setFocus(row.positionId) }
        }
        Label { anchors.centerIn: parent; visible: view.controller.count === 0; text: "暂无自选"; color: view.controller.paletteMuted; font.pixelSize: 10 }
    }
}
