#include "AppController.h"

#include "WindowManager.h"

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
} // namespace

AppController::AppController(WindowManager &windows, QObject *parent)
    : QObject(parent), m_windows(windows), m_model(this), m_market(this),
      m_updater(this), m_timer(this) {
  QVector<Position> loaded;
  if (!m_store.load(loaded, m_settings)) {
    m_statusText = QStringLiteral("数据文件异常，原文件已备份");
    m_statusColor = QStringLiteral("#E88B00");
  }
  m_model.replaceAll(std::move(loaded));
  connect(&m_market, &MarketService::batchReady, this,
          &AppController::applyQuoteBatch);
  connect(&m_timer, &QTimer::timeout, this, [this] { refreshQuotes(false); });
  connect(&m_windows, &WindowManager::showRequested, this,
          &AppController::showFull);
  connect(&m_windows, &WindowManager::hotkeyActivated, this,
          &AppController::toggleMainRequested);
  m_saveTimer.setSingleShot(true);
  m_saveTimer.setInterval(10000);
  connect(&m_saveTimer, &QTimer::timeout, this, [this] { saveStore(); });
  connect(&m_updater, &UpdateChecker::updateAvailable, this,
          [this](const QString &tagName, const QUrl &url) {
            m_windows.showUpdateNotification(tagName, url);
            setStatus(QStringLiteral("发现新版本 %1").arg(tagName),
                      QStringLiteral("#30B96A"));
          });
  connect(&m_updater, &UpdateChecker::upToDate, this, [this] {
    if (m_checkUpdatesNotify)
      m_windows.showTrayMessage(QStringLiteral("MianA Desk"),
                                QStringLiteral("已是最新版本"));
  });
  connect(&m_updater, &UpdateChecker::checkFailed, this, [this] {
    if (m_checkUpdatesNotify)
      m_windows.showTrayMessage(QStringLiteral("MianA Desk"),
                                QStringLiteral("检查更新失败，请检查网络后重试"));
  });
  if (!m_settings.paused)
    m_timer.start(800);
  QTimer::singleShot(500, this, &AppController::initialRefresh);
  QTimer::singleShot(5000, this, [this] { checkForUpdates(false); });
}

QObject *AppController::positions() { return &m_model; }
QString AppController::iconUrl() const {
  return QStringLiteral("qrc:/qt/qml/MianA/assets/miana_desk.png");
}
QString AppController::statusText() const { return m_statusText; }
QString AppController::statusColor() const { return m_statusColor; }
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
QString AppController::themeColor() const {
  return m_preview ? m_preview->color : m_settings.themeColor;
}
int AppController::themeOpacity() const {
  return m_preview ? m_preview->opacity : m_settings.themeOpacity;
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
QString AppController::focusedPrice() const {
  const auto *item = focused();
  return item && item->price > 0.0
             ? PositionModel::moneySymbol(item->currency) +
                   PositionModel::formatPrice(item->price, item->precision)
             : QStringLiteral("--");
}

QHash<QString, QString> AppController::palette() const {
  QColor base =
      autoTheme() && m_underlayColor ? *m_underlayColor : QColor(themeColor());
  if (!base.isValid())
    base = Qt::white;
  const double luminance =
      base.red() * .299 + base.green() * .587 + base.blue() * .114;
  if (luminance < 135)
    return {{QStringLiteral("bg"), blend(base, Qt::white, .08)},
            {QStringLiteral("panel"), blend(base, Qt::white, .15)},
            {QStringLiteral("hover"), blend(base, Qt::white, .23)},
            {QStringLiteral("line"), blend(base, Qt::white, .38)},
            {QStringLiteral("text"), QStringLiteral("#F5F8FA")},
            {QStringLiteral("muted"), QStringLiteral("#CDD7DE")},
            {QStringLiteral("hint"), QStringLiteral("#AAB8C2")}};
  return {{QStringLiteral("bg"), blend(base, Qt::white, .18)},
          {QStringLiteral("panel"), blend(base, Qt::white, .32)},
          {QStringLiteral("hover"), blend(base, Qt::white, .43)},
          {QStringLiteral("line"), blend(base, Qt::white, .68)},
          {QStringLiteral("text"), QStringLiteral("#18232D")},
          {QStringLiteral("muted"), QStringLiteral("#586976")},
          {QStringLiteral("hint"), QStringLiteral("#758691")}};
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
  const QString symbol = MarketService::normalizeSymbol(rawSymbol);
  if (symbol.isEmpty())
    return QStringLiteral("请输入股票代码");
  if (m_model.containsSymbol(symbol))
    return QStringLiteral("该代码已在自选中");
  const QString market = MarketService::marketKey(symbol);
  Position item;
  item.id = QUuid::createUuid().toString(QUuid::Id128);
  item.symbol = symbol;
  item.name = rawName.trimmed().isEmpty() ? symbol : rawName.trimmed().left(80);
  item.customName = !rawName.trimmed().isEmpty();
  item.precision = MarketService::pricePrecision(symbol);
  item.currency = market == QStringLiteral("CN")   ? QStringLiteral("CNY")
                  : market == QStringLiteral("HK") ? QStringLiteral("HKD")
                                                   : QStringLiteral("USD");
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
  return {};
}

QString AppController::editPosition(const QString &positionId,
                                    const QString &rawSymbol,
                                    const QString &rawName) {
  auto *item = m_model.find(positionId);
  const QString symbol = MarketService::normalizeSymbol(rawSymbol);
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
    const QString market = MarketService::marketKey(symbol);
    item->currency = market == QStringLiteral("CN")   ? QStringLiteral("CNY")
                     : market == QStringLiteral("HK") ? QStringLiteral("HKD")
                                                      : QStringLiteral("USD");
    item->precision = MarketService::pricePrecision(symbol);
  }
  if (!saveStore()) {
    *item = previous;
    return QStringLiteral("数据保存失败，请检查数据目录权限");
  }
  m_model.notifyPosition(positionId);
  emit focusChanged();
  requestRefresh(true, false);
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
  emit stateChanged();
  emit focusChanged();
}

