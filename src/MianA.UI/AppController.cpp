#include "MianA.UI/AppController.h"

#include "MianA.Core/IntradaySeries.h"
#include "MianA.Core/MarketRules.h"
#include "MianA.Windows/WindowManager.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QImage>
#include <QPoint>
#include <QPixmap>
#include <QScreen>
#include <QTime>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace {
constexpr int RefreshIntervalMs = 600;

QString blend(const QColor &base, const QColor &target, double amount) {
  const auto channel = [amount](int from, int to) {
    return qRound(from + (to - from) * amount);
  };
  return QColor(channel(base.red(), target.red()),
                channel(base.green(), target.green()),
                channel(base.blue(), target.blue()))
      .name(QColor::HexRgb)
      .toUpper();
}

QString withOpacity(const QString &color, int opacity) {
  QColor result(color);
  result.setAlpha(std::clamp(opacity, 0, 100) * 255 / 100);
  return result.name(QColor::HexArgb).toUpper();
}
} // namespace

AppController::AppController(WindowManager &windows, QObject *parent)
    : QObject(parent), m_windows(windows), m_model(this), m_market(this),
      m_updater(this), m_timer(this) {
  connect(this, &AppController::paletteChanged, this, &AppController::statusChanged);
  QVector<Position> loaded;
  if (!m_store.load(loaded, m_settings)) {
    m_statusText = m_store.loadError();
    m_statusColor = QStringLiteral("#E88B00");
  }
  m_model.replaceAll(std::move(loaded));
  connect(&m_market, &MarketService::batchReady, this,
          &AppController::applyQuoteBatch);
  connect(&m_market, &MarketService::intradayReady, this,
          &AppController::applyIntradayBatch);
  connect(&m_timer, &QTimer::timeout, this, [this] {
    refreshQuotes(false);
    maybeRefreshOnScheduleChange();
  });
  connect(&m_windows, &WindowManager::showRequested, this,
          &AppController::showCurrent);
  connect(&m_windows, &WindowManager::hotkeyActivated, this,
          &AppController::toggleMainRequested);
  m_saveTimer.setSingleShot(true);
  m_saveTimer.setInterval(10000);
  connect(&m_saveTimer, &QTimer::timeout, this, [this] { saveStore(); });
  connect(&m_updater, &UpdateChecker::updateAvailable, this,
          [this](const QString &tagName, const QUrl &url) {
            m_checkUpdatesNotify = false;
            m_windows.showUpdateNotification(tagName, url);
            setStatus(QStringLiteral("发现新版本 %1").arg(tagName),
                      QStringLiteral("#30B96A"));
          });
  connect(&m_updater, &UpdateChecker::upToDate, this, [this] {
    if (m_checkUpdatesNotify) {
      setStatus(QStringLiteral("已是最新版本"), QStringLiteral("#30B96A"));
      m_windows.showTrayMessage(QStringLiteral("MianA Desk"),
                                QStringLiteral("已是最新版本"));
    }
    m_checkUpdatesNotify = false;
  });
  connect(&m_updater, &UpdateChecker::checkFailed, this, [this] {
    if (m_checkUpdatesNotify) {
      setStatus(QStringLiteral("检查更新失败，请检查网络后重试"),
                QStringLiteral("#E88B00"));
      m_windows.showTrayMessage(QStringLiteral("MianA Desk"),
                                QStringLiteral("检查更新失败，请检查网络后重试"));
    }
    m_checkUpdatesNotify = false;
  });
  if (!m_settings.paused)
    m_timer.start(RefreshIntervalMs);
  QTimer::singleShot(500, this, &AppController::initialRefresh);
  QTimer::singleShot(5000, this, [this] { checkForUpdates(false); });
}

