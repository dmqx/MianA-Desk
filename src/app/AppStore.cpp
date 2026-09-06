#include "AppStore.h"

#include "services/MarketService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace {
constexpr int DataSchemaVersion = 1;
}

AppStore::AppStore() : m_path(dataPath()) {}

bool AppStore::load(QVector<Position> &positions, AppSettings &settings) {
  m_loadError.clear();
  m_safeToOverwrite = true;
  QFile file(m_path);
  if (!file.exists())
    return true;
  if (!file.open(QIODevice::ReadOnly)) {
    m_loadError = file.errorString();
    m_safeToOverwrite = false;
    return false;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
  const auto root = document.object();
  if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
      root.value(QStringLiteral("schema_version")).toInt(-1) !=
          DataSchemaVersion ||
      !root.value(QStringLiteral("positions")).isArray() ||
      (!root.value(QStringLiteral("settings")).isUndefined() &&
       !root.value(QStringLiteral("settings")).isObject())) {
    return backupInvalidData(parseError.error == QJsonParseError::NoError
                                 ? QStringLiteral("不支持的数据格式")
                                 : parseError.errorString());
  }
  const auto raw = root.value(QStringLiteral("settings")).toObject();
  auto boolean = [&](const char *key, bool fallback) {
    const auto value = raw.value(QLatin1String(key));
    return value.isBool() ? value.toBool() : fallback;
  };
  auto integer = [&](const char *key, int fallback) {
    const auto value = raw.value(QLatin1String(key));
    return value.isDouble() && std::isfinite(value.toDouble())
               ? value.toInt(fallback)
               : fallback;
  };
  settings.x = integer("x", 80);
  settings.y = integer("y", 80);
  settings.topmost = boolean("topmost", true);
  settings.locked = boolean("locked", false);
  settings.compact = boolean("compact", false);
  settings.floating = boolean("floating", false);
  settings.showIntraday = boolean("show_intraday", true);
  settings.hideStockCode = boolean("hide_stock_code", false);
  settings.focusId = raw.value(QStringLiteral("focus_id")).toString();
  settings.paused = boolean("paused", false);
  settings.minimizeToTray = boolean("minimize_to_tray", true);
  settings.autoTheme = boolean("auto_theme", false);
  const int legacyOpacity = std::clamp(integer("theme_opacity", 95), 40, 95);
  settings.frameOpacity = std::clamp(integer("frame_opacity", legacyOpacity), 10, 100);
  settings.textOpacity = std::clamp(integer("text_opacity", legacyOpacity), 10, 100);
  const QString color =
      raw.value(QStringLiteral("theme_color")).toString().toUpper();
  static const QRegularExpression hexColor(QStringLiteral("^#[0-9A-F]{6}$"));
  settings.themeColor =
      hexColor.match(color).hasMatch() ? color : QStringLiteral("#FFFFFF");

  QVector<Position> loaded;
  QSet<QString> ids;
  const auto values = root.value(QStringLiteral("positions")).toArray();
  loaded.reserve(values.size());
  bool discarded = false;
  for (const auto &value : values) {
    if (!value.isObject()) {
      discarded = true;
      continue;
    }
    bool valid = false;
    auto item = parsePosition(value.toObject(), valid);
    if (!valid) {
      discarded = true;
      continue;
    }
    if (ids.contains(item.id))
      item.id = QUuid::createUuid().toString(QUuid::Id128);
    ids.insert(item.id);
    loaded.push_back(std::move(item));
  }
  if (!ids.contains(settings.focusId))
    settings.focusId = loaded.isEmpty() ? QString{} : loaded.constFirst().id;
  positions = std::move(loaded);
  return discarded ? backupInvalidData(QStringLiteral("部分自选数据无效")) : true;
}

