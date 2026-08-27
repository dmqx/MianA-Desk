pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "../components" as Components

ColumnLayout {
    id: view
    required property var controller
    required property var mainWindow
    signal addRequested()
    signal editRequested(string positionId, string symbol, string name)
    signal deleteRequested(string positionId)
    spacing: 0

    TextMetrics {
        id: contextMenuTextMetrics
        text: "编辑自选"
        font.family: "Microsoft YaHei UI"
        font.pointSize: 9
    }

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: 40
        DragHandler { target: null; enabled: !view.controller.locked; onActiveChanged: if (active) view.mainWindow.startSystemMove() }
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 11; anchors.rightMargin: 7; spacing: 5
            Image { source: view.controller.iconUrl; fillMode: Image.PreserveAspectFit; smooth: true; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            Label { text: "●"; color: view.controller.statusColor; font.family: "Arial"; font.pixelSize: 8 }
            Label { text: view.controller.statusText; color: view.controller.paletteMuted; font.family: "Microsoft YaHei UI"; font.pixelSize: 9; elide: Text.ElideRight; Layout.fillWidth: true }
            Components.FlatButton { controller: view.controller; text: view.controller.locked ? "锁" : "移"; onClicked: view.controller.toggle("locked") }
            Components.FlatButton { controller: view.controller; text: view.controller.topmost ? "顶" : "层"; onClicked: view.controller.toggle("topmost") }
            Components.FlatButton { controller: view.controller; text: "折"; onClicked: view.controller.toggle("compact") }
            Components.FlatButton { controller: view.controller; text: "浮"; onClicked: view.controller.toggle("floating") }
            Components.FlatButton { controller: view.controller; text: "×"; font.pixelSize: 14; onClicked: view.mainWindow.close() }
        }
    }
    Item {
        Layout.fillWidth: true; Layout.preferredHeight: 37
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 11; anchors.rightMargin: 9
            Label { text: "自选"; color: view.controller.paletteMuted; font.pixelSize: 12; font.bold: true; Layout.alignment: Qt.AlignVCenter }
            Label { text: view.controller.count; color: view.controller.paletteHint; font.pixelSize: 12; Layout.alignment: Qt.AlignVCenter }
            Item { Layout.fillWidth: true }
            Components.FlatButton { controller: view.controller; text: "↻"; font.family: "Segoe UI Symbol"; font.pixelSize: 10; font.bold: true; Layout.alignment: Qt.AlignVCenter; onClicked: view.controller.manualRefresh() }
            Components.FlatButton { controller: view.controller; text: "＋ 添加"; font.pixelSize: 10; font.bold: true; Layout.alignment: Qt.AlignVCenter; onClicked: view.addRequested() }
        }
    }
    ListView {
        id: list
        property int dragSource: -1
        property int dragDestination: -1
        property real dragPointerY: -1
        property real dragGrabOffset: 0
        property string dragPositionId: ""
        property string dragName: ""
        property string dragSymbol: ""
        property string dragPrice: ""
        property bool dragHasError: false

        function updateDragTarget(viewportY) {
            if (dragSource < 0 || view.controller.count <= 0)
                return
            const rowExtent = 55 + spacing
            const contentPointY = contentY + Math.max(0, Math.min(height - 1, viewportY))
            dragDestination = Math.max(0, Math.min(view.controller.count - 1,
                                                   Math.floor(contentPointY / rowExtent)))
        }
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 4
        Layout.rightMargin: 4
        Layout.bottomMargin: 4
        clip: true
        spacing: 6
        model: view.controller.positions
        moveDisplaced: Transition {
            NumberAnimation { properties: "x,y"; duration: 180; easing.type: Easing.OutCubic }
        }
        ScrollBar.vertical: ScrollBar { width: 7; policy: list.contentHeight > list.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }

        Timer {
            interval: 16
            repeat: true
            running: list.dragSource >= 0 && list.dragPointerY >= 0
            onTriggered: {
                const edge = 34
                let delta = 0
                if (list.dragPointerY < edge)
                    delta = -Math.max(2, (edge - list.dragPointerY) * 0.22)
                else if (list.dragPointerY > list.height - edge)
                    delta = Math.max(2, (list.dragPointerY - list.height + edge) * 0.22)
                if (delta === 0)
                    return
                const maximum = Math.max(0, list.contentHeight - list.height)
                list.contentY = Math.max(0, Math.min(maximum, list.contentY + delta))
                list.updateDragTarget(list.dragPointerY)
            }
        }
        delegate: Components.PositionCard {
            id: card
            controller: view.controller
            x: 4
            width: list.width - 8 - (list.contentHeight > list.height ? 8 : 0)
            opacity: list.dragSource === index ? 0 : 1
            reorderOffsetY: {
                if (list.dragSource < 0 || index === list.dragSource)
                    return 0
                const extent = card.height + list.spacing
                if (list.dragDestination > list.dragSource
                        && index > list.dragSource && index <= list.dragDestination)
                    return -extent
                if (list.dragDestination < list.dragSource
                        && index >= list.dragDestination && index < list.dragSource)
                    return extent
                return 0
            }
            Behavior on opacity { NumberAnimation { duration: 120 } }
            Behavior on reorderOffsetY {
                NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
            }
            onSelected: view.controller.setFocus(card.positionId)
            onContextRequested: function(localX, localY) {
                const point = card.mapToGlobal(localX, localY)
                positionMenu.positionId = card.positionId
                positionMenu.positionSymbol = card.symbol
                positionMenu.positionName = card.name
                const menuX = Math.round(Math.max(Screen.virtualX + 2,
                    Math.min(point.x + 2, Screen.virtualX + Screen.desktopAvailableWidth - positionMenu.width - 2)))
                const menuY = Math.round(Math.max(Screen.virtualY + 2,
                    Math.min(point.y + 2, Screen.virtualY + Screen.desktopAvailableHeight - positionMenu.height - 2)))
                positionMenu.openAt(menuX, menuY)
            }
            onLongPressed: function(localY) {
                list.dragSource = card.index
                list.dragDestination = card.index
                list.dragGrabOffset = localY
                list.dragPointerY = card.mapToItem(list, card.width / 2, localY).y
                list.dragPositionId = card.positionId
                list.dragName = card.name
                list.dragSymbol = card.symbol
                list.dragPrice = card.priceText
                list.dragHasError = card.hasError
            }
            onDragMoved: function(localY) {
                const point = card.mapToItem(list, card.width / 2, localY)
                list.dragPointerY = point.y
                list.updateDragTarget(point.y)
            }
            onDragFinished: function(commit) {
                const source = list.dragSource
                const destination = list.dragDestination
                list.dragSource = -1
                list.dragDestination = -1
                list.dragPointerY = -1
                list.dragGrabOffset = 0
                if (commit && source >= 0 && destination >= 0 && source !== destination)
                    Qt.callLater(function() { view.controller.movePosition(source, destination) })
            }
        }
        Components.PositionCard {
            parent: list
            visible: list.dragSource >= 0
            enabled: false
            z: 100
            x: 4
            y: Math.max(1, Math.min(list.height - height - 1,
                                   list.dragPointerY - list.dragGrabOffset))
            width: list.width - 8 - (list.contentHeight > list.height ? 8 : 0)
            controller: view.controller
            index: -1
            positionId: list.dragPositionId
            symbol: list.dragSymbol
            name: list.dragName
            priceText: list.dragPrice
            hasError: list.dragHasError
            reorderActive: true
            opacity: visible ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        }
        Label {
            anchors.centerIn: parent
            visible: view.controller.count === 0
            text: "还没有自选\n点“添加”开始查看实时价格"
            horizontalAlignment: Text.AlignHCenter
            color: view.controller.paletteMuted
            font.pixelSize: 11
        }
        MouseArea { anchors.fill: parent; visible: view.controller.count === 0; onClicked: view.addRequested() }
    }

    property Window positionMenuWindow: Window {
        id: positionMenu
        objectName: "positionMenu"
        property string positionId: ""
        property string positionSymbol: ""
        property string positionName: ""
        property int requestedX: 0
        property int requestedY: 0
        visible: false
        // A Window is stored as an object property, but qmllint still treats it
        // as a ColumnLayout child. Its native size must not use Layout attached properties.
        // qmllint disable Quick.layout-positioning
        width: Math.ceil(contextMenuTextMetrics.width) + 18
        height: 62
        // qmllint enable Quick.layout-positioning
        color: view.controller.palettePanel
        flags: Qt.FramelessWindowHint | Qt.Tool | Qt.NoDropShadowWindowHint
        modality: Qt.NonModal
        transientParent: view.mainWindow
        onActiveChanged: if (visible && !active) hide()

        function placeAtRequestedPosition() {
            if (!visible)
                return
            x = requestedX
            y = requestedY
            raise()
            requestActivate()
        }
        function openAt(globalX, globalY) {
            requestedX = globalX
            requestedY = globalY
            Qt.callLater(function() {
                show()
                placeAtRequestedPosition()
                Qt.callLater(placeAtRequestedPosition)
            })
        }

        Rectangle {
            anchors.fill: parent
            color: view.controller.palettePanel
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 2
            spacing: 0
            Basic.Button {
                id: editPositionItem
                text: "编辑自选"
                focusPolicy: Qt.NoFocus
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                leftPadding: 6
                rightPadding: 4
                font.family: "Microsoft YaHei UI"
                font.pointSize: 9
                contentItem: Text {
                    text: editPositionItem.text
                    color: view.controller.paletteText
                    font: editPositionItem.font
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: editPositionItem.hovered ? view.controller.paletteHover : "transparent"
                    radius: 4
                }
                onClicked: {
                    positionMenu.hide()
                    view.editRequested(positionMenu.positionId, positionMenu.positionSymbol, positionMenu.positionName)
                }
            }
            Item { Layout.fillWidth: true; Layout.preferredHeight: 2 }
            Basic.Button {
                id: deletePositionItem
                text: "删除自选"
                focusPolicy: Qt.NoFocus
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                leftPadding: 6
                rightPadding: 4
                font.family: "Microsoft YaHei UI"
                font.pointSize: 9
                contentItem: Text {
                    text: deletePositionItem.text
                    color: view.controller.paletteText
                    font: deletePositionItem.font
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: deletePositionItem.hovered ? view.controller.paletteHover : "transparent"
                    radius: 4
                }
                onClicked: {
                    positionMenu.hide()
                    view.deleteRequested(positionMenu.positionId)
                }
            }
        }
        Shortcut { sequences: [StandardKey.Cancel]; onActivated: positionMenu.hide() }
    }
}