PositionModel *AppController::positions() noexcept { return &m_model; }
QString AppController::iconUrl() const {
  return QStringLiteral("qrc:/qt/qml/MianA/assets/miana_desk.png");
}
QString AppController::statusText() const { return m_statusText; }
QString AppController::statusColor() const {
  return withOpacity(m_statusColor, textOpacity());
}
int AppController::count() const { return m_model.rowCount(); }
int AppController::savedX() const { return m_settings.x; }
int AppController::savedY() const { return m_settings.y; }
bool AppController::topmost() const { return m_settings.topmost; }
bool AppController::locked() const { return m_settings.locked; }
bool AppController::compact() const { return m_settings.compact; }
bool AppController::floating() const { return m_settings.floating; }
bool AppController::paused() const { return m_settings.paused; }
bool AppController::autoTheme() const {
  return m_preview ? m_preview->automatic : m_settings.autoTheme;
}
bool AppController::minimizeToTray() const { return m_settings.minimizeToTray; }
bool AppController::autostart() const { return m_windows.autostartEnabled(); }
bool AppController::showIntraday() const { return m_settings.showIntraday; }
bool AppController::hideStockCode() const { return m_settings.hideStockCode; }
QString AppController::themeColor() const {
  return m_preview ? m_preview->color : m_settings.themeColor;
}
int AppController::frameOpacity() const {
  return m_preview ? m_preview->frameOpacity : m_settings.frameOpacity;
}
int AppController::textOpacity() const {
  return m_preview ? m_preview->textOpacity : m_settings.textOpacity;
}
QString AppController::paletteBg() const {
  return palette().value(QStringLiteral("bg"));
}
QString AppController::palettePanel() const {
  return palette().value(QStringLiteral("panel"));
}
QString AppController::paletteHover() const {
  return palette().value(QStringLiteral("hover"));
}
QString AppController::paletteLine() const {
  return palette().value(QStringLiteral("line"));
}
QString AppController::paletteText() const {
  return palette().value(QStringLiteral("text"));
}
QString AppController::paletteMuted() const {
  return palette().value(QStringLiteral("muted"));
}
QString AppController::paletteHint() const {
  return palette().value(QStringLiteral("hint"));
}
QString AppController::paletteUp() const {
  return palette().value(QStringLiteral("up"));
}
QString AppController::paletteDown() const {
  return palette().value(QStringLiteral("down"));
}
QString AppController::paletteWarning() const {
  return palette().value(QStringLiteral("warning"));
}


const Position *AppController::focused() const {
  if (const auto *item = m_model.find(m_settings.focusId))
    return item;
  return m_model.positions().isEmpty() ? nullptr
                                       : &m_model.positions().constFirst();
}
QString AppController::focusedSymbol() const {
  const auto *item = focused();
  return item ? item->symbol : QStringLiteral("暂无自选");
}
QString AppController::focusedName() const {
  const auto *item = focused();
  return item ? item->name : QStringLiteral("暂无自选");
}
QString AppController::focusedPrice() const {
  const auto *item = focused();
  return item && item->price > 0.0
             ? PositionModel::formatPrice(item->price, item->precision)
             : QStringLiteral("--");
}

QHash<QString, QString> AppController::palette() const {
  QColor base =
      autoTheme() && m_underlayColor ? *m_underlayColor : QColor(themeColor());
  if (!base.isValid())
    base = Qt::white;
  const int frame = frameOpacity();
  const int text = textOpacity();
  const auto frameColor = [frame](const QString &color) {
    return withOpacity(color, frame);
  };
  const auto textColor = [text](const QString &color) {
    return withOpacity(color, text);
  };
  const double luminance =
      base.red() * .299 + base.green() * .587 + base.blue() * .114;
  if (luminance < 135)
    return {{QStringLiteral("bg"), frameColor(blend(base, Qt::white, .08))},
            {QStringLiteral("panel"), frameColor(blend(base, Qt::white, .15))},
            {QStringLiteral("hover"), frameColor(blend(base, Qt::white, .23))},
            {QStringLiteral("line"), frameColor(blend(base, Qt::white, .38))},
            {QStringLiteral("text"), textColor(QStringLiteral("#F5F8FA"))},
            {QStringLiteral("muted"), textColor(QStringLiteral("#CDD7DE"))},
            {QStringLiteral("hint"), textColor(QStringLiteral("#AAB8C2"))},
            {QStringLiteral("up"), textColor(QStringLiteral("#E5484D"))},
            {QStringLiteral("down"), textColor(QStringLiteral("#1FAE7A"))},
            {QStringLiteral("warning"), textColor(QStringLiteral("#E88B00"))}};
  return {{QStringLiteral("bg"), frameColor(blend(base, Qt::white, .18))},
          {QStringLiteral("panel"), frameColor(blend(base, Qt::white, .32))},
          {QStringLiteral("hover"), frameColor(blend(base, Qt::white, .43))},
          {QStringLiteral("line"), frameColor(blend(base, Qt::white, .68))},
          {QStringLiteral("text"), textColor(QStringLiteral("#18232D"))},
          {QStringLiteral("muted"), textColor(QStringLiteral("#586976"))},
          {QStringLiteral("hint"), textColor(QStringLiteral("#758691"))},
          {QStringLiteral("up"), textColor(QStringLiteral("#E5484D"))},
          {QStringLiteral("down"), textColor(QStringLiteral("#1FAE7A"))},
          {QStringLiteral("warning"), textColor(QStringLiteral("#E88B00"))}};
}

