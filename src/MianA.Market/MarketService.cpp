#include "MianA.Market/MarketService.h"

#include "MianA.Core/IntradaySeries.h"
#include "MianA.Core/MarketRules.h"

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
bool validPrice(double price) { return std::isfinite(price) && price > 0.0; }
QNetworkRequest configuredRequest(const QUrl &url,
                                  const QByteArray &referer = {}) {
  QNetworkRequest request(url);
  request.setRawHeader(QByteArrayLiteral("User-Agent"),
                       QByteArrayLiteral("Mozilla/5.0 MianADesk/2.0"));
  if (!referer.isEmpty())
    request.setRawHeader(QByteArrayLiteral("Referer"), referer);
  request.setTransferTimeout(8000);
  return request;
}
} // namespace

MarketService::MarketService(QObject *parent)
    : QObject(parent), m_network(this) {
  requestCnTradingCalendar();
}

QDateTime MarketService::marketNow(const QString &market) const {
  return QDateTime::currentDateTimeUtc()
      .toTimeZone(MarketRules::timeZone(market));
}

QDate MarketService::marketDate(const QString &symbol) const {
  return marketNow(MarketRules::marketKey(symbol)).date();
}

bool MarketService::isTradingDay(const QString &symbol, const QDate &date) const {
  if (!date.isValid())
    return false;
  if (MarketRules::marketKey(symbol) == QStringLiteral("CN") &&
      m_cnTradingCalendarLoaded && date >= m_cnCalendarFirst &&
      date <= m_cnCalendarLast)
    return m_cnTradingDates.contains(date);
  return date.dayOfWeek() < Qt::Saturday;
}

bool MarketService::isMarketOpen(const QString &symbol) const {
  const QString market = MarketRules::marketKey(symbol);
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
  const QString market = MarketRules::marketKey(symbol);
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
    const bool cn = MarketRules::marketKey(request.symbol) == QStringLiteral("CN");
    QUrl url;
    if (cn) {
      url = QUrl(QStringLiteral("https://web.ifzq.gtimg.cn/appstock/"
                                "app/minute/query?code=%1")
                     .arg(QuoteProviders::queryKey(Provider::Tencent, request.symbol)));
    } else {
      url = QUrl(QStringLiteral("https://query1.finance.yahoo.com/v8/"
                                "finance/chart/%1?interval=1m&range=1d")
                     .arg(QString::fromLatin1(
                         QUrl::toPercentEncoding(QuoteProviders::queryKey(Provider::Yahoo, request.symbol)))));
    }
    QNetworkRequest requestObject = configuredRequest(url);
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
                               QuoteProviders::queryKey(Provider::Tencent, request.symbol), body)
                         : parseYahooIntraday(body, MarketRules::marketKey(request.symbol));
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
  return MarketRules::marketKey(symbol) == QStringLiteral("CN")
             ? QVector<Provider>{Provider::Tencent, Provider::Sina,
                                 Provider::EastMoney, Provider::Yahoo}
             : QVector<Provider>{Provider::Yahoo, Provider::Tencent};
}

QNetworkRequest MarketService::makeRequest(Provider provider,
                                           const QString &symbol) {
  QString url;
  if (provider == Provider::Tencent) {
    url = QStringLiteral("https://qt.gtimg.cn/q=%1")
              .arg(QuoteProviders::queryKey(provider, symbol));
  } else if (provider == Provider::Sina) {
    url = QStringLiteral("https://hq.sinajs.cn/list=%1")
              .arg(QuoteProviders::queryKey(provider, symbol));
  } else if (provider == Provider::EastMoney) {
    url = QStringLiteral("https://push2.eastmoney.com/api/qt/stock/"
                         "get?secid=%1&fields=f43,f57,f58,f59,f60,f124")
              .arg(QuoteProviders::queryKey(provider, symbol));
  } else {
    url = QStringLiteral("https://query1.finance.yahoo.com/v8/finance/chart/"
                         "%1?interval=1m&range=1d")
              .arg(QString::fromLatin1(QUrl::toPercentEncoding(QuoteProviders::queryKey(Provider::Yahoo, symbol))));
  }
  const QByteArray referer =
      provider == Provider::Tencent ? QByteArrayLiteral("https://gu.qq.com/")
      : provider == Provider::Sina
          ? QByteArrayLiteral("https://finance.sina.com.cn/")
          : QByteArray{};
  return configuredRequest(QUrl(url), referer);
}

QNetworkRequest MarketService::makeGroupedRequest(
    Provider provider, const QVector<std::shared_ptr<Pending>> &pendings) {
  QStringList keys;
  keys.reserve(pendings.size());
  for (const auto &pending : pendings)
    keys.push_back(QuoteProviders::queryKey(provider, pending->symbol));
  const QString joined = keys.join(QLatin1Char(','));
  QString url;
  if (provider == Provider::Tencent)
    url = QStringLiteral("https://qt.gtimg.cn/q=%1").arg(joined);
  else
    url = QStringLiteral("https://hq.sinajs.cn/list=%1").arg(joined);
  const QByteArray referer = provider == Provider::Tencent
                                 ? QByteArrayLiteral("https://gu.qq.com/")
                                 : QByteArrayLiteral("https://finance.sina.com.cn/");
  return configuredRequest(QUrl(url), referer);
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
          ? QuoteProviders::decodeChinesePayload(body)
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
                ? QuoteProviders::parseReplyText(provider, pending->positionId,
                                                   pending->symbol, groupedText)
                : QuoteProviders::parseReply(provider, pending->positionId,
                                               pending->symbol, body);
        rememberTradingState(result);
        resolvePending(batch, pending, std::move(result));
        continue;
      } catch (const std::exception &exception) {
        pending->errors.push_back(QuoteProviders::name(provider) +
                                  QStringLiteral(": ") +
                                  QString::fromUtf8(exception.what()));
      }
    } else {
      pending->errors.push_back(QuoteProviders::name(provider) +
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
    const int slot = IntradaySeries::chinaSlot(minuteOfDay);
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
                           .toTimeZone(MarketRules::timeZone(market)).date();
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
  QNetworkRequest request = configuredRequest(QUrl(QStringLiteral(
      "https://assets.linkdiary.cn/shares/trade-data-list.txt")));
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
  const QString market = MarketRules::marketKey(result.symbol);
  const auto now = marketNow(market);
  const QDate quoteDate =
      QDateTime::fromSecsSinceEpoch(result.marketTimestamp, QTimeZone::utc())
          .toTimeZone(MarketRules::timeZone(market))
          .date();
  TradingState state;
  state.localDate = now.date();
  state.tradingDay = quoteDate == now.date();
  state.expiresAt =
      QDateTime::currentSecsSinceEpoch() + (state.tradingDay ? 86400 : 90);
  m_tradingStates.insert(result.symbol, state);
}
