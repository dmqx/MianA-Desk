#pragma once

#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <memory>

class AppController;
class QLocalServer;
class QLockFile;
class QMenu;
class QSystemTrayIcon;
class QWindow;

class WindowManager final : public QObject, public QAbstractNativeEventFilter {
  Q_OBJECT
public:
  explicit WindowManager(QObject *parent = nullptr);
  ~WindowManager() override;

  bool acquireSingleInstance();
  void initializeTray(AppController *controller);
  void watchWindow(QWindow *window);
  void setTrayState(bool floating, bool paused, bool autostart);

  [[nodiscard]] bool autostartEnabled() const;
  bool setAutostart(bool enabled);
  void showTrayMessage(const QString &title, const QString &message);
  void showUpdateNotification(const QString &tagName, const QUrl &url);
  void onWinEvent(quint32 event);

  bool nativeEventFilter(const QByteArray &eventType, void *message,
                         qintptr *result) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void showRequested();
  void hotkeyActivated();

private:
  void registerHotkey();
  void unregisterHotkey();
  void applyDwm(QWindow *window);
  void scheduleDwm(QWindow *window);
  void applyNativeWindowStyle(QWindow *window);
  void applyTopmost(QWindow *window);

  std::unique_ptr<QLockFile> m_lock;
  std::unique_ptr<QLocalServer> m_server;
  QSystemTrayIcon *m_tray = nullptr;
  QMenu *m_trayMenu = nullptr;
  QHash<QString, QObject *> m_trayActions;
  QVector<QPointer<QWindow>> m_windows;
  QTimer m_topmostTimer;
  QUrl m_updateUrl;
  quint32 m_showMessage = 0;
  bool m_hotkeyRegistered = false;
};