void AppController::setWindowPosition(int x, int y) {
  m_settings.x = x;
  m_settings.y = y;
  scheduleSave();
}
void AppController::setFocus(const QString &positionId) {
  if (!m_model.find(positionId))
    return;
  m_settings.focusId = positionId;
  scheduleSave();
  emit focusChanged();
}
void AppController::cycleFocus() {
  const auto &items = m_model.positions();
  if (items.isEmpty())
    return;
  int current = -1;
  for (int i = 0; i < items.size(); ++i)
    if (items.at(i).id == m_settings.focusId)
      current = i;
  setFocus(items.at((current + 1) % items.size()).id);
}

QString AppController::addPosition(const QString &rawSymbol,
                                   const QString &rawName) {
  const QString symbol = MarketRules::normalizeSymbol(rawSymbol);
  if (symbol.isEmpty())
    return QStringLiteral("请输入股票代码");
  if (m_model.containsSymbol(symbol))
    return QStringLiteral("该代码已在自选中");
  Position item;
  item.id = QUuid::createUuid().toString(QUuid::Id128);
  item.symbol = symbol;
  item.name = rawName.trimmed().isEmpty() ? symbol : rawName.trimmed().left(80);
  item.customName = !rawName.trimmed().isEmpty();
  item.precision = MarketRules::pricePrecision(symbol);
  item.currency = MarketRules::currency(symbol);
  const QString previousFocus = m_settings.focusId;
  m_model.append(item);
  m_settings.focusId = item.id;
  if (!saveStore()) {
    m_model.removeById(item.id);
    m_settings.focusId = previousFocus;
    return QStringLiteral("数据保存失败，请检查数据目录权限");
  }
  emit stateChanged();
  emit focusChanged();
  requestRefresh(true, false);
  refreshIntradayHistory();
  return {};
}

QString AppController::editPosition(const QString &positionId,
                                    const QString &rawSymbol,
                                    const QString &rawName) {
  auto *item = m_model.find(positionId);
  const QString symbol = MarketRules::normalizeSymbol(rawSymbol);
  if (!item || symbol.isEmpty())
    return QStringLiteral("请输入股票代码");
  if (m_model.containsSymbol(symbol, positionId))
    return QStringLiteral("该代码已在自选中");
  const Position previous = *item;
  const bool changed = item->symbol != symbol;
  QString suppliedName = rawName.trimmed();
  const bool keepAutomaticName =
      !changed && !item->customName && suppliedName == item->name;
  if (changed && !item->customName && suppliedName == item->name)
    suppliedName.clear();
  item->symbol = symbol;
  if (!keepAutomaticName) {
    item->name = suppliedName.isEmpty() ? symbol : suppliedName.left(80);
    item->customName = !suppliedName.isEmpty();
  }
  if (changed) {
    item->price = item->updatedAt = item->retryAfter = 0.0;
    item->error.clear();
    item->source.clear();
    item->failureCount = 0;
    item->intraday.clear();
    item->intradayMinute = 0;
    item->intradayMinutes.clear();
    item->intradayDate = {};
    item->changePercent = item->previousClose = 0.0;
    item->hasChange = false;
    item->currency = MarketRules::currency(symbol);
    item->precision = MarketRules::pricePrecision(symbol);
  }
  if (!saveStore()) {
    *item = previous;
    return QStringLiteral("数据保存失败，请检查数据目录权限");
  }
  if (changed) {
    m_lastResultBatch[positionId] = m_nextBatchId;
    m_lastSuccessBatch[positionId] = m_nextBatchId;
    m_lastIntradayBatch[positionId] = m_nextBatchId;
  }
  m_model.notifyPosition(positionId);
  emit focusChanged();
  requestRefresh(true, false);
  refreshIntradayHistory();
  return {};
}

