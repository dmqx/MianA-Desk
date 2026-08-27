#include "WindowManager.h"

#include "AppController.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMenu>
#include <QProxyStyle>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleOptionMenuItem>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QWindow>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <shobjidl.h>
#include <windows.h>
#pragma comment(lib, "ole32.lib")
#endif
#include <utility>

#ifdef Q_OS_WIN
namespace {
WindowManager *g_winEventHookTarget = nullptr;
HWINEVENTHOOK g_foregroundHook = nullptr;
HWINEVENTHOOK g_minimizeHook = nullptr;
HWINEVENTHOOK g_reorderHook = nullptr;
ITaskbarList *g_taskbarList = nullptr;
} // namespace
static void WINAPI winEventHookProc(HWINEVENTHOOK hook, DWORD event,
                                   HWND hwnd, LONG idObject,
                                   LONG idChild, DWORD eventThread,
                                   DWORD eventTime);
#endif

namespace {
constexpr int HotkeyId = 0x4D41;
const QString InstanceId = QStringLiteral("MianADesk.Qt.Cpp.v2");
const QString RunKey = QStringLiteral(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString AppRegistryId = QStringLiteral("MianADesk");
const QString LegacyRegistryId = QStringLiteral("StockDeskWidget");

class CompactTrayMenuStyle final : public QProxyStyle {
public:
  using QProxyStyle::QProxyStyle;

  int pixelMetric(PixelMetric metric, const QStyleOption *option,
                  const QWidget *widget) const override {
    if (metric == PM_IndicatorWidth || metric == PM_IndicatorHeight ||
        metric == PM_SmallIconSize)
      return 13;
    return QProxyStyle::pixelMetric(metric, option, widget);
  }

  QSize sizeFromContents(ContentsType type, const QStyleOption *option,
                         const QSize &size,
                         const QWidget *widget) const override {
    QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
    if (type == CT_MenuItem) {
      const auto *item =
          qstyleoption_cast<const QStyleOptionMenuItem *>(option);
      if (item && item->menuItemType != QStyleOptionMenuItem::Separator) {
        result.setHeight(22);
      }
    }
    return result;
  }

  void drawControl(ControlElement element, const QStyleOption *option,
                   QPainter *painter,
                   const QWidget *widget) const override {
    if (element == CE_MenuItem) {
      const auto *item =
          qstyleoption_cast<const QStyleOptionMenuItem *>(option);
      if (item) {
        QStyleOptionMenuItem compact(*item);
        compact.maxIconWidth = 13;
        QProxyStyle::drawControl(element, &compact, painter, widget);
        return;
      }
    }
    QProxyStyle::drawControl(element, option, painter, widget);
  }
};
} // namespace

WindowManager::WindowManager(QObject *parent) : QObject(parent) {
#ifdef Q_OS_WIN
  m_showMessage =
      RegisterWindowMessageW(L"MianADesk.Qt.Cpp.v2.Show");
#endif
  if (qApp)
    qApp->installNativeEventFilter(this);
#ifdef Q_OS_WIN
  // React immediately to system events that can drop the widget out
  // of the topmost band (opening another app, Win+D, z-order changes)
  // by re-asserting the native topmost z-order from the event hook.
  g_winEventHookTarget = this;
  g_foregroundHook = SetWinEventHook(
      EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
      &winEventHookProc, 0, 0, WINEVENT_OUTOFCONTEXT);
  g_minimizeHook = SetWinEventHook(
      EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_SWITCHEND, nullptr,
      &winEventHookProc, 0, 0, WINEVENT_OUTOFCONTEXT);
  g_reorderHook = SetWinEventHook(
      EVENT_OBJECT_REORDER, EVENT_OBJECT_REORDER, nullptr,
      &winEventHookProc, 0, 0, WINEVENT_OUTOFCONTEXT);
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr,
                                 CLSCTX_INPROC_SERVER, IID_ITaskbarList,
                                 reinterpret_cast<void **>(&g_taskbarList))))
    g_taskbarList->HrInit();
  // Low-frequency safety net in case an event is missed.
  m_topmostTimer.setInterval(5000);
  connect(&m_topmostTimer, &QTimer::timeout, this, [this] {
    for (const auto &window : std::as_const(m_windows))
      if (window)
        applyNativeWindowStyle(window);
  });
  m_topmostTimer.start();
