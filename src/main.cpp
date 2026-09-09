#include "MianA.UI/AppController.h"
#include "MianA.Windows/WindowManager.h"

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QVariant>
#include <QWindow>

#if defined(Q_OS_WIN) && defined(_MSC_VER)
// Activate the version 6 Windows common-controls theme. The legacy PySide
// build carried this manifest dependency, which affects native QMenu metrics,
// colors, borders, and text rendering.
#pragma comment(                                                               \
    linker,                                                                    \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' processorArchitecture='*' "                            \
    "publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

int main(int argc, char *argv[]) {
  QQuickStyle::setStyle(QStringLiteral("Windows"));
  QApplication app(argc, argv);

  QCoreApplication::setApplicationName(QStringLiteral("MianA Desk"));
  QCoreApplication::setOrganizationName(QStringLiteral("MianA"));
  QCoreApplication::setApplicationVersion(QStringLiteral(MIANA_VERSION_STRING));
  QApplication::setQuitOnLastWindowClosed(false);
  const QIcon icon(QStringLiteral(":/qt/qml/MianA/assets/miana_desk.ico"));
  app.setWindowIcon(icon);

  WindowManager windows;
  if (!windows.acquireSingleInstance())
    return 0;

  AppController controller(windows);
  QObject::connect(&controller, &AppController::shutdownRequested, &app,
                   &QCoreApplication::quit);
  QObject::connect(&windows, &WindowManager::toggleFloatingRequested,
                   &controller, &AppController::toggleFloatingAndShow);
  QObject::connect(&windows, &WindowManager::showFullRequested, &controller,
                   &AppController::showFull);
  QObject::connect(&windows, &WindowManager::togglePausedRequested,
                   &controller, [&controller] {
                     controller.toggle(QStringLiteral("paused"));
                   });
  QObject::connect(&windows, &WindowManager::autostartRequested, &controller,
                   &AppController::setAutostart);
  QObject::connect(&windows, &WindowManager::settingsRequested, &controller,
                   &AppController::settingsRequested);
  QObject::connect(&windows, &WindowManager::checkUpdatesRequested,
                   &controller, [&controller] {
                     controller.checkForUpdates(true);
                   });
  QObject::connect(&windows, &WindowManager::quitRequested, &controller,
                   &AppController::saveAndShutdown);

  QQmlApplicationEngine engine;
  engine.setInitialProperties(
      {{QStringLiteral("appController"), QVariant::fromValue(&controller)}});
  engine.loadFromModule(QStringLiteral("MianA"), QStringLiteral("Main"));
  if (engine.rootObjects().isEmpty())
    return 1;

  if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst())) {
    // Keep the native Quick surface alpha-capable so panel colors can reveal
    // the desktop without also lowering text opacity.
    if (auto *quickWindow = qobject_cast<QQuickWindow *>(window))
      quickWindow->setColor(Qt::transparent);
    windows.watchWindow(window);
  }
  // The legacy application creates its tray menu only after QML has loaded.
  // Preserve that order so the QApplication style, palette and fonts have
  // completed the same initialization before QMenu snapshots them.
  windows.initializeTray();
  windows.setTrayState(controller.floating(), controller.paused(),
                       controller.autostart());
  QObject::connect(&app, &QGuiApplication::focusWindowChanged, &windows,
                   [&windows](QWindow *window) {
                     if (window)
                       windows.watchWindow(window);
                   });
  return app.exec();
}