void AppController::deletePosition(const QString &positionId) {
  const auto previous = m_model.positions();
  const QString previousFocus = m_settings.focusId;
  if (!m_model.removeById(positionId))
    return;
  if (m_settings.focusId == positionId)
    m_settings.focusId = m_model.positions().isEmpty()
                             ? QString{}
                             : m_model.positions().constFirst().id;
  if (!saveStore()) {
    m_model.replaceAll(previous);
    m_settings.focusId = previousFocus;
    return;
  }
  m_lastResultBatch.remove(positionId);
  m_lastSuccessBatch.remove(positionId);
  m_lastIntradayBatch.remove(positionId);
  emit stateChanged();
  emit focusChanged();
}

void AppController::movePosition(int source, int destination) {
  const auto previous = m_model.positions();
  if (m_model.movePosition(source, destination) && !saveStore())
    m_model.replaceAll(previous);
}

void AppController::showCurrent() { emit showMainRequested(); }

void AppController::showFull() {
  const bool changed = m_settings.floating || m_settings.compact;
  const bool oldFloating = m_settings.floating, oldCompact = m_settings.compact;
  m_settings.floating = false;
  m_settings.compact = false;
  if (changed && !saveStore()) {
    m_settings.floating = oldFloating;
    m_settings.compact = oldCompact;
  }
  if (changed) {
    emit stateChanged();
    m_windows.refreshWindowStyle();
    syncTray();
  }
  emit showMainRequested();
}
void AppController::toggleFloatingAndShow() {
  toggle(QStringLiteral("floating"));
  emit showMainRequested();
}
void AppController::setAutostart(bool enabled) {
  if (!m_windows.setAutostart(enabled))
    setStatus(QStringLiteral("开机启动设置失败，请检查系统权限"),
              QStringLiteral("#E88B00"));
  emit stateChanged();
  syncTray();
}
void AppController::previewAppearance(bool automatic, const QString &color,
                                      int frameOpacityValue,
                                      int textOpacityValue) {
  QColor normalized(color);
  const QString safe = normalized.isValid() ? normalized.name().toUpper()
                                            : m_settings.themeColor;
  m_preview = Appearance{automatic, safe,
                         std::clamp(frameOpacityValue, 10, 100),
                         std::clamp(textOpacityValue, 10, 100)};
  emit stateChanged();
  emit paletteChanged();
}

void AppController::toggle(const QString &key) {
  bool *setting = nullptr;
  if (key == QStringLiteral("topmost"))
    setting = &m_settings.topmost;
  else if (key == QStringLiteral("locked"))
    setting = &m_settings.locked;
  else if (key == QStringLiteral("compact"))
    setting = &m_settings.compact;
  else if (key == QStringLiteral("floating"))
    setting = &m_settings.floating;
  else if (key == QStringLiteral("paused"))
    setting = &m_settings.paused;
  if (!setting)
    return;
  const bool previous = *setting;
  *setting = !*setting;
  if (!saveStore())
    *setting = previous;
  emit stateChanged();
  if (key == QStringLiteral("floating"))
    m_windows.refreshWindowStyle();
  syncTray();
  if (key == QStringLiteral("paused")) {
    if (m_settings.paused) {
      m_timer.stop();
      setStatus(QStringLiteral("行情已暂停"), QStringLiteral("#657582"));
    } else {
      m_timer.start(RefreshIntervalMs);
      setStatus(QStringLiteral("等待刷新"), QStringLiteral("#657582"));
      requestRefresh(true, false);
      maybeRefreshOnScheduleChange();
    }
  }
}

