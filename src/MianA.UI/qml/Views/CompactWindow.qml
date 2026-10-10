pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Controls" as Components

ColumnLayout {
    id: view
    required property AppController controller
    required property Window mainWindow
    signal actionsRequested()
    signal depthRequested(string positionId)
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
            Components.AppButton {
                id: menuButton
                objectName: "actionsButton"
                controller: view.controller
                text: "⋯"
                variant: Components.AppButton.Title
                Accessible.name: "操作菜单"
                onClicked: view.actionsRequested()
            }
            Components.AppButton { controller: view.controller; text: "×"; font.pixelSize: 14; onClicked: view.mainWindow.close() }
        }
    }
    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 8
        Layout.rightMargin: 8
        Layout.bottomMargin: 4
        ListView {
            id: list
            objectName: "positionsList"
            property int dragSource: -1
            property int dragDestination: -1
            property real dragPointerY: -1
            property real dragGrabOffset: 0
            property string dragPositionId: ""
            property string dragName: ""
            property string dragSymbol: ""
            property string dragPrice: ""
            property bool dragHasError: false
            property double dragChangePercent: 0.0
            property double dragPreviousClose: 0.0
            property bool dragHasChange: false
            property var dragIntraday: []

            function updateDragTarget(viewportY) {
                if (dragSource < 0 || view.controller.count <= 0)
                    return
                const rowExtent = 40 + spacing
                const contentPointY = contentY + Math.max(0, Math.min(height - 1, viewportY))
                dragDestination = Math.max(0, Math.min(view.controller.count - 1,
                                                       Math.floor(contentPointY / rowExtent)))
            }
            anchors.fill: parent
            clip: true
            spacing: 6
            model: view.controller.positions
            moveDisplaced: Transition {
                NumberAnimation { properties: "x,y"; duration: 180; easing.type: Easing.OutCubic }
            }

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
            delegate: Components.CompactCard {
                id: card
                controller: view.controller
                x: 0
                width: list.width
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
                Behavior on reorderOffsetY {
                    NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
                }
                onDepthRequested: function(positionId) { view.depthRequested(positionId) }
                onLongPressed: function(localY) {
                    list.dragDestination = card.index
                    list.dragGrabOffset = localY
                    list.dragPointerY = card.mapToItem(list, card.width / 2, localY).y
                    list.dragPositionId = card.positionId
                    list.dragName = card.name
                    list.dragSymbol = card.symbol
                    list.dragPrice = card.priceText
                    list.dragHasError = card.hasError
                    list.dragChangePercent = card.changePercent
                    list.dragPreviousClose = card.previousClose
                    list.dragHasChange = card.hasChange
                    list.dragIntraday = card.intraday
                    list.dragSource = card.index
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
            Components.CompactCard {
                parent: list
                visible: list.dragSource >= 0
                enabled: false
                z: 100
                x: 0
                y: Math.max(1, Math.min(list.height - height - 1,
                                       list.dragPointerY - list.dragGrabOffset))
                width: list.width
                controller: view.controller
                index: -1
                positionId: list.dragPositionId
                symbol: list.dragSymbol
                name: list.dragName
                priceText: list.dragPrice
                hasError: list.dragHasError
                changePercent: list.dragChangePercent
                previousClose: list.dragPreviousClose
                hasChange: list.dragHasChange
                intraday: list.dragIntraday
                reorderActive: true
                opacity: visible ? 1 : 0
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

}
