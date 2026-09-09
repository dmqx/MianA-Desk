import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: view
    required property AppController controller
    required property Window mainWindow

    // Keep the floating strip under the pointer all the way to the screen
    // edge. startSystemMove() invokes Windows edge snapping, which can move a
    // fixed-size tool window back inward when the drag is released.
    MouseArea {
        anchors.fill: parent
        enabled: !view.controller.locked
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        property real pressedX: 0
        property real pressedY: 0
        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton) {
                pressedX = mouse.x
                pressedY = mouse.y
            }
        }
        onDoubleClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                view.mainWindow.close()
            } else if (mouse.x >= quote.x && mouse.x <= quote.x + quote.width) {
                view.controller.toggle("floating")
            }
        }
        onPositionChanged: function(mouse) {
            if (pressedButtons & Qt.LeftButton) {
                view.mainWindow.x = Math.round(view.mainWindow.x + mouse.x - pressedX)
                view.mainWindow.y = Math.round(view.mainWindow.y + mouse.y - pressedY)
            }
        }
    }
    ColumnLayout {
        id: quote
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0
        Label {
            text: view.controller.focusedPrice
            color: view.controller.paletteMuted
            font.family: "Segoe UI"
            font.pixelSize: 13
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: view.controller.hideStockCode ? view.controller.focusedName : view.controller.focusedSymbol.replace(/\.[A-Za-z]+$/, "")
            color: view.controller.paletteMuted
            font.family: "Segoe UI"
            font.pixelSize: view.controller.hideStockCode ? 7 : 8
            font.bold: view.controller.hideStockCode
            Layout.alignment: Qt.AlignHCenter
        }
    }
    Text {
        id: nextSymbol
        opacity: nextSymbolMouse.containsMouse ? 1 : 0
        text: "›"
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.verticalCenter: parent.verticalCenter
        color: view.controller.paletteMuted
        font.family: "Segoe UI Symbol"
        font.pixelSize: 13
        width: 10
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter

        anchors.verticalCenterOffset: -1.5
        MouseArea {
            anchors.fill: parent
            id: nextSymbolMouse
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: view.controller.cycleFocus()
        }
    }
}