bool AppStore::save(const QVector<Position> &positions,
                    const AppSettings &settings) {
  if (!m_safeToOverwrite)
    return false;
  const QJsonObject rawSettings{
      {QStringLiteral("x"), settings.x},
      {QStringLiteral("y"), settings.y},
      {QStringLiteral("topmost"), settings.topmost},
      {QStringLiteral("locked"), settings.locked},
      {QStringLiteral("compact"), settings.compact},
      {QStringLiteral("floating"), settings.floating},
      {QStringLiteral("show_intraday"), settings.showIntraday},
      {QStringLiteral("hide_stock_code"), settings.hideStockCode},
      {QStringLiteral("focus_id"), settings.focusId},
      {QStringLiteral("paused"), settings.paused},
      {QStringLiteral("minimize_to_tray"), settings.minimizeToTray},
      {QStringLiteral("auto_theme"), settings.autoTheme},
      {QStringLiteral("theme_color"), settings.themeColor},
      {QStringLiteral("frame_opacity"), settings.frameOpacity},
      {QStringLiteral("text_opacity"), settings.textOpacity}};
  QJsonArray rawPositions;
  for (const auto &item : positions)
    rawPositions.append(
        QJsonObject{{QStringLiteral("id"), item.id},
                    {QStringLiteral("symbol"), item.symbol},
                    {QStringLiteral("name"), item.name},
                    {QStringLiteral("price"), item.price},
                    {QStringLiteral("currency"), item.currency},
                    {QStringLiteral("precision"), item.precision},
                    {QStringLiteral("custom_name"), item.customName},
                    {QStringLiteral("updated_at"), item.updatedAt},
                    {QStringLiteral("error"), item.error},
                    {QStringLiteral("source"), item.source},
                    {QStringLiteral("failure_count"), item.failureCount},
                    {QStringLiteral("retry_after"), item.retryAfter}});
  const QJsonDocument document(
      QJsonObject{{QStringLiteral("schema_version"), DataSchemaVersion},
                  {QStringLiteral("settings"), rawSettings},
                  {QStringLiteral("positions"), rawPositions}});
  QDir().mkpath(QFileInfo(m_path).absolutePath());
  QSaveFile file(m_path);
  const QByteArray bytes = document.toJson(QJsonDocument::Indented);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(bytes) != bytes.size()) {
    file.cancelWriting();
    return false;
  }
  return file.commit();
}

bool AppStore::backupInvalidData(const QString &reason) {
  const QString backup =
      m_path + QStringLiteral(".invalid-%1-%2.bak")
                   .arg(QDateTime::currentMSecsSinceEpoch())
                   .arg(QUuid::createUuid().toString(QUuid::Id128));
  m_safeToOverwrite = QFile::copy(m_path, backup);
  m_loadError = (m_safeToOverwrite
                    ? QStringLiteral("数据文件异常，原文件已备份：")
                    : QStringLiteral("数据文件异常，备份失败，已禁止覆盖：")) + reason;
  return false;
}

QString AppStore::loadError() const { return m_loadError; }
QString AppStore::dataPath() {
  const QString overridePath =
      qEnvironmentVariable("MIANA_DESK_DATA_DIR").trimmed();
  if (!overridePath.isEmpty())
    return QDir(overridePath).filePath(QStringLiteral("data.json"));
  QString root = qEnvironmentVariable("APPDATA");
  if (root.isEmpty())
    root = QDir::homePath() + QStringLiteral("/AppData/Roaming");
  return QDir(root).filePath(QStringLiteral("MianA Desk/data.json"));
}

Position AppStore::parsePosition(const QJsonObject &object, bool &valid) {
  Position item;
  item.symbol = MarketService::normalizeSymbol(
      object.value(QStringLiteral("symbol")).toString());
  item.price = object.value(QStringLiteral("price")).toDouble();
  item.updatedAt = object.value(QStringLiteral("updated_at")).toDouble();
  item.retryAfter = object.value(QStringLiteral("retry_after")).toDouble();
  valid = !item.symbol.isEmpty() && item.price >= 0.0 &&
          std::isfinite(item.price) && std::isfinite(item.updatedAt) &&
          std::isfinite(item.retryAfter);
  if (!valid)
    return item;
  item.id = object.value(QStringLiteral("id")).toString();
  if (item.id.isEmpty())
    item.id = QUuid::createUuid().toString(QUuid::Id128);
  item.name =
      object.value(QStringLiteral("name")).toString().trimmed().left(80);
  if (item.name.isEmpty())
    item.name = item.symbol;
  item.currency = object.value(QStringLiteral("currency"))
                      .toString(QStringLiteral("CNY"))
                      .toUpper()
                      .left(8);
  item.precision = MarketService::pricePrecision(item.symbol);
  item.customName = object.value(QStringLiteral("custom_name")).toBool(false);
  item.error = object.value(QStringLiteral("error")).toString().left(240);
  item.source = object.value(QStringLiteral("source")).toString().left(40);
  item.failureCount =
      std::clamp(object.value(QStringLiteral("failure_count")).toInt(), 0, 20);
  item.updatedAt = std::max(0.0, item.updatedAt);
  item.retryAfter = std::max(0.0, item.retryAfter);
  return item;
}