#endif
  if (qGuiApp) {
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
              if (state == Qt::ApplicationInactive)
                for (const auto &window : std::as_const(m_windows))
                  if (window)
                    applyTopmost(window);
            });
  }
}

WindowManager::~WindowManager() {
#ifdef Q_OS_WIN
  if (g_foregroundHook)
    UnhookWinEvent(g_foregroundHook);
  if (g_minimizeHook)
    UnhookWinEvent(g_minimizeHook);
  if (g_reorderHook)
    UnhookWinEvent(g_reorderHook);
  g_foregroundHook = nullptr;
  g_minimizeHook = nullptr;
  g_reorderHook = nullptr;
  g_winEventHookTarget = nullptr;
  if (g_taskbarList) {
    g_taskbarList->Release();
    g_taskbarList = nullptr;
  }
  CoUninitialize();
#endif
  unregisterHotkey();
  if (m_tray) {
    m_tray->hide();
    m_tray->setContextMenu(nullptr);
  }
  delete m_trayMenu;
  m_trayMenu = nullptr;
  if (qApp)
    qApp->removeNativeEventFilter(this);
}

bool WindowManager::acquireSingleInstance() {
  const QString lockPath =
      QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
          .filePath(InstanceId + QStringLiteral(".lock"));
  m_lock = std::make_unique<QLockFile>(lockPath);
  m_lock->setStaleLockTime(0);
  if (!m_lock->tryLock(0)) {
    bool delivered = false;
    QLocalSocket socket;
    socket.connectToServer(InstanceId);
    if (socket.waitForConnected(750)) {
      socket.write("show\n");
      socket.flush();
      if (socket.waitForBytesWritten(750) && socket.waitForReadyRead(750))
        delivered = socket.readAll().contains("ok");
    }
#ifdef Q_OS_WIN
    if (!delivered && m_showMessage != 0)
      PostMessageW(HWND_BROADCAST, m_showMessage, 0, 0);
#endif
    return false;
  }
  QLocalServer::removeServer(InstanceId);
  m_server = std::make_unique<QLocalServer>();
  if (!m_server->listen(InstanceId))
    return false;
  connect(m_server.get(), &QLocalServer::newConnection, this, [this] {
    while (auto *socket = m_server->nextPendingConnection()) {
      socket->setParent(m_server.get());
      const auto handleRequest = [this, socket] {
        if (socket->readAll().contains("show")) {
          emit showRequested();
          socket->write("ok\n");
          socket->flush();
        }
      };
      connect(socket, &QLocalSocket::readyRead, this, handleRequest);
      connect(socket, &QLocalSocket::disconnected, this, handleRequest);
      connect(socket, &QLocalSocket::disconnected, socket,
              &QObject::deleteLater);

      // The client writes immediately after connecting. On a fast local
      // connection the payload can already be buffered before newConnection
      // is handled, so readyRead has already fired by the time it is connected.
      // Consume any pending request now as well as listening for later data.
      if (socket->bytesAvailable() > 0)
        handleRequest();
    }
  });
  registerHotkey();
  return true;
}

