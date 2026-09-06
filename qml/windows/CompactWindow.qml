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
        Layout.leftMargin: 4
        Layout.rightMargin: 4
        Layout.bottomMargin: 4
        clip: true
        spacing: 6
        model: view.controller.positions
        ScrollBar.vertical: ScrollBar { width: 7; policy: ScrollBar.AsNeeded }
        delegate: Components.CompactCard {
            id: card
            controller: view.controller
            x: Math.max(0, (list.width - width) / 2)
            width: list.width - 8 - (list.contentHeight > list.height ? 8 : 0)
        }
        Label {
            anchors.centerIn: parent
            visible: view.controller.count === 0
            text: "暂无自选"
            color: view.controller.paletteMuted
            font.pixelSize: 10
        }
    }
}