#include "MianA.Market/QuoteProviders.h"
#include "MianA.Core/MarketRules.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimeZone>
#include <cmath>
#include <stdexcept>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
bool validPrice(double price) { return std::isfinite(price) && price > 0.0; }

qint64 providerTimestamp(QStringList parts, const QString &market) {
  if (parts.size() <= 30)
    return 0;
  static const QRegularExpression nonDigits(QStringLiteral("[^0-9]"));
  QString raw = parts.at(30);
  if (parts.size() > 31 && raw.size() < 14)
    raw += parts.at(31);
  raw.remove(nonDigits);
  if (raw.size() < 14)
    return 0;
  QDateTime value =
      QDateTime::fromString(raw.first(14), QStringLiteral("yyyyMMddHHmmss"));
  value.setTimeZone(MarketRules::timeZone(market));
  return value.isValid() ? value.toSecsSinceEpoch() : 0;
}

QuoteResult baseResult(QuoteProviders::Provider provider,
                       const QString &positionId, const QString &symbol) {
  QuoteResult result;
  result.positionId = positionId;
  result.symbol = symbol;
  result.precision = MarketRules::pricePrecision(symbol);
  result.currency = MarketRules::currency(symbol);
  result.source = QuoteProviders::name(provider);
  return result;
}
} // namespace