bool AppController::saveSettings(bool pausedValue, bool floatingValue,
                                 bool tray, bool autostartValue,
                                 bool showIntradayValue, bool hideStockCodeValue,
                                 bool automatic, const QString &color,
                                 int frameOpacityValue, int textOpacityValue) {
  const AppSettings previous = m_settings;
  const bool wasPaused = m_settings.paused;
  QColor normalized(color);
  m_preview.reset();
  m_settings.paused = pausedValue;
  m_settings.floating = floatingValue;
  m_settings.minimizeToTray = tray;
  m_settings.autoTheme = automatic;
  m_settings.showIntraday = showIntradayValue;
  m_settings.hideStockCode = hideStockCodeValue;
  m_settings.themeColor =
      normalized.isValid() ? normalized.name().toUpper() : previous.themeColor;
  m_settings.frameOpacity = std::clamp(frameOpacityValue, 10, 100);
  m_settings.textOpacity = std::clamp(textOpacityValue, 10, 100);
  if (!saveStore()) {
    m_settings = previous;
    emit stateChanged();
    emit paletteChanged();
    return false;
  }
  // Always synchronize the Run entry so stale or legacy paths are repaired.
  const bool autostartOk = m_windows.setAutostart(autostartValue);
  if (!autostartOk)
    setStatus(QStringLiteral("开机启动设置失败，请检查系统权限"),
              QStringLiteral("#E88B00"));
  emit paletteChanged();
  emit stateChanged();
  if (previous.floating != m_settings.floating)
    m_windows.refreshWindowStyle();
  syncTray();
  if (m_settings.paused)
    m_timer.stop();
  else if (!m_timer.isActive())
    m_timer.start(RefreshIntervalMs);
  if (wasPaused != pausedValue) {
    setStatus(pausedValue ? QStringLiteral("行情已暂停")
                          : QStringLiteral("等待刷新"),
              QStringLiteral("#657582"));
    if (!pausedValue) {
      requestRefresh(true, false);
      maybeRefreshOnScheduleChange();
    }
  }
  return autostartOk;
}

void AppController::initialRefresh() {
  requestRefresh(true, true);
  refreshIntradayHistory();
  maybeRefreshOnScheduleChange();
}
void AppController::manualRefresh() { requestRefresh(true, true); }
void AppController::checkForUpdates(bool notifyIfCurrent) {
  m_checkUpdatesNotify = m_checkUpdatesNotify || notifyIfCurrent;
  if (notifyIfCurrent)
    setStatus(QStringLiteral("正在检查更新…"), QStringLiteral("#657582"));
  m_updater.check();
}
void AppController::refreshQuotes(bool force) { requestRefresh(force, false); }

void AppController::maybeRefreshOnScheduleChange() {
  if (m_shuttingDown || m_settings.paused || m_model.positions().isEmpty())
    return;
  QStringList markets;
  const auto loopUtc = QDateTime::currentDateTimeUtc();
  for (const auto &item : m_model.positions()) {
    const QString market = MarketRules::marketKey(item.symbol);
    const auto local = loopUtc.toTimeZone(MarketRules::timeZone(market));
    const QDate date = local.date();
    if (!m_market.isTradingDay(item.symbol, date))
      continue;
    const int minute = local.time().hour() * 60 + local.time().minute();
    const bool isCnOrHk = market == QStringLiteral("CN") ||
                          market == QStringLiteral("HK");
    // A股/港股午休与收盘边界触发一次。US 按美股收盘边界。
    const int breakStart = market == QStringLiteral("HK") ? 720 : 690;
    const bool breakWindow = isCnOrHk && minute >= breakStart && minute < 780;
    const bool closeWindow = market == QStringLiteral("CN")
                                 ? minute >= 900 && minute < 1440
                                 : minute >= 960 && minute < 1440;
    const bool needBreak = breakWindow &&
                           m_breakRefreshDates.value(market) != date;
    const bool needClose = closeWindow &&
                           m_closedRefreshDates.value(market) != date;
    if (needBreak || needClose) {
      if (!markets.contains(market))
        markets.push_back(market);
    }
  }
  for (const auto &market : markets) {
    const auto nowUtc = QDateTime::currentDateTimeUtc();
    const auto local = nowUtc.toTimeZone(MarketRules::timeZone(market));
    const QDate date = local.date();
    const int minute = local.time().hour() * 60 + local.time().minute();
    const int breakStart = market == QStringLiteral("HK") ? 720 : 690;
    if (minute >= breakStart && minute < 780)
      m_breakRefreshDates.insert(market, date);
    else
      m_closedRefreshDates.insert(market, date);
    requestRefresh(true, false);
    return;
  }
}

