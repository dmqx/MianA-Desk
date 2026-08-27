#include "MarketService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrl>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

struct MarketService::Pending {
  QString positionId;
  QString symbol;
  QVector<Provider> providers;
  int providerIndex = 0;
  bool inFlight = false;
  QStringList errors;
};
struct MarketService::Batch {
  int batchId = 0;
  int expected = 0;
  int resolved = 0;
  QVector<QuoteResult> results;
  QHash<QString, int> resultIndex;
  QVector<std::shared_ptr<Pending>> pendings;
};

namespace {
QString providerName(MarketService::Provider provider) {
  switch (provider) {
  case MarketService::Provider::Tencent:
    return QStringLiteral("腾讯");
  case MarketService::Provider::Sina:
    return QStringLiteral("新浪");
  case MarketService::Provider::EastMoney:
    return QStringLiteral("东方财富");
  case MarketService::Provider::Yahoo:
    return QStringLiteral("Yahoo");
  }
  return {};
}
QString chinaQuerySymbol(const QString &symbol) {
  const QString code = symbol.section(QLatin1Char('.'), 0, 0);
  const QString suffix = symbol.section(QLatin1Char('.'), 1, 1);
  return (suffix == QStringLiteral("SS")   ? QStringLiteral("sh")
          : suffix == QStringLiteral("SZ") ? QStringLiteral("sz")
                                           : QStringLiteral("bj")) +
         code;
}
qint64 providerTimestamp(QStringList parts, const QString &market) {
  if (parts.size() <= 30)
    return 0;
  static const QRegularExpression nonDigits(QStringLiteral("[^0-9]"));
  // Tencent packs a full "yyyyMMddHHmmss" into field 30 while Sina splits
  // the date (field 30) and time (field 31) into two separate fields.
  QString raw = parts.at(30);
  if (parts.size() > 31 && raw.size() < 14)
    raw += parts.at(31);
  raw.remove(nonDigits);
  if (raw.size() < 14)
    return 0;
  static const QTimeZone newYork(QByteArrayLiteral("America/New_York"));
  static const QTimeZone hongKong(QByteArrayLiteral("Asia/Hong_Kong"));
  static const QTimeZone shanghai(QByteArrayLiteral("Asia/Shanghai"));
  QDateTime value =
      QDateTime::fromString(raw.left(14), QStringLiteral("yyyyMMddHHmmss"));
  value.setTimeZone(market == QStringLiteral("US")   ? newYork
                   : market == QStringLiteral("HK") ? hongKong
                                                    : shanghai);
  return value.isValid() ? value.toSecsSinceEpoch() : 0;
}
bool validPrice(double price) { return std::isfinite(price) && price > 0.0; }
const QTimeZone &marketTimeZone(const QString &market) {
  static const QTimeZone newYork(QByteArrayLiteral("America/New_York"));
  static const QTimeZone hongKong(QByteArrayLiteral("Asia/Hong_Kong"));
  static const QTimeZone shanghai(QByteArrayLiteral("Asia/Shanghai"));
  return market == QStringLiteral("US")   ? newYork
         : market == QStringLiteral("HK") ? hongKong
                                           : shanghai;
}
QString decodeChinesePayload(const QByteArray &body) {
#ifdef Q_OS_WIN
  const int length =
      MultiByteToWideChar(936, 0, body.constData(), body.size(), nullptr, 0);
  if (length > 0) {
    std::wstring decoded(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(936, 0, body.constData(), body.size(), decoded.data(),
                        length);
    return QString::fromWCharArray(decoded.data(), length);
  }
#endif
  return QString::fromLocal8Bit(body);
}
} // namespace

MarketService::MarketService(QObject *parent)
    : QObject(parent), m_network(this) {}

QString MarketService::normalizeSymbol(const QString &raw) {
  QString value = raw.trimmed().toUpper();
  value.remove(QLatin1Char(' '));
  const QList<QPair<QString, QString>> suffixes{
      {QStringLiteral(".XSHG"), QStringLiteral(".SS")},
      {QStringLiteral(".SH"), QStringLiteral(".SS")},
      {QStringLiteral(".XSHE"), QStringLiteral(".SZ")},
      {QStringLiteral(".SHE"), QStringLiteral(".SZ")}};
  for (const auto &[from, to] : suffixes)
    if (value.endsWith(from))
      return value.left(value.size() - from.size()) + to;
  if (value.contains(QLatin1Char('.')) || value.contains(QLatin1Char('=')) ||
      value.contains(QLatin1Char('^')))
    return value;
  static const QRegularExpression sixDigits(QStringLiteral("^[0-9]{6}$"));
  if (sixDigits.match(value).hasMatch()) {
    if (QStringLiteral("659").contains(value.front()))
      return value + QStringLiteral(".SS");
    if (QStringLiteral("0123").contains(value.front()))
      return value + QStringLiteral(".SZ");
    if (QStringLiteral("48").contains(value.front()))
      return value + QStringLiteral(".BJ");
  }
  return value;
}

QString MarketService::marketKey(const QString &symbol) {
  const QString upper = symbol.toUpper();
  if (upper.endsWith(QStringLiteral(".SS")) ||
      upper.endsWith(QStringLiteral(".SZ")) ||
      upper.endsWith(QStringLiteral(".BJ")))
    return QStringLiteral("CN");
  if (upper.endsWith(QStringLiteral(".HK")))
    return QStringLiteral("HK");
  if (upper.endsWith(QStringLiteral(".US")))
    return QStringLiteral("US");
  // Codes without a recognized suffix (e.g. a raw six-digit A-share
  // code that could not be normalized) default to the A-share market
  // instead of being reported as a US instrument.
  return QStringLiteral("CN");
}

int MarketService::pricePrecision(const QString &symbol) {
  const QString value = normalizeSymbol(symbol);
  const QString code = value.section(QLatin1Char('.'), 0, 0);
  if (code.isEmpty())
    return 2;
  if (value.endsWith(QStringLiteral(".SS")))
    return code.startsWith(QLatin1Char('6')) ? 2 : 3;
  if (value.endsWith(QStringLiteral(".SZ")))
    return QStringLiteral("023").contains(code.front()) ? 2 : 3;
  return value.endsWith(QStringLiteral(".HK")) ? 3 : 2;
}

QDateTime MarketService::marketNow(const QString &market) const {
  return QDateTime::currentDateTimeUtc()
      .toTimeZone(marketTimeZone(market));
}

bool MarketService::isMarketOpen(const QString &symbol) const {
  const QString market = marketKey(symbol);
  const auto now = marketNow(market);
  if (now.date().dayOfWeek() >= Qt::Saturday)
    return false;
  const auto state = m_tradingStates.value(market);
  if (state.localDate == now.date() &&
      state.expiresAt > QDateTime::currentSecsSinceEpoch() && !state.tradingDay)
    return false;
  const int minute = now.time().hour() * 60 + now.time().minute();
  if (market == QStringLiteral("CN"))
    return (minute >= 570 && minute < 690) || (minute >= 780 && minute < 900);
  if (market == QStringLiteral("HK"))
    return (minute >= 570 && minute < 720) || (minute >= 780 && minute < 960);
  return minute >= 570 && minute < 960;
}

QString MarketService::marketStatus(const QString &symbol) const {
  const QString market = marketKey(symbol);
  const QString label = market == QStringLiteral("CN") ? QStringLiteral("A股")
                        : market == QStringLiteral("HK")
                            ? QStringLiteral("港股")
                            : QStringLiteral("美股");
  const auto now = marketNow(market);
  const auto state = m_tradingStates.value(market);
  if (now.date().dayOfWeek() >= Qt::Saturday ||
      (state.localDate == now.date() &&
       state.expiresAt > QDateTime::currentSecsSinceEpoch() &&
       !state.tradingDay))
    return label + QStringLiteral("休市");
  const int minute = now.time().hour() * 60 + now.time().minute();
  if (isMarketOpen(symbol))
    return label + QStringLiteral("交易中");
  if ((market == QStringLiteral("CN") && minute >= 690 && minute < 780) ||
      (market == QStringLiteral("HK") && minute >= 720 && minute < 780))
    return label + QStringLiteral("午休");
  return label +
         (minute < 570 ? QStringLiteral("未开盘") : QStringLiteral("已收盘"));
}

void MarketService::requestQuotes(const QVector<QuoteRequest> &requests,
                                  int batchId) {
  if (requests.isEmpty()) {
    emit batchReady(batchId, {});
    return;
  }
  auto batch = std::make_shared<Batch>();
  batch->batchId = batchId;
  batch->expected = requests.size();
  batch->results.resize(requests.size());
  batch->pendings.reserve(requests.size());
  for (qsizetype i = 0; i < requests.size(); ++i) {
    const auto &request = requests.at(i);
    auto pending = std::make_shared<Pending>();
    pending->positionId = request.positionId;
    pending->symbol = request.symbol;
    pending->providers = providersFor(request.symbol);
    batch->resultIndex.insert(pending->positionId, int(i));
    batch->pendings.push_back(std::move(pending));
  }
  m_batches.insert(batchId, batch);
  dispatchNext(batch);
}

void MarketService::abortAll() {
  for (const auto &reply : std::as_const(m_replies))
    if (reply)
      reply->abort();
  m_replies.clear();
  m_batches.clear();
}

QVector<MarketService::Provider>
MarketService::providersFor(const QString &symbol) {
  return marketKey(symbol) == QStringLiteral("CN")
             ? QVector<Provider>{Provider::Tencent, Provider::Sina,
                                 Provider::EastMoney, Provider::Yahoo}
             : QVector<Provider>{Provider::Yahoo, Provider::Tencent};
}

QString MarketService::queryKey(Provider provider, const QString &symbol) {
  if (provider == Provider::Tencent) {
    const QString market = marketKey(symbol);
    if (market == QStringLiteral("CN"))
      return chinaQuerySymbol(symbol);
    if (market == QStringLiteral("HK"))
      return QStringLiteral("r_hk") + symbol.section(QLatin1Char('.'), 0, 0)
                                         .rightJustified(5, QLatin1Char('0'));
    return QStringLiteral("us") + symbol;
  }
  return chinaQuerySymbol(symbol);
}

QNetworkRequest MarketService::makeRequest(Provider provider,
                                           const QString &symbol) {
  QString url;
  if (provider == Provider::Tencent) {
    url = QStringLiteral("https://qt.gtimg.cn/q=%1")
              .arg(queryKey(provider, symbol));
  } else if (provider == Provider::Sina) {
    url = QStringLiteral("https://hq.sinajs.cn/list=%1")
              .arg(queryKey(provider, symbol));
  } else if (provider == Provider::EastMoney) {
    const QString code = symbol.section(QLatin1Char('.'), 0, 0);
    const QString prefix = symbol.endsWith(QStringLiteral(".SS"))
                               ? QStringLiteral("1")
                               : QStringLiteral("0");
    url = QStringLiteral("https://push2.eastmoney.com/api/qt/stock/"
                         "get?secid=%1.%2&fields=f43,f57,f58,f59,f124")
              .arg(prefix, code);
  } else {
    url = QStringLiteral("https://query1.finance.yahoo.com/v8/finance/chart/"
                         "%1?interval=1m&range=1d")
              .arg(QString::fromLatin1(QUrl::toPercentEncoding(symbol)));
  }
  QNetworkRequest request{QUrl(url)};
  request.setRawHeader(QByteArrayLiteral("User-Agent"),
                       QByteArrayLiteral("Mozilla/5.0 MianADesk/2.0"));
  if (provider == Provider::Tencent)
    request.setRawHeader(QByteArrayLiteral("Referer"),
                         QByteArrayLiteral("https://gu.qq.com/"));
  if (provider == Provider::Sina)
    request.setRawHeader(QByteArrayLiteral("Referer"),
                         QByteArrayLiteral("https://finance.sina.com.cn/"));
  request.setTransferTimeout(8000);
  return request;
}

QNetworkRequest MarketService::makeGroupedRequest(
    Provider provider, const QVector<std::shared_ptr<Pending>> &pendings) {
  QStringList keys;
  keys.reserve(pendings.size());
  for (const auto &pending : pendings)
    keys.push_back(queryKey(provider, pending->symbol));
  const QString joined = keys.join(QLatin1Char(','));
  QString url;
  if (provider == Provider::Tencent)
    url = QStringLiteral("https://qt.gtimg.cn/q=%1").arg(joined);
  else
    url = QStringLiteral("https://hq.sinajs.cn/list=%1").arg(joined);
  QNetworkRequest request{QUrl(url)};
  request.setRawHeader(QByteArrayLiteral("User-Agent"),
                       QByteArrayLiteral("Mozilla/5.0 MianADesk/2.0"));
  if (provider == Provider::Tencent)
    request.setRawHeader(QByteArrayLiteral("Referer"),
                         QByteArrayLiteral("https://gu.qq.com/"));
  else
    request.setRawHeader(QByteArrayLiteral("Referer"),
                         QByteArrayLiteral("https://finance.sina.com.cn/"));
  request.setTransferTimeout(8000);
  return request;
}

void MarketService::dispatchNext(const std::shared_ptr<Batch> &batch) {
  if (batch->resolved == batch->expected)
    return;
  QHash<int, QVector<std::shared_ptr<Pending>>> groups;
  for (const auto &pending : batch->pendings) {
    if (pending->inFlight)
      continue;
    if (pending->providerIndex < pending->providers.size())
      groups[int(pending->providers.at(pending->providerIndex))]
          .push_back(pending);
  }
  for (auto it = groups.begin(); it != groups.end(); ++it) {
    const Provider provider = static_cast<Provider>(it.key());
    auto &pendings = it.value();
    const bool batchable =
        provider == Provider::Tencent || provider == Provider::Sina;
    if (!batchable) {
      for (const auto &pending : pendings) {
        pending->inFlight = true;
        auto *reply = m_network.get(makeRequest(provider, pending->symbol));
        m_replies.push_back(reply);
        connect(reply, &QNetworkReply::finished, this,
                [this, batch, pending, reply, provider] {
                  handleGroupedReply(batch, {pending}, provider, reply);
                });
      }
      continue;
    }
    for (const auto &pending : pendings)
      pending->inFlight = true;
    auto *reply = m_network.get(makeGroupedRequest(provider, pendings));
    m_replies.push_back(reply);
    connect(reply, &QNetworkReply::finished, this,
            [this, batch, pendings, reply, provider] {
              handleGroupedReply(batch, pendings, provider, reply);
            });
  }
}

void MarketService::handleGroupedReply(
    const std::shared_ptr<Batch> &batch,
    const QVector<std::shared_ptr<Pending>> &pendings, Provider provider,
    QNetworkReply *reply) {
  m_replies.removeAll(reply);
  if (m_batches.value(batch->batchId) != batch) {
    reply->deleteLater();
    return;
  }
  const bool ok = reply->error() == QNetworkReply::NoError;
  const QByteArray body = ok ? reply->readAll() : QByteArray{};
  const QString error = reply->errorString();
  reply->deleteLater();
  // A grouped Tencent/Sina reply carries every symbol in one GBK text
  // payload: decode it once here instead of once per symbol.
  const QString groupedText =
      ok && (provider == Provider::Tencent || provider == Provider::Sina)
          ? decodeChinesePayload(body)
          : QString{};
  bool advanced = false;
  for (const auto &pending : pendings) {
    pending->inFlight = false;
    if (ok) {
      try {
        auto result =
            provider == Provider::Tencent || provider == Provider::Sina
                ? parseReplyText(provider, pending->positionId,
                                 pending->symbol, groupedText)
                : parseReply(provider, pending->positionId,
                             pending->symbol, body);
        rememberTradingState(result);
        resolvePending(batch, pending, std::move(result));
        continue;
      } catch (const std::exception &exception) {
        pending->errors.push_back(providerName(provider) +
                                  QStringLiteral(": ") +
                                  QString::fromUtf8(exception.what()));
      }
    } else {
      pending->errors.push_back(providerName(provider) +
                                QStringLiteral(": ") + error);
    }
    ++pending->providerIndex;
    advanced = true;
    if (pending->providerIndex >= pending->providers.size()) {
      QuoteResult result;
      result.positionId = pending->positionId;
      result.symbol = pending->symbol;
      const qsizetype firstError =
          std::max<qsizetype>(0, pending->errors.size() - 3);
      result.error =
          pending->errors.mid(firstError).join(QStringLiteral("；"));
      if (result.error.isEmpty())
        result.error = QStringLiteral("行情源暂不可用");
      resolvePending(batch, pending, std::move(result));
    }
  }
  if (advanced)
    dispatchNext(batch);
}

void MarketService::resolvePending(const std::shared_ptr<Batch> &batch,
                                   const std::shared_ptr<Pending> &pending,
                                   QuoteResult result) {
  if (m_batches.value(batch->batchId) != batch)
    return;
  const int index = batch->resultIndex.value(pending->positionId, -1);
  if (index < 0)
    return;
  batch->results[index] = std::move(result);
  ++batch->resolved;
  if (batch->resolved >= batch->expected) {
    const auto results = batch->results;
    m_batches.remove(batch->batchId);
    emit batchReady(batch->batchId, results);
  }
}

QuoteResult MarketService::parseReply(Provider provider,
                                      const QString &positionId,
                                      const QString &symbol,
                                      const QByteArray &body) {
  if (provider == Provider::Tencent || provider == Provider::Sina)
    return parseReplyText(provider, positionId, symbol,
                          decodeChinesePayload(body));
  QuoteResult result;
  result.positionId = positionId;
  result.symbol = symbol;
  result.precision = pricePrecision(symbol);
  result.source = providerName(provider);
  const QString market = marketKey(symbol);
  result.currency = market == QStringLiteral("CN")   ? QStringLiteral("CNY")
                    : market == QStringLiteral("HK") ? QStringLiteral("HKD")
                                                     : QStringLiteral("USD");
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
    result.name = data.value(QStringLiteral("f58")).toString();
    result.marketTimestamp =
        data.value(QStringLiteral("f124")).toVariant().toLongLong();
  } else {
    const auto chart = QJsonDocument::fromJson(body)
                           .object()
                           .value(QStringLiteral("chart"))
                           .toObject();
    const auto values = chart.value(QStringLiteral("result")).toArray();
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
      const auto closes = quoteArray.isEmpty()
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
    result.marketTimestamp = meta.value(QStringLiteral("regularMarketTime"))
                                 .toVariant()
                                 .toLongLong();
  }
  result.success = true;
  return result;
}

