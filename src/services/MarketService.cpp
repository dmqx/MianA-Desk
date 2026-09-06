#include "MarketService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <limits>
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
  bool completed = false;
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
QString yahooSymbol(QString symbol) {
  if (symbol.endsWith(QStringLiteral(".US"))) {
    symbol.chop(3);
    symbol.replace(QLatin1Char('.'), QLatin1Char('-'));
  }
  return symbol;
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
    : QObject(parent), m_network(this) {
  requestCnTradingCalendar();
}

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
  if (value.endsWith(QStringLiteral(".SS")) ||
      value.endsWith(QStringLiteral(".SZ")) ||
      value.endsWith(QStringLiteral(".BJ")) ||
      value.endsWith(QStringLiteral(".HK")) ||
      value.endsWith(QStringLiteral(".US")))
    return value;
  static const QRegularExpression usTicker(
      QStringLiteral("^[A-Z][A-Z0-9]*([.-][A-Z0-9]+)?$"));
  if (usTicker.match(value).hasMatch()) {
    value.replace(QLatin1Char('-'), QLatin1Char('.'));
    return value + QStringLiteral(".US");
  }
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
  const QString upper = normalizeSymbol(symbol);
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

QDate MarketService::marketDate(const QString &symbol) const {
  return marketNow(marketKey(symbol)).date();
}

bool MarketService::isTradingDay(const QString &symbol, const QDate &date) const {
  if (!date.isValid())
    return false;
  if (marketKey(symbol) == QStringLiteral("CN") &&
      m_cnTradingCalendarLoaded && date >= m_cnCalendarFirst &&
      date <= m_cnCalendarLast)
    return m_cnTradingDates.contains(date);
  return date.dayOfWeek() < Qt::Saturday;
}

bool MarketService::isMarketOpen(const QString &symbol) const {
  const QString market = marketKey(symbol);
  const auto now = marketNow(market);
  if (!isTradingDay(symbol, now.date()))
    return false;
  const auto state = m_tradingStates.value(symbol);
  if (state.localDate == now.date() &&
      state.expiresAt > QDateTime::currentSecsSinceEpoch() && !state.tradingDay)
    return false;
  const int minute = now.time().hour() * 60 + now.time().minute();
  if (market == QStringLiteral("CN"))
    // A 股 09:15-09:30 为集合竞价，也需要自动刷新行情。
    return (minute >= 555 && minute < 690) || (minute >= 780 && minute < 900);
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
  const auto state = m_tradingStates.value(symbol);
  if (!isTradingDay(symbol, now.date()) ||
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
         (minute < (market == QStringLiteral("CN") ? 555 : 570)
              ? QStringLiteral("未开盘") : QStringLiteral("已收盘"));
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

void MarketService::requestIntraday(const QVector<QuoteRequest> &requests,
                                    int batchId) {
  if (requests.isEmpty()) {
    emit intradayReady(batchId, {});
    return;
  }
  auto results = std::make_shared<QVector<IntradayResult>>();
  auto pending = std::make_shared<int>(int(requests.size()));
  results->resize(requests.size());
  for (qsizetype i = 0; i < requests.size(); ++i) {
    const auto &request = requests.at(i);
    const bool cn = marketKey(request.symbol) == QStringLiteral("CN");
    QUrl url;
    if (cn) {
      url = QUrl(QStringLiteral("https://web.ifzq.gtimg.cn/appstock/"
                                "app/minute/query?code=%1")
                     .arg(chinaQuerySymbol(request.symbol)));
    } else {
      url = QUrl(QStringLiteral("https://query1.finance.yahoo.com/v8/"
                                "finance/chart/%1?interval=1m&range=1d")
                     .arg(QString::fromLatin1(
                         QUrl::toPercentEncoding(yahooSymbol(request.symbol)))));
    }
    QNetworkRequest requestObject{url};
    requestObject.setRawHeader(QByteArrayLiteral("User-Agent"),
                               QByteArrayLiteral("Mozilla/5.0 MianADesk/2.0"));
    requestObject.setTransferTimeout(8000);
    auto *reply = m_network.get(requestObject);
    m_replies.push_back(reply);
    connect(reply, &QNetworkReply::finished, this,
            [this, results, pending, batchId, i, request, reply, cn] {
              m_replies.removeAll(reply);
              IntradayResult result;
              result.positionId = request.positionId;
              result.symbol = request.symbol;
              if (reply->error() == QNetworkReply::NoError) {
                const QByteArray body = reply->readAll();
                try {
                  const auto parsed =
                      cn ? parseTencentIntraday(
                               chinaQuerySymbol(request.symbol), body)
                         : parseYahooIntraday(body, marketKey(request.symbol));
                  result.prices = parsed.prices;
                  result.minutes = parsed.minutes;
                  result.tradingDate = parsed.tradingDate;
                  if (result.prices.size() >= 2)
                    result.success = true;
                  else
                    result.error = QStringLiteral("分时数据不足");
                } catch (const std::exception &exception) {
                  result.error = QString::fromUtf8(exception.what());
                }
              } else {
                result.error = reply->errorString();
              }
              reply->deleteLater();
              (*results)[i] = std::move(result);
              if (--(*pending) == 0)
                emit intradayReady(batchId, *results);
            });
  }
}

void MarketService::abortAll() {
  // Invalidate batches before abort() can synchronously emit finished().
  m_batches.clear();
  const auto replies = std::exchange(m_replies, {});
  for (const auto &reply : replies) {
    if (reply) {
      disconnect(reply, nullptr, this, nullptr);
      reply->abort();
      reply->deleteLater();
    }
  }
  if (m_calendarReply) {
    disconnect(m_calendarReply, nullptr, this, nullptr);
    m_calendarReply->abort();
    m_calendarReply->deleteLater();
  }
  m_calendarReply = nullptr;
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
    QString ticker = normalizeSymbol(symbol);
    if (ticker.endsWith(QStringLiteral(".US")))
      ticker.chop(3);
    return QStringLiteral("us") + ticker;
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
                         "get?secid=%1.%2&fields=f43,f57,f58,f59,f60,f124")
              .arg(prefix, code);
  } else {
    url = QStringLiteral("https://query1.finance.yahoo.com/v8/finance/chart/"
                         "%1?interval=1m&range=1d")
              .arg(QString::fromLatin1(QUrl::toPercentEncoding(yahooSymbol(symbol))));
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
  if (m_batches.value(batch->batchId) != batch ||
      batch->resolved == batch->expected)
    return;
  QHash<int, QVector<std::shared_ptr<Pending>>> groups;
  for (const auto &pending : batch->pendings) {
    if (pending->inFlight || pending->completed)
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
    if (pending->completed)
      continue;
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
  if (m_batches.value(batch->batchId) != batch || pending->completed)
    return;
  const int index = batch->resultIndex.value(pending->positionId, -1);
  if (index < 0)
    return;
  pending->completed = true;
  pending->inFlight = false;
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
    const double prevClose = data.value(QStringLiteral("f60")).toDouble() /
                             std::pow(10.0, precision);
    if (validPrice(prevClose)) {
      result.previousClose = prevClose;
      result.changePercent =
          (result.price - prevClose) / prevClose * 100.0;
      result.hasChange = true;
    }
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
    const double chartPrevClose =
        meta.value(QStringLiteral("chartPreviousClose")).toDouble();
    if (validPrice(chartPrevClose)) {
      result.previousClose = chartPrevClose;
      result.changePercent =
          (result.price - chartPrevClose) / chartPrevClose * 100.0;
      result.hasChange = true;
    }
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
  const int prevCloseIndex = provider == Provider::Tencent ? 4 : 2;
  bool prevOk = false;
  const double prevClose = parts.value(prevCloseIndex).toDouble(&prevOk);
  if (prevOk && validPrice(prevClose)) {
      result.previousClose = prevClose;
    result.changePercent = (result.price - prevClose) / prevClose * 100.0;
    result.hasChange = true;
  }
  result.name =
      provider == Provider::Tencent ? parts.value(1) : parts.value(0);
  result.marketTimestamp = providerTimestamp(parts, market);
  result.success = true;
  return result;
}

int MarketService::cnIntradaySlot(int minuteOfDay) {
  // Keep the existing 240 buckets; session endpoints close the final bucket.
  if (minuteOfDay >= 570 && minuteOfDay <= 690)
    return std::min(119, minuteOfDay - 570);
  if (minuteOfDay >= 780 && minuteOfDay <= 900)
    return std::min(239, 120 + minuteOfDay - 780);
  return -1;
}

IntradayResult MarketService::parseTencentIntraday(const QString &key,
                                                    const QByteArray &body) {
  const auto payload = QJsonDocument::fromJson(body)
                           .object()
                           .value(QStringLiteral("data"))
                           .toObject()
                           .value(key)
                           .toObject()
                           .value(QStringLiteral("data"))
                           .toObject();
  IntradayResult result;
  result.tradingDate = QDate::fromString(
      payload.value(QStringLiteral("date")).toString(), QStringLiteral("yyyyMMdd"));
  if (!result.tradingDate.isValid())
    throw std::runtime_error("missing intraday trading date");
  const auto data = payload.value(QStringLiteral("data")).toArray();
  result.prices =
      QVector<double>(240, std::numeric_limits<double>::quiet_NaN());
  for (const auto &row : data) {
    const QStringList fields = row.toString().split(QLatin1Char(' '));
    QString timeField = fields.value(0);
    timeField.remove(QLatin1Char(':'));
    bool timeOk = false;
    const int hhmm = timeField.left(4).toInt(&timeOk);
    const int minuteOfDay =
        timeOk ? (hhmm / 100) * 60 + (hhmm % 100) : -1;
    bool priceOk = false;
    const double price = fields.value(1).toDouble(&priceOk);
    const int slot = cnIntradaySlot(minuteOfDay);
    if (priceOk && validPrice(price) && slot >= 0 &&
        slot < result.prices.size())
      result.prices[slot] = price;
  }
  bool hasPrice = false;
  for (const double value : result.prices)
    hasPrice = hasPrice || validPrice(value);
  if (!hasPrice)
    throw std::runtime_error("empty intraday payload");
  return result;
}

IntradayResult MarketService::parseYahooIntraday(const QByteArray &body,
                                                  const QString &market) {
  const auto root = QJsonDocument::fromJson(body)
                        .object()
                        .value(QStringLiteral("chart"))
                        .toObject()
                        .value(QStringLiteral("result"))
                        .toArray();
  if (root.isEmpty())
    throw std::runtime_error("empty Yahoo result");
  const auto quote = root.at(0)
                         .toObject()
                         .value(QStringLiteral("indicators"))
                         .toObject()
                         .value(QStringLiteral("quote"))
                         .toArray();
  const auto closes =
      quote.isEmpty() ? QJsonArray{} : quote.at(0).toObject()
                                          .value(QStringLiteral("close"))
                                          .toArray();
  const auto timestamps = root.at(0)
                              .toObject()
                              .value(QStringLiteral("timestamp"))
                              .toArray();
  IntradayResult result;
  result.prices.reserve(closes.size());
  for (qsizetype i = 0; i < closes.size(); ++i) {
    const auto &value = closes.at(i);
    if (!value.isDouble() || !validPrice(value.toDouble()) ||
        i >= timestamps.size())
      continue;
    const qint64 seconds = timestamps.at(i).toVariant().toLongLong();
    if (seconds <= 0)
      continue;
    const QDate date = QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc())
                           .toTimeZone(marketTimeZone(market)).date();
    if (date < result.tradingDate)
      continue;
    if (date != result.tradingDate) {
      result.prices.clear();
      result.minutes.clear();
      result.tradingDate = date;
    }
    result.prices.push_back(value.toDouble());
    result.minutes.push_back(seconds / 60);
  }
  if (result.prices.isEmpty() || !result.tradingDate.isValid())
    throw std::runtime_error("empty intraday payload");
  return result;
}

void MarketService::requestCnTradingCalendar() {
  QNetworkRequest request{QUrl(QStringLiteral(
      "https://assets.linkdiary.cn/shares/trade-data-list.txt"))};
  request.setRawHeader(QByteArrayLiteral("User-Agent"),
                       QByteArrayLiteral("Mozilla/5.0 MianADesk/2.0"));
  request.setTransferTimeout(8000);
  m_calendarReply = m_network.get(request);
  connect(m_calendarReply, &QNetworkReply::finished, this, [this] {
    const auto reply = m_calendarReply;
    m_calendarReply = nullptr;
    if (!reply)
      return;
    const bool ok = reply->error() == QNetworkReply::NoError;
    const QByteArray body = ok ? reply->readAll() : QByteArray{};
    reply->deleteLater();
    if (!ok)
      return;
    QSet<QDate> dates;
    const auto values = QString::fromUtf8(body).split(QLatin1Char(','));
    for (const auto &value : values) {
      const QDate date = QDate::fromString(value.trimmed(), Qt::ISODate);
      if (date.isValid())
        dates.insert(date);
    }
    if (dates.isEmpty())
      return;
    m_cnTradingDates = std::move(dates);
    const auto range = std::minmax_element(m_cnTradingDates.cbegin(),
                                          m_cnTradingDates.cend());
    m_cnCalendarFirst = *range.first;
    m_cnCalendarLast = *range.second;
    m_cnTradingCalendarLoaded = true;
  });
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
  m_tradingStates.insert(result.symbol, state);
}
