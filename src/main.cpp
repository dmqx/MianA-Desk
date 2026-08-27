#include "app/AppController.h"
#include "app/WindowManager.h"

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
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

  QQmlApplicationEngine engine;
  engine.setInitialProperties(
      {{QStringLiteral("appController"), QVariant::fromValue(&controller)}});
  engine.loadFromModule(QStringLiteral("MianA"), QStringLiteral("Main"));
  if (engine.rootObjects().isEmpty())
    return 1;

  if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst()))
    windows.watchWindow(window);
  // The legacy application creates its tray menu only after QML has loaded.
  // Preserve that order so the QApplication style, palette and fonts have
  // completed the same initialization before QMenu snapshots them.
  windows.initializeTray(&controller);
  QObject::connect(&app, &QGuiApplication::focusWindowChanged, &windows,
                   [&windows](QWindow *window) {
                     if (window)
                       windows.watchWindow(window);
                   });
  return app.exec();
}