void AppController::requestRefresh(bool force, bool showProgress) {
  if (m_shuttingDown)
    return;
  if (m_model.positions().isEmpty() || (m_settings.paused && !force)) {
    if (m_model.positions().isEmpty())
      setStatus(m_store.loadError().isEmpty()
                    ? QStringLiteral("添加自选开始")
                    : m_store.loadError(),
                m_store.loadError().isEmpty() ? QStringLiteral("#657582")
                                              : QStringLiteral("#E88B00"));
    return;
  }
  if (m_inflightBatches >= 1) {
    m_pendingRefresh = true;
    m_pendingForce = m_pendingForce || force;
    m_pendingProgress = m_pendingProgress || showProgress;
    if (showProgress)
      setStatus(QStringLiteral("更新中"), QStringLiteral("#E88B00"));
    return;
  }
  const double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
  QVector<QuoteRequest> requests;
  for (const auto &item : m_model.positions()) {
    if ((force || m_market.isMarketOpen(item.symbol)) &&
        (force || item.retryAfter <= now))
      requests.push_back({item.id, item.symbol});
  }
  if (requests.isEmpty()) {
    setStatus(marketSummary(), QStringLiteral("#657582"));
    return;
  }
  const int batchId = ++m_nextBatchId;
  ++m_inflightBatches;
  if (showProgress)
    setStatus(QStringLiteral("更新中"), QStringLiteral("#E88B00"));
  m_market.requestQuotes(requests, batchId);
}

void AppController::applyQuoteBatch(int batchId,
                                    const QVector<QuoteResult> &results) {
  if (m_shuttingDown)
    return;
  m_inflightBatches = std::max(0, m_inflightBatches - 1);
  bool changed = false;
  for (const auto &result : results) {
    auto *item = m_model.find(result.positionId);
    if (!item || item->symbol != result.symbol)
      continue;
    const int lastResult = m_lastResultBatch.value(item->id);
    if (result.success) {
      if (batchId <= m_lastSuccessBatch.value(item->id))
        continue;
      item->price = result.price;
      item->currency = result.currency;
      item->precision = result.precision;
      item->source = result.source;
      if (!item->customName && !result.name.isEmpty())
        item->name = result.name;
      item->changePercent = result.changePercent;
      item->previousClose = result.previousClose;
      item->hasChange = result.hasChange;
      item->updatedAt = QDateTime::currentMSecsSinceEpoch() / 1000.0;
      IntradaySeries::appendLiveQuote(
          *item, result,
          m_market.isTradingDay(item->symbol, m_market.marketDate(item->symbol)));      item->error.clear();
      item->failureCount = 0;
      item->retryAfter = 0.0;
      m_lastSuccessBatch[item->id] = batchId;
    } else {
      if (batchId <= lastResult)
        continue;
      item->error = result.error;
      ++item->failureCount;
      item->retryAfter =
          QDateTime::currentMSecsSinceEpoch() / 1000.0 +
          std::min(15.0 * std::pow(2.0, std::max(0, item->failureCount - 1)),
                   900.0);
    }
    m_lastResultBatch[item->id] = std::max(lastResult, batchId);
    m_model.notifyPosition(item->id, {PositionModel::NameRole,
                                      PositionModel::PriceTextRole,
                                      PositionModel::ErrorRole,
                                      PositionModel::PriceRole,
                                      PositionModel::ChangePercentRole,
                                      PositionModel::PreviousCloseRole,
                                      PositionModel::HasChangeRole,
                                      PositionModel::IntradayRole});
    changed = true;
  }
  if (changed) {
    scheduleSave();
    emit focusChanged();
    bool hasError = false, open = false;
    for (const auto &item : m_model.positions()) {
      hasError = hasError || !item.error.isEmpty();
      open = open || m_market.isMarketOpen(item.symbol);
    }
    setStatus(marketSummary(), hasError ? QStringLiteral("#E88B00")
                               : open   ? QStringLiteral("#30B96A")
                                        : QStringLiteral("#657582"));
  }
  if (m_pendingRefresh && m_inflightBatches < 1) {
    const bool force = m_pendingForce, progress = m_pendingProgress;
    m_pendingRefresh = m_pendingForce = m_pendingProgress = false;
    requestRefresh(force, progress);
  }
}

