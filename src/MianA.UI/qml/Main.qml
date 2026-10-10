pragma ComponentBehavior: Bound
// Loader.item is a dynamically created SettingsWindow, although qmllint sees QObject.
// qmllint disable missing-property

import QtQuick
import QtQuick.Controls
import "Views" as Views
import "Controls" as Components

ApplicationWindow {
    id: root
    required property AppController appController
    property bool floatingMode: appController.floating
    property bool hideBorderShadow: appController.hideBorderShadow
    property bool lockedMode: appController.locked
    readonly property bool autoThemeEnabled: appController.autoTheme
    visible: true
    title: "MianA Desk · 股票桌面小组件"
    color: "transparent"
    property string panelMode: ""
    readonly property bool actionsOpen: panelMode === "menu"
    property string depthPositionId: ""
    readonly property bool panelOpen: panelMode.length > 0
    readonly property int panelWidth: panelMode === "depth" ? depthPanel.implicitWidth : actionsPanel.implicitWidth
    property bool actionsOnLeft: false
    property bool changingPanel: false
    readonly property int leftInset: panelOpen && actionsOnLeft ? panelWidth : 0
    readonly property int pageWidth: appController.floating ? 74 : 288 + (panelOpen ? panelWidth : 0)
    readonly property int pageHeight: appController.floating ? 34 : 136
    flags: Qt.FramelessWindowHint
        | (appController.topmost ? Qt.Window
           : ((appController.floating || appController.locked) ? Qt.Tool : Qt.Window))
        | (appController.hideBorderShadow ? Qt.NoDropShadowWindowHint : 0)
        | (appController.topmost ? Qt.WindowStaysOnTopHint : 0)
    x: appController.savedX - leftInset
    y: appController.savedY
    width: pageWidth
    height: pageHeight
    minimumWidth: pageWidth
    maximumWidth: pageWidth
    minimumHeight: pageHeight
    maximumHeight: pageHeight

    function setPanel(mode, positionId) {
        const stockX = x + leftInset
        changingPanel = true
        depthPositionId = mode === "depth" ? positionId : ""
        panelMode = mode
        const right = Screen.virtualX + Screen.desktopAvailableWidth
        actionsOnLeft = panelOpen && stockX + 288 + panelWidth > right
            && stockX - panelWidth >= Screen.virtualX
        x = stockX - leftInset
        changingPanel = false
        updateDepth()
    }
    function toggleActions() { setPanel(actionsOpen ? "" : "menu", "") }
    function toggleDepth(positionId) {
        setPanel(panelMode === "depth" && depthPositionId === positionId ? "" : "depth", positionId)
    }
    function updateDepth() {
        if (panelMode !== "depth") return
        const book = appController.orderBook(depthPositionId)
        if (!book.name) { setPanel("", ""); return }
        depthPanel.book = book
    }

    function ensureVisible() {
        const left = Screen.virtualX
        const top = Screen.virtualY
        const right = left + (appController.floating ? Screen.width : Screen.desktopAvailableWidth)
        const bottom = top + (appController.floating ? Screen.height : Screen.desktopAvailableHeight)
        if (x + width <= left || x >= right || y + 40 <= top || y >= bottom) {
            x = Math.round(Math.max(left, Math.min(x, right - width)))
            y = Math.round(Math.max(top, Math.min(y, bottom - height)))
        }
    }
    function showActive() { show(); raise(); requestActivate(); Qt.callLater(ensureVisible) }
    function openSettings() {
        if (settingsLoader.item) settingsLoader.item["openForSettings"]()
        else settingsLoader.active = true
    }

    onXChanged: {
        if (!changingPanel) appController.setWindowPosition(Math.round(x + leftInset), Math.round(y))
        if (visible && appController.autoTheme) themeSampleTimer.restart()
    }
    onYChanged: {
        if (!changingPanel) appController.setWindowPosition(Math.round(x + leftInset), Math.round(y))
        if (visible && appController.autoTheme) themeSampleTimer.restart()
    }
    onVisibleChanged: {
        if (visible && appController.autoTheme) themeSampleTimer.restart()
    }
    onFloatingModeChanged: setPanel("", "")
    onAutoThemeEnabledChanged: if (visible && autoThemeEnabled) themeSampleTimer.restart()
    onWidthChanged: if (visible) Qt.callLater(ensureVisible)
    onHeightChanged: if (visible) Qt.callLater(ensureVisible)
    onClosing: function(close) {
        if (appController.minimizeToTray) { close.accepted = false; hide() }
        else appController.saveAndShutdown()
    }
    Component.onCompleted: {
        Qt.callLater(ensureVisible)
        if (appController.autoTheme) themeSampleTimer.restart()
    }

    Connections {
        target: root.appController
        function onShowMainRequested() {
            if (settingsLoader.item) settingsLoader.item["closeSettings"]()
            root.showActive()
        }
        function onToggleMainRequested() { root.visible ? root.hide() : root.showActive() }
        function onSettingsRequested() { root.openSettings() }
        function onFocusChanged() { root.updateDepth() }
        function onStateChanged() { root.updateDepth() }
    }

    Timer {
        id: themeSampleTimer
        interval: 100
        onTriggered: root.appController.sampleTheme(root.x, root.y, root.width, root.height)
    }

    background: Rectangle {
        color: root.appController.floating ? root.appController.palettePanel : root.appController.paletteBg
        radius: root.appController.floating ? root.height / 2
            : (root.appController.hideBorderShadow ? 8 : 0)
    }

    Loader {
        id: modeLoader
        x: root.leftInset
        width: root.appController.floating ? 74 : 288
        height: root.height
        sourceComponent: root.appController.floating ? floatingComponent
            : compactComponent
    }
    Components.ActionPanel {
        id: actionsPanel
        controller: root.appController
        visible: root.actionsOpen && !root.appController.floating
        x: root.actionsOnLeft ? 0 : 288
        width: implicitWidth
        height: root.height
        onManageRequested: watchlist.openForManagement()
    }
    Components.DepthPanel {
        id: depthPanel
        controller: root.appController
        visible: root.panelMode === "depth" && !root.appController.floating
        x: root.actionsOnLeft ? 0 : 288
        width: implicitWidth
        height: root.height
    }
    Rectangle {
        visible: root.panelOpen
        x: root.actionsOnLeft ? root.panelWidth : 288
        y: 4
        width: 1
        height: root.height - 8
        color: root.appController.paletteLine
        opacity: root.appController.autoTheme ? 0.2 : 1
    }
    Component {
        id: compactComponent
        Views.CompactWindow {
            controller: root.appController
            mainWindow: root
            onActionsRequested: root.toggleActions()
            onDepthRequested: function(positionId) { root.toggleDepth(positionId) }
        }
    }
    Component {
        id: floatingComponent
        Views.FloatingWindow { controller: root.appController; mainWindow: root }
    }

    Views.WatchlistWindow { id: watchlist; controller: root.appController; mainWindow: root }

    Component {
        id: settingsComponent
        Views.SettingsWindow { controller: root.appController; mainWindow: root }
    }
    Loader {
        id: settingsLoader
        active: false
        sourceComponent: settingsComponent
        onLoaded: item["openForSettings"]()
    }
}