QuoteResult MarketService::parseReplyText(
    Provider provider, const QString &positionId, const QString &symbol,
    const QString &text) {
  QuoteResult result;
  result.positionId = positionId;
  result.symbol = symbol;
  result.precision = pricePrecision(symbol);
  result.source = providerName(provider);
  const QString market = marketKey(symbol);
  result.currency = market == QStringLiteral("CN")   ? QStringLiteral("CNY")
                    : market == QStringLiteral("HK") ? QStringLiteral("HKD")
                                                     : QStringLiteral("USD");
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
  const QStringList parts =
      text.mid(valueStart, end - valueStart)
          .split(provider == Provider::Tencent ? QLatin1Char('~')
                                               : QLatin1Char(','));
  if (parts.size() <= 3)
    throw std::runtime_error("missing quote price");
  bool ok = false;
  result.price = parts.at(3).toDouble(&ok);
  if (!ok || !validPrice(result.price))
    throw std::runtime_error("invalid quote price");
  result.name =
      provider == Provider::Tencent ? parts.value(1) : parts.value(0);
  result.marketTimestamp = providerTimestamp(parts, market);
  result.success = true;
  return result;
}

void MarketService::rememberTradingState(const QuoteResult &result) {
  if (result.marketTimestamp <= 0)
    return;
  const QString market = marketKey(result.symbol);
  const auto now = marketNow(market);
  const QDate quoteDate =
      QDateTime::fromSecsSinceEpoch(result.marketTimestamp, QTimeZone::utc())
          .toTimeZone(marketTimeZone(market))
          .date();
  TradingState state;
  state.localDate = now.date();
  state.tradingDay = quoteDate == now.date();
  state.expiresAt =
      QDateTime::currentSecsSinceEpoch() + (state.tradingDay ? 86400 : 90);
  m_tradingStates.insert(market, state);
}
