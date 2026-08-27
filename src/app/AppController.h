#pragma once

#include "AppStore.h"
#include "models/PositionModel.h"
#include "services/MarketService.h"
#include "services/UpdateChecker.h"

#include <QColor>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <optional>

class WindowManager;

class AppController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QObject *positions READ positions CONSTANT)
  Q_PROPERTY(QString iconUrl READ iconUrl CONSTANT)
  Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
  Q_PROPERTY(QString statusColor READ statusColor NOTIFY statusChanged)
  Q_PROPERTY(int count READ count NOTIFY stateChanged)
  Q_PROPERTY(int savedX READ savedX NOTIFY stateChanged)
  Q_PROPERTY(int savedY READ savedY NOTIFY stateChanged)
  Q_PROPERTY(bool topmost READ topmost NOTIFY stateChanged)
  Q_PROPERTY(bool locked READ locked NOTIFY stateChanged)
  Q_PROPERTY(bool compact READ compact NOTIFY stateChanged)
  Q_PROPERTY(bool floating READ floating NOTIFY stateChanged)
  Q_PROPERTY(bool paused READ paused NOTIFY stateChanged)
  Q_PROPERTY(bool autoTheme READ autoTheme NOTIFY stateChanged)
  Q_PROPERTY(bool minimizeToTray READ minimizeToTray NOTIFY stateChanged)
  Q_PROPERTY(bool autostart READ autostart NOTIFY stateChanged)
  Q_PROPERTY(QString themeColor READ themeColor NOTIFY stateChanged)
  Q_PROPERTY(int themeOpacity READ themeOpacity NOTIFY stateChanged)
  Q_PROPERTY(QString paletteBg READ paletteBg NOTIFY paletteChanged)
  Q_PROPERTY(QString palettePanel READ palettePanel NOTIFY paletteChanged)
  Q_PROPERTY(QString paletteHover READ paletteHover NOTIFY paletteChanged)
  Q_PROPERTY(QString paletteLine READ paletteLine NOTIFY paletteChanged)
  Q_PROPERTY(QString paletteText READ paletteText NOTIFY paletteChanged)
  Q_PROPERTY(QString paletteMuted READ paletteMuted NOTIFY paletteChanged)
  Q_PROPERTY(QString paletteHint READ paletteHint NOTIFY paletteChanged)
  Q_PROPERTY(QString focusedSymbol READ focusedSymbol NOTIFY focusChanged)
  Q_PROPERTY(QString focusedPrice READ focusedPrice NOTIFY focusChanged)

public:
  explicit AppController(WindowManager &windows, QObject *parent = nullptr);
  QObject *positions();
  QString iconUrl() const;
  QString statusText() const;
  QString statusColor() const;
  int count() const;
  int savedX() const;
  int savedY() const;
  bool topmost() const;
  bool locked() const;
  bool compact() const;
  bool floating() const;
  bool paused() const;
  bool autoTheme() const;
  bool minimizeToTray() const;
  bool autostart() const;
  QString themeColor() const;
  int themeOpacity() const;
  QString paletteBg() const;
  QString palettePanel() const;
  QString paletteHover() const;
  QString paletteLine() const;
  QString paletteText() const;
  QString paletteMuted() const;
  QString paletteHint() const;
  QString focusedSymbol() const;
  QString focusedPrice() const;

public slots:
  void setWindowPosition(int x, int y);
  void setFocus(const QString &positionId);
  void cycleFocus();
  QString addPosition(const QString &symbol, const QString &name);
  QString editPosition(const QString &positionId, const QString &symbol,
                       const QString &name);
  void deletePosition(const QString &positionId);
  void movePosition(int source, int destination);
  void showFull();
  void toggleFloatingAndShow();
  void setAutostart(bool enabled);
  void previewAppearance(bool autoTheme, const QString &color, int opacity);
  void toggle(const QString &key);
  bool saveSettings(bool paused, bool floating, bool tray, bool autostart,
                    bool autoTheme, const QString &color, int opacity);
  void initialRefresh();
  void manualRefresh();
  void refreshQuotes(bool force = false);
  void checkForUpdates(bool notifyIfCurrent = false);
  void sampleTheme(int x, int y, int width, int height);
  void saveAndShutdown();

signals:
  void statusChanged();
  void stateChanged();
  void focusChanged();
  void paletteChanged();
  void showMainRequested();
  void toggleMainRequested();
  void settingsRequested();
  void shutdownRequested();

private:
  struct Appearance {
    bool automatic;
    QString color;
    int opacity;
  };
  [[nodiscard]] const Position *focused() const;
  [[nodiscard]] QHash<QString, QString> palette() const;
  [[nodiscard]] QString marketSummary() const;
  void setStatus(const QString &text, const QString &color);
  bool saveStore();
  void scheduleSave();
  void requestRefresh(bool force, bool showProgress);
  void applyQuoteBatch(int batchId, const QVector<QuoteResult> &results);
  void syncTray();

  WindowManager &m_windows;
  AppStore m_store;
  AppSettings m_settings;
  PositionModel m_model;
  MarketService m_market;
  UpdateChecker m_updater;
  QTimer m_timer;
  QTimer m_saveTimer;
  QString m_statusText = QStringLiteral("等待刷新");
  QString m_statusColor = QStringLiteral("#657582");
  std::optional<Appearance> m_preview;
  std::optional<QColor> m_underlayColor;
  int m_inflightBatches = 0;
  int m_nextBatchId = 0;
  QHash<QString, int> m_lastResultBatch;
  QHash<QString, int> m_lastSuccessBatch;
  bool m_pendingRefresh = false;
  bool m_pendingForce = false;
  bool m_pendingProgress = false;
  bool m_checkUpdatesNotify = false;
  bool m_shuttingDown = false;
};