void WindowManager::initializeTray(AppController *controller) {
  m_trayMenu = new QMenu();
  auto *menu = m_trayMenu;
  // Match the legacy PySide implementation exactly: proxy the application's
  // existing widget style and only override its compact menu metrics. Qt then
  // keeps the native Windows font, palette and per-monitor DPI handling.
  auto *compactMenuStyle = new CompactTrayMenuStyle();
  compactMenuStyle->setParent(menu);
  menu->setStyle(compactMenuStyle);
  auto *show = menu->addAction(QStringLiteral("显示完整组件"));
  menu->addSeparator();
  auto *floating = menu->addAction(QStringLiteral("自由浮窗模式"));
  floating->setCheckable(true);
  auto *paused = menu->addAction(QStringLiteral("暂停行情刷新"));
  paused->setCheckable(true);
  auto *autostart = menu->addAction(QStringLiteral("开机自动启动"));
  autostart->setCheckable(true);
  auto *checkUpdate = menu->addAction(QStringLiteral("检查更新"));
  auto *settings = menu->addAction(QStringLiteral("设置"));
  menu->addSeparator();
  auto *quit = menu->addAction(QStringLiteral("退出"));
  connect(show, &QAction::triggered, controller, &AppController::showFull);
  connect(floating, &QAction::triggered, controller,
          &AppController::toggleFloatingAndShow);
  connect(paused, &QAction::triggered, controller,
          [controller] { controller->toggle(QStringLiteral("paused")); });
  connect(autostart, &QAction::triggered, controller,
          &AppController::setAutostart);
  connect(settings, &QAction::triggered, controller,
          &AppController::settingsRequested);
  connect(checkUpdate, &QAction::triggered, controller,
          [controller] { controller->checkForUpdates(true); });
  connect(quit, &QAction::triggered, controller,
          &AppController::saveAndShutdown);
  m_tray = new QSystemTrayIcon(
      QIcon(QStringLiteral(":/qt/qml/MianA/assets/miana_desk.ico")), this);
  m_tray->setToolTip(QStringLiteral("MianA Desk"));
  m_tray->setContextMenu(menu);
  connect(m_tray, &QSystemTrayIcon::messageClicked, this, [this] {
    if (!m_updateUrl.isEmpty())
      QDesktopServices::openUrl(m_updateUrl);
  });
  connect(m_tray, &QSystemTrayIcon::activated, controller,
          [controller](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger)
              controller->showFull();
          });
  m_trayActions.insert(QStringLiteral("floating"), floating);
  m_trayActions.insert(QStringLiteral("paused"), paused);
  m_trayActions.insert(QStringLiteral("autostart"), autostart);
  setTrayState(controller->floating(), controller->paused(),
               controller->autostart());
  m_tray->show();
}

void WindowManager::watchWindow(QWindow *window) {
  if (!window || m_windows.contains(window))
    return;
  // QMenu already uses the native Windows frame, shadow, palette and font.
  // Applying the frameless QML DWM treatment changes its text composition and
  // can produce visibly different antialiasing at fractional DPI scales.
  if (m_trayMenu && window == m_trayMenu->windowHandle())
    return;
  m_windows.push_back(window);
  window->installEventFilter(this);
  if (window->isVisible())
    scheduleDwm(window);
}

void WindowManager::setTrayState(bool floating, bool paused, bool autostart) {
  if (auto *action = qobject_cast<QAction *>(
          m_trayActions.value(QStringLiteral("floating"))))
    action->setChecked(floating);
  if (auto *action = qobject_cast<QAction *>(
          m_trayActions.value(QStringLiteral("paused"))))
    action->setChecked(paused);
  if (auto *action = qobject_cast<QAction *>(
          m_trayActions.value(QStringLiteral("autostart"))))
    action->setChecked(autostart);
}

void WindowManager::showTrayMessage(const QString &title,
                                    const QString &message) {
  if (m_tray)
    m_tray->showMessage(title, message, QSystemTrayIcon::Information, 6000);
}
void WindowManager::showUpdateNotification(const QString &tagName,
                                           const QUrl &url) {
  m_updateUrl = url;
  showTrayMessage(QStringLiteral("发现新版本"),
                  QStringLiteral("MianA Desk %1 已发布，点击查看下载页")
                      .arg(tagName));
}
bool WindowManager::autostartEnabled() const {
#ifdef Q_OS_WIN
  QSettings settings(RunKey, QSettings::NativeFormat);
  return !settings.value(AppRegistryId).toString().isEmpty() ||
         !settings.value(LegacyRegistryId).toString().isEmpty();
#else
  return false;
#endif
}