void AppController::movePosition(int source, int destination) {
  const auto previous = m_model.positions();
  if (m_model.movePosition(source, destination) && !saveStore())
    m_model.replaceAll(previous);
}

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
                                      int opacity) {
  QColor normalized(color);
  const QString safe = normalized.isValid() ? normalized.name().toUpper()
                                            : m_settings.themeColor;
  m_preview = Appearance{automatic, safe, std::clamp(opacity, 40, 95)};
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
  syncTray();
  if (key == QStringLiteral("paused")) {
    if (m_settings.paused) {
      m_timer.stop();
      setStatus(QStringLiteral("行情已暂停"), QStringLiteral("#657582"));
    } else {
      m_timer.start(800);
      setStatus(QStringLiteral("等待刷新"), QStringLiteral("#657582"));
      requestRefresh(true, false);
    }
  }
}

bool AppController::saveSettings(bool pausedValue, bool floatingValue,
                                 bool tray, bool autostartValue, bool automatic,
                                 const QString &color, int opacity) {
  const AppSettings previous = m_settings;
  const bool wasPaused = m_settings.paused;
  const bool wasAutostart = m_windows.autostartEnabled();
  QColor normalized(color);
  m_preview.reset();
  m_settings.paused = pausedValue;
  m_settings.floating = floatingValue;
  m_settings.minimizeToTray = tray;
  m_settings.autoTheme = automatic;
  m_settings.themeColor =
      normalized.isValid() ? normalized.name().toUpper() : previous.themeColor;
  m_settings.themeOpacity = std::clamp(opacity, 40, 95);
  if (!saveStore()) {
    m_settings = previous;
    emit stateChanged();
    emit paletteChanged();
    return false;
  }
  bool autostartOk = true;
  if (autostartValue != wasAutostart) {
    autostartOk = m_windows.setAutostart(autostartValue);
    if (!autostartOk)
      setStatus(QStringLiteral("开机启动设置失败，请检查系统权限"),
                QStringLiteral("#E88B00"));
  }
  emit paletteChanged();
  emit stateChanged();
  syncTray();
  if (wasPaused && !pausedValue)
    requestRefresh(true, false);
  return autostartOk;
}

void AppController::initialRefresh() { requestRefresh(true, true); }
void AppController::manualRefresh() { requestRefresh(true, true); }
void AppController::checkForUpdates(bool notifyIfCurrent) {
  m_checkUpdatesNotify = notifyIfCurrent;
  if (notifyIfCurrent)
    setStatus(QStringLiteral("正在检查更新…"), QStringLiteral("#657582"));
  m_updater.check();
}
void AppController::refreshQuotes(bool force) { requestRefresh(force, false); }

void AppController::requestRefresh(bool force, bool showProgress) {
  if (m_shuttingDown)
    return;
  if (m_model.positions().isEmpty() || (m_settings.paused && !force)) {
    if (m_model.positions().isEmpty())
      setStatus(m_store.loadError().isEmpty()
                    ? QStringLiteral("添加自选开始")
                    : QStringLiteral("数据文件异常，原文件已备份"),
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
      item->updatedAt = QDateTime::currentMSecsSinceEpoch() / 1000.0;
      item->error.clear();
      item->failureCount = 0;
      item->retryAfter = 0.0;
      m_lastSuccessBatch[item->id] = batchId;
    } else {
      if (batchId < lastResult)
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
                                      PositionModel::ErrorRole});
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

QString AppController::marketSummary() const {
  QStringList statuses, markets;
  for (const auto &item : m_model.positions()) {
    const QString market = MarketService::marketKey(item.symbol);
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
