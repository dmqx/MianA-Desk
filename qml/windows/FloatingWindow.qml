import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as Components

Item {
    id: view
    required property var controller
    required property var mainWindow

    // Keep the floating strip under the pointer all the way to the screen
    // edge. startSystemMove() invokes Windows edge snapping, which can move a
    // fixed-size tool window back inward when the drag is released.
    MouseArea {
        anchors.fill: parent
        enabled: !view.controller.locked
        acceptedButtons: Qt.LeftButton
        property real pressedX: 0
        property real pressedY: 0
        onPressed: function(mouse) {
            pressedX = mouse.x
            pressedY = mouse.y
        }
        onPositionChanged: function(mouse) {
            if (pressed) {
                view.mainWindow.x = Math.round(view.mainWindow.x + mouse.x - pressedX)
                view.mainWindow.y = Math.round(view.mainWindow.y + mouse.y - pressedY)
            }
        }
    }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 13
        anchors.rightMargin: 3
        spacing: 3
        Label { text: view.controller.focusedSymbol; color: view.controller.paletteMuted; font.family: "Segoe UI"; font.pixelSize: 10 }
        Label { text: view.controller.focusedPrice; color: view.controller.paletteMuted; font.family: "Segoe UI"; font.pixelSize: 10; Layout.leftMargin: 6 }
        Item { Layout.fillWidth: true }
        Components.FlatButton { controller: view.controller; text: "›"; onClicked: view.controller.cycleFocus() }
        Components.FlatButton { controller: view.controller; text: "展"; onClicked: view.controller.toggle("floating") }
    }
}