bool WindowManager::setAutostart(bool enabled) {
#ifdef Q_OS_WIN
  QSettings settings(RunKey, QSettings::NativeFormat);
  if (enabled) {
    QString executable = qEnvironmentVariable("MIANA_SINGLE_EXE");
    if (executable.isEmpty() || !QFileInfo::exists(executable))
      executable = QCoreApplication::applicationFilePath();
    settings.setValue(
        AppRegistryId,
        QStringLiteral("\"") + QDir::toNativeSeparators(executable) +
            QStringLiteral("\""));
    settings.remove(LegacyRegistryId);
  } else {
    settings.remove(AppRegistryId);
    settings.remove(LegacyRegistryId);
  }
  settings.sync();
  return settings.status() == QSettings::NoError;
#else
  Q_UNUSED(enabled);
  return false;
#endif
}

bool WindowManager::nativeEventFilter(const QByteArray &eventType,
                                      void *message, qintptr *result) {
  Q_UNUSED(result);
#ifdef Q_OS_WIN
  if (eventType == QByteArrayLiteral("windows_generic_MSG") ||
      eventType == QByteArrayLiteral("windows_dispatcher_MSG")) {
    const auto *msg = static_cast<MSG *>(message);
    if (m_showMessage != 0 && msg->message == m_showMessage) {
      emit showRequested();
      return true;
    }
    // The widget lives in the tray and stays on top: swallow minimize
    // commands (Alt+Space, Win+D's "show desktop") so it never flickers
    // by being minimized and then restored.
    if (msg->message == WM_SYSCOMMAND &&
        (msg->wParam & 0xFFF0) == SC_MINIMIZE)
      return true;
    if (msg->message == WM_HOTKEY && msg->wParam == HotkeyId) {
      emit hotkeyActivated();
      return true;
    }
    if (msg->message == WM_DWMCOMPOSITIONCHANGED)
      for (const auto &window : std::as_const(m_windows))
        if (window)
          scheduleDwm(window);
  }
#else
  Q_UNUSED(eventType);
  Q_UNUSED(message);
#endif
  return false;
}

bool WindowManager::eventFilter(QObject *watched, QEvent *event) {
  if (auto *window = qobject_cast<QWindow *>(watched)) {
    if (event->type() == QEvent::Show ||
        event->type() == QEvent::PlatformSurface ||
        event->type() == QEvent::WinIdChange)
      scheduleDwm(window);
    if (event->type() == QEvent::WindowDeactivate ||
        event->type() == QEvent::WindowStateChange)
      applyTopmost(window);
  }
  return QObject::eventFilter(watched, event);
}

void WindowManager::registerHotkey() {
#ifdef Q_OS_WIN
  m_hotkeyRegistered =
      RegisterHotKey(nullptr, HotkeyId, MOD_ALT | MOD_NOREPEAT, 0x51) != FALSE;
#endif
}
void WindowManager::unregisterHotkey() {
#ifdef Q_OS_WIN
  if (m_hotkeyRegistered)
    UnregisterHotKey(nullptr, HotkeyId);
#endif
  m_hotkeyRegistered = false;
}

void WindowManager::scheduleDwm(QWindow *window) {
  QPointer<QWindow> safe(window);
  QTimer::singleShot(0, this, [this, safe] {
    if (safe && safe->isVisible())
      applyDwm(safe);
  });
}

#ifdef Q_OS_WIN
static void WINAPI winEventHookProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                    LONG idObject, LONG idChild,
                                    DWORD eventThread, DWORD eventTime) {
  Q_UNUSED(hook);
  Q_UNUSED(hwnd);
  Q_UNUSED(idObject);
  Q_UNUSED(idChild);
  Q_UNUSED(eventThread);
  Q_UNUSED(eventTime);
  if (WindowManager *target = g_winEventHookTarget)
    target->onWinEvent(event);
}
#endif

