pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: panel
    objectName: "depthPanel"
    required property AppController controller
    property var book: ({})
    readonly property color priceColor: !book.hasChange ? controller.paletteMuted
        : book.changePercent > 0 ? controller.paletteUp
        : book.changePercent < 0 ? controller.paletteDown : controller.paletteMuted
    implicitWidth: 114
    implicitHeight: 136
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.topMargin: 2
        anchors.bottomMargin: 2
        spacing: 0
        Label {
            objectName: "depthTitle"
            text: "五档盘口"
            Layout.fillWidth: true
            Layout.preferredHeight: 10
            Layout.maximumHeight: 10
            Layout.bottomMargin: 3
            font.family: "Microsoft YaHei UI"
            font.pixelSize: 8
            color: panel.controller.paletteMuted
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Column {
                anchors.fill: parent
                visible: (panel.book.rows || []).length > 0
                Repeater {
                    model: panel.book.rows || []
                    delegate: Item {
                        id: row
                        required property var modelData
                        required property int index
                        width: parent.width
                        objectName: "depthRow" + index
                        readonly property real lineHeight: (parent.height - 7) / 10
                        height: lineHeight + (index === 4 ? 7 : 0)
                        RowLayout {
                            width: parent.width
                            height: row.lineHeight
                            spacing: 4
                            Label { text: row.modelData.label; font.family: "Microsoft YaHei UI"; font.pixelSize: 9; color: panel.controller.paletteMuted; Layout.preferredWidth: 16 }
                            Label { objectName: "depthPrice"; text: row.modelData.price; font.family: "Microsoft YaHei UI"; font.pixelSize: 9; color: panel.priceColor; horizontalAlignment: Text.AlignRight; Layout.fillWidth: true }
                            Label { objectName: "depthVolume"; text: Number(row.modelData.volume) >= 10000 ? (Number(row.modelData.volume) / 10000).toFixed(1) + "万" : row.modelData.volume; font.family: "Microsoft YaHei UI"; font.pixelSize: 9; color: panel.controller.paletteMuted; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft; Layout.preferredWidth: 34 }
                        }
                        Item {
                            objectName: "depthDivider"
                            visible: row.index === 4
                            y: row.lineHeight
                            width: parent.width
                            height: 7
                            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 1; color: panel.controller.paletteLine; opacity: panel.controller.autoTheme ? 0.2 : 1 }
                        }
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: (panel.book.rows || []).length === 0
                text: panel.book.message || "暂无五档数据"
                font.family: "Microsoft YaHei UI"
                font.pixelSize: 9
                color: panel.controller.paletteMuted
            }
        }
    }
}