namespace QuoteProviders {
QString name(Provider provider) {
  switch (provider) {
  case Provider::Tencent: return QStringLiteral("腾讯");
  case Provider::Sina: return QStringLiteral("新浪");
  case Provider::EastMoney: return QStringLiteral("东方财富");
  case Provider::Yahoo: return QStringLiteral("Yahoo");
  }
  return {};
}

QString queryKey(Provider provider, const QString &symbol) {
  const QString code = symbol.section(QLatin1Char('.'), 0, 0);
  const QString suffix = symbol.section(QLatin1Char('.'), 1, 1);
  if (provider == Provider::Tencent) {
    const QString market = MarketRules::marketKey(symbol);
    if (market == QStringLiteral("CN"))
      return (suffix == QStringLiteral("SS") ? QStringLiteral("sh")
              : suffix == QStringLiteral("SZ") ? QStringLiteral("sz")
                                                : QStringLiteral("bj")) + code;
    if (market == QStringLiteral("HK"))
      return QStringLiteral("r_hk") + code.rightJustified(5, QLatin1Char('0'));
    QString ticker = MarketRules::normalizeSymbol(symbol);
    if (ticker.endsWith(QStringLiteral(".US")))
      ticker.chop(3);
    return QStringLiteral("us") + ticker;
  }
  if (provider == Provider::Yahoo) {
    QString ticker = MarketRules::normalizeSymbol(symbol);
    if (ticker.endsWith(QStringLiteral(".US"))) {
      ticker.chop(3);
      ticker.replace(QLatin1Char('.'), QLatin1Char('-'));
    }
    return ticker;
  }
  if (provider == Provider::EastMoney) {
    const QString prefix = symbol.endsWith(QStringLiteral(".SS"))
                               ? QStringLiteral("1") : QStringLiteral("0");
    return prefix + QLatin1Char('.') + code;
  }
  return (suffix == QStringLiteral("SS") ? QStringLiteral("sh")
          : suffix == QStringLiteral("SZ") ? QStringLiteral("sz")
                                            : QStringLiteral("bj")) + code;
}

QString decodeChinesePayload(const QByteArray &body) {
#ifdef Q_OS_WIN
  const int length = MultiByteToWideChar(936, 0, body.constData(),
                                         int(body.size()), nullptr, 0);
  if (length > 0) {
    std::wstring decoded(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(936, 0, body.constData(), int(body.size()),
                        decoded.data(), length);
    return QString::fromWCharArray(decoded.data(), length);
  }
#endif
  return QString::fromLocal8Bit(body);
}
QuoteResult parseReplyText(Provider provider, const QString &positionId,
                           const QString &symbol, const QString &text) {
  QuoteResult result = baseResult(provider, positionId, symbol);
  const QString needle =
      (provider == Provider::Tencent ? QStringLiteral("v_")
                                     : QStringLiteral("hq_str_")) +
      queryKey(provider, symbol) + QStringLiteral("=\"");
  const int start = text.indexOf(needle);
  if (start < 0)
    throw std::runtime_error("missing provider payload");
  const int valueStart = start + needle.size();
  const int end = text.indexOf(QLatin1Char('"'), valueStart);
  if (end < 0)
    throw std::runtime_error("invalid provider payload");
  const QStringList parts = text.sliced(valueStart, end - valueStart)
                                .split(provider == Provider::Tencent
                                           ? QLatin1Char('~') : QLatin1Char(','));
  if (parts.size() <= 3)
    throw std::runtime_error("missing quote price");
  bool priceOk = false;
  result.price = parts.at(3).toDouble(&priceOk);
  if (!priceOk || !validPrice(result.price))
    throw std::runtime_error("invalid quote price");
  const int closeIndex = provider == Provider::Tencent ? 4 : 2;
  bool closeOk = false;
  const double previousClose = parts.value(closeIndex).toDouble(&closeOk);
  if (closeOk && validPrice(previousClose)) {
    result.previousClose = previousClose;
    result.changePercent =
        (result.price - previousClose) / previousClose * 100.0;
    result.hasChange = true;
  }
  result.name = provider == Provider::Tencent ? parts.value(1) : parts.value(0);
  result.marketTimestamp =
      providerTimestamp(parts, MarketRules::marketKey(symbol));
  result.success = true;
  return result;
}

QuoteResult parseReply(Provider provider, const QString &positionId,
                       const QString &symbol, const QByteArray &body) {
  if (provider == Provider::Tencent || provider == Provider::Sina)
    return parseReplyText(provider, positionId, symbol,
                          decodeChinesePayload(body));

  QuoteResult result = baseResult(provider, positionId, symbol);
  if (provider == Provider::EastMoney) {
    const auto data = QJsonDocument::fromJson(body)
                          .object()
                          .value(QStringLiteral("data"))
                          .toObject();
    const int precision = data.value(QStringLiteral("f59")).toInt(2);
    result.price = data.value(QStringLiteral("f43")).toDouble() /
                   std::pow(10.0, precision);
    if (!validPrice(result.price))
      throw std::runtime_error("invalid quote price");
    const double previousClose =
        data.value(QStringLiteral("f60")).toDouble() /
        std::pow(10.0, precision);
    if (validPrice(previousClose)) {
      result.previousClose = previousClose;
      result.changePercent =
          (result.price - previousClose) / previousClose * 100.0;
      result.hasChange = true;
    }
    result.name = data.value(QStringLiteral("f58")).toString();
    result.marketTimestamp =
        data.value(QStringLiteral("f124")).toVariant().toLongLong();
    result.success = true;
    return result;
  }

  const auto values = QJsonDocument::fromJson(body)
                          .object()
                          .value(QStringLiteral("chart"))
                          .toObject()
                          .value(QStringLiteral("result"))
                          .toArray();
  if (values.isEmpty())
    throw std::runtime_error("empty Yahoo result");
  const auto root = values.at(0).toObject();
  const auto meta = root.value(QStringLiteral("meta")).toObject();
  result.price = meta.value(QStringLiteral("regularMarketPrice")).toDouble();
  if (!validPrice(result.price)) {
    const auto quoteArray = root.value(QStringLiteral("indicators"))
                                .toObject()
                                .value(QStringLiteral("quote"))
                                .toArray();
    const auto closes =
        quoteArray.isEmpty()
            ? QJsonArray{}
            : quoteArray.at(0)
                  .toObject()
                  .value(QStringLiteral("close"))
                  .toArray();
    for (qsizetype i = closes.size(); i > 0; --i) {
      const auto value = closes.at(i - 1);
      if (value.isDouble() && validPrice(value.toDouble())) {
        result.price = value.toDouble();
        break;
      }
    }
  }
  if (!validPrice(result.price))
    throw std::runtime_error("invalid quote price");
  result.currency =
      meta.value(QStringLiteral("currency")).toString(result.currency);
  result.name = meta.value(QStringLiteral("shortName")).toString();
  if (result.name.isEmpty())
    result.name = meta.value(QStringLiteral("longName")).toString();
  result.marketTimestamp =
      meta.value(QStringLiteral("regularMarketTime")).toVariant().toLongLong();
  const double previousClose =
      meta.value(QStringLiteral("chartPreviousClose")).toDouble();
  if (validPrice(previousClose)) {
    result.previousClose = previousClose;
    result.changePercent =
        (result.price - previousClose) / previousClose * 100.0;
    result.hasChange = true;
  }
  result.success = true;
  return result;
}
} // namespace QuoteProviders