void WindowManager::onWinEvent(quint32 event) {
  // Re-assert the topmost z-order on every event that can drop the
  // widget out of the topmost band. No time-based throttling here:
  // the taskbar re-orders aggressively, and a debounce would let it
  // win the race (the widget would stay below the taskbar).
  switch (event) {
  case EVENT_SYSTEM_FOREGROUND:
  case EVENT_SYSTEM_MINIMIZESTART:
  case EVENT_SYSTEM_MINIMIZEEND:
  case EVENT_SYSTEM_SWITCHSTART:
  case EVENT_SYSTEM_SWITCHEND:
  case EVENT_OBJECT_REORDER:
    break;
  default:
    return;
  }
  for (const auto &window : std::as_const(m_windows))
    if (window)
      applyTopmost(window);
}

void WindowManager::applyTopmost(QWindow *window) {
#ifdef Q_OS_WIN
  if (!window || !(window->flags() & Qt::WindowStaysOnTopHint))
    return;
  const HWND hwnd = reinterpret_cast<HWND>(window->winId());
  if (!hwnd)
    return;
  if (IsIconic(hwnd))
    ShowWindow(hwnd, SW_RESTORE);
  SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
  Q_UNUSED(window);
#endif
}

void WindowManager::applyNativeWindowStyle(QWindow *window) {
#ifdef Q_OS_WIN
  if (!window)
    return;
  const HWND hwnd = reinterpret_cast<HWND>(window->winId());
  if (!hwnd)
    return;
  // This widget lives in the tray: keep it out of the taskbar and
  // Alt+Tab by turning off WS_EX_APPWINDOW and enabling WS_EX_TOOLWINDOW.
  LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
  exStyle &= ~WS_EX_APPWINDOW;
  exStyle |= WS_EX_TOOLWINDOW;
  SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
  // Remove the taskbar button natively as well; Qt keeps re-adding
  // WS_EX_APPWINDOW for plain top-level windows, so DeleteTab is the
  // reliable way to keep the widget tray-only.
  if (g_taskbarList)
    g_taskbarList->DeleteTab(hwnd);
  if (!(window->flags() & Qt::WindowStaysOnTopHint))
    return;
  // Win+D (show desktop) can minimize the widget; bring it back so
  // topmost mode keeps it visible.
  if (IsIconic(hwnd))
    ShowWindow(hwnd, SW_RESTORE);
  // Move the window to the very top of the topmost band, i.e. above the
  // taskbar as well (SetWindowPos with a specific handle would place it
  // BEHIND that window, which is the opposite of what we need here).
  SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
  Q_UNUSED(window);
#endif
}

void WindowManager::applyDwm(QWindow *window) {
#ifdef Q_OS_WIN
  const HWND hwnd = reinterpret_cast<HWND>(window->winId());
  if (!hwnd)
    return;
  const bool positionMenu =
      window->objectName() == QStringLiteral("positionMenu");
  const int ncRendering = positionMenu ? DWMNCRP_DISABLED : DWMNCRP_ENABLED;
  // DWMWCP_ROUND is Windows' larger standard rounded-corner treatment.
  const int rounded = 2;
  const int backdropNone = 1;
  const COLORREF noColor = 0xFFFFFFFE;
  DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncRendering,
                        sizeof(ncRendering));
  DwmSetWindowAttribute(hwnd, 33, &rounded, sizeof(rounded));
  DwmSetWindowAttribute(hwnd, 38, &backdropNone, sizeof(backdropNone));
  DwmSetWindowAttribute(hwnd, 34, &noColor, sizeof(noColor));
  const MARGINS margins = positionMenu ? MARGINS{0, 0, 0, 0}
                                       : MARGINS{0, 0, 0, 1};
  DwmExtendFrameIntoClientArea(hwnd, &margins);
  applyNativeWindowStyle(window);
#else
  Q_UNUSED(window);
#endif
}
