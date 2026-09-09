#pragma once

#include "MianA.Infrastructure/AppStore.h"
#include "MianA.UI/PositionModel.h"
#include "MianA.Market/MarketService.h"
#include "MianA.Infrastructure/UpdateChecker.h"

#include <QColor>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <optional>

class WindowManager;

class AppController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("AppController is created by the application")
  Q_PROPERTY(PositionModel *positions READ positions CONSTANT)
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
  Q_PROPERTY(bool showIntraday READ showIntraday NOTIFY stateChanged)
  Q_PROPERTY(bool hideStockCode READ hideStockCode NOTIFY stateChanged)
  Q_PROPERTY(QString themeColor READ themeColor NOTIFY stateChanged)
  Q_PROPERTY(int frameOpacity READ frameOpacity NOTIFY stateChanged)
  Q_PROPERTY(int textOpacity READ textOpacity NOTIFY stateChanged)
  Q_PROPERTY(QString paletteBg READ paletteBg NOTIFY paletteChanged)
  Q_PROPERTY(QString palettePanel READ palettePanel NOTIFY paletteChanged)
  Q_PROPERTY(QString paletteHover READ paletteHover NOTIFY paletteChanged)
  Q_PROPERTY(QString paletteLine READ paletteLine NOTIFY paletteChanged)
  Q_PROPERTY(QString paletteText READ paletteText NOTIFY paletteChanged)
  Q_PROPERTY(QString paletteMuted READ paletteMuted NOTIFY paletteChanged)
  Q_PROPERTY(QString paletteHint READ paletteHint NOTIFY paletteChanged)
  Q_PROPERTY(QString paletteUp READ paletteUp NOTIFY paletteChanged)
  Q_PROPERTY(QString paletteDown READ paletteDown NOTIFY paletteChanged)
  Q_PROPERTY(QString paletteWarning READ paletteWarning NOTIFY paletteChanged)
  Q_PROPERTY(QString focusedSymbol READ focusedSymbol NOTIFY focusChanged)
  Q_PROPERTY(QString focusedName READ focusedName NOTIFY focusChanged)
  Q_PROPERTY(QString focusedPrice READ focusedPrice NOTIFY focusChanged)

public:
  explicit AppController(WindowManager &windows, QObject *parent = nullptr);
  [[nodiscard]] PositionModel *positions() noexcept;
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
  bool showIntraday() const;
  bool hideStockCode() const;
  QString themeColor() const;
  int frameOpacity() const;
  int textOpacity() const;
  QString paletteBg() const;
  QString palettePanel() const;
  QString paletteHover() const;
  QString paletteLine() const;
  QString paletteText() const;
  QString paletteMuted() const;
  QString paletteHint() const;
  QString paletteUp() const;
  QString paletteDown() const;
  QString paletteWarning() const;
  QString focusedSymbol() const;
  QString focusedName() const;
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
  void showCurrent();
  void showFull();
  void toggleFloatingAndShow();
  void setAutostart(bool enabled);
  void previewAppearance(bool autoTheme, const QString &color, int frameOpacity,
                         int textOpacity);
  void toggle(const QString &key);
  bool saveSettings(bool paused, bool floating, bool tray, bool autostart,
                    bool showIntraday, bool hideStockCode, bool autoTheme,
                    const QString &color, int frameOpacity, int textOpacity);
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
    int frameOpacity;
    int textOpacity;
  };
  [[nodiscard]] const Position *focused() const;
  [[nodiscard]] QHash<QString, QString> palette() const;
  [[nodiscard]] QString marketSummary() const;
  void setStatus(const QString &text, const QString &color);
  bool saveStore();
  void scheduleSave();
  void maybeRefreshOnScheduleChange();
  void requestRefresh(bool force, bool showProgress);
  void applyQuoteBatch(int batchId, const QVector<QuoteResult> &results);
  void refreshIntradayHistory();
  void applyIntradayBatch(int batchId, const QVector<IntradayResult> &results);
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
  QHash<QString, int> m_lastIntradayBatch;
  QHash<QString, QDate> m_closedRefreshDates;
  QHash<QString, QDate> m_breakRefreshDates;
  bool m_pendingRefresh = false;
  bool m_pendingForce = false;
  bool m_pendingProgress = false;
  bool m_checkUpdatesNotify = false;
  bool m_shuttingDown = false;
};