void AppController::refreshIntradayHistory() {
  QVector<QuoteRequest> requests;
  requests.reserve(m_model.positions().size());
  for (const auto &item : m_model.positions())
    requests.push_back({item.id, item.symbol});
  if (requests.isEmpty())
    return;
  const int batchId = ++m_nextBatchId;
  m_market.requestIntraday(requests, batchId);
}

void AppController::applyIntradayBatch(int batchId,
                                       const QVector<IntradayResult> &results) {
  if (m_shuttingDown)
    return;
  for (const auto &result : results) {
    if (!result.success || !result.tradingDate.isValid())
      continue;
    auto *item = m_model.find(result.positionId);
    if (!item || item->symbol != result.symbol ||
        batchId <= m_lastIntradayBatch.value(item->id) ||
        result.tradingDate < item->intradayDate)
      continue;
    if (!IntradaySeries::mergeHistory(*item, result))
      continue;    m_lastIntradayBatch[item->id] = batchId;
    m_model.notifyPosition(item->id, {PositionModel::IntradayRole});
  }
}

QString AppController::marketSummary() const {
  QStringList statuses, markets;
  for (const auto &item : m_model.positions()) {
    const QString market = MarketRules::marketKey(item.symbol);
    if (!markets.contains(market)) {
      markets.push_back(market);
      statuses.push_back(m_market.marketStatus(item.symbol));
    }
  }
  return (statuses.isEmpty() ? QStringLiteral("市场状态")
                             : statuses.join(QLatin1Char('/'))) +
         QStringLiteral(" · ") +
         QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
}

void AppController::sampleTheme(int x, int y, int width, int height) {
  if (!autoTheme())
    return;
  QList<int> reds, greens, blues;
  const int margin = 18;
  const QList<QPoint> points{{x - margin, y + height * 3 / 10},
                             {x + width + margin, y + height * 3 / 10},
                             {x + width * 3 / 10, y - margin},
                             {x + width * 3 / 10, y + height + margin},
                             {x - margin, y + height * 7 / 10},
                             {x + width + margin, y + height * 7 / 10}};
  for (const QPoint &point : points) {
    QScreen *screen = QGuiApplication::screenAt(point);
    if (!screen)
      continue;
    const QImage image =
        screen
            ->grabWindow(0, point.x() - screen->geometry().x(),
                         point.y() - screen->geometry().y(), 1, 1)
            .toImage();
    if (image.isNull())
      continue;
    const QColor color = image.pixelColor(0, 0);
    reds.push_back(color.red());
    greens.push_back(color.green());
    blues.push_back(color.blue());
  }
  if (reds.isEmpty())
    return;
  auto median = [](QList<int> values) {
    std::sort(values.begin(), values.end());
    return values.at(values.size() / 2);
  };
  const QColor sampled(median(reds), median(greens), median(blues));
  if (!m_underlayColor ||
      std::abs(sampled.red() - m_underlayColor->red()) +
              std::abs(sampled.green() - m_underlayColor->green()) +
              std::abs(sampled.blue() - m_underlayColor->blue()) >=
          18) {
    m_underlayColor = sampled;
    emit paletteChanged();
  }
}

void AppController::setStatus(const QString &text, const QString &color) {
  if (m_statusText == text && m_statusColor == color)
    return;
  m_statusText = text;
  m_statusColor = color;
  emit statusChanged();
}
bool AppController::saveStore() {
  if (m_store.save(m_model.positions(), m_settings))
    return true;
  setStatus(QStringLiteral("数据保存失败，请检查数据目录权限"),
            QStringLiteral("#E88B00"));
  return false;
}
void AppController::scheduleSave() {
  if (!m_saveTimer.isActive())
    m_saveTimer.start();
}
void AppController::syncTray() {
  m_windows.setTrayState(floating(), paused(), autostart());
}
void AppController::saveAndShutdown() {
  if (m_shuttingDown)
    return;
  m_shuttingDown = true;
  m_timer.stop();
  saveStore();
  m_market.abortAll();
  emit shutdownRequested();
}
