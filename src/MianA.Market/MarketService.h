#pragma once

#include "MianA.Core/Quote.h"
#include "MianA.Market/QuoteProviders.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QVector>
#include <memory>

class QNetworkReply;

class MarketService final : public QObject {
  Q_OBJECT
public:
  using Provider = QuoteProviders::Provider;
  explicit MarketService(QObject *parent = nullptr);
  [[nodiscard]] bool isMarketOpen(const QString &symbol) const;
  [[nodiscard]] QString marketStatus(const QString &symbol) const;
  [[nodiscard]] QDate marketDate(const QString &symbol) const;
  [[nodiscard]] bool isTradingDay(const QString &symbol,
                                  const QDate &date) const;
  void requestQuotes(const QVector<QuoteRequest> &requests, int batchId);
  void requestIntraday(const QVector<QuoteRequest> &requests, int batchId);
  void abortAll();

signals:
  void batchReady(int batchId, const QVector<QuoteResult> &results);
  void intradayReady(int batchId, const QVector<IntradayResult> &results);

private:
  struct Pending;
  struct Batch;
  struct TradingState {
    QDate localDate;
    bool tradingDay = true;
    qint64 expiresAt = 0;
  };
  void dispatchNext(const std::shared_ptr<Batch> &batch);
  void handleGroupedReply(const std::shared_ptr<Batch> &batch,
                          const QVector<std::shared_ptr<Pending>> &pendings,
                          Provider provider, QNetworkReply *reply);
  void resolvePending(const std::shared_ptr<Batch> &batch,
                      const std::shared_ptr<Pending> &pending,
                      QuoteResult result);
  [[nodiscard]] static QVector<Provider> providersFor(const QString &symbol);
  [[nodiscard]] static QNetworkRequest makeRequest(Provider provider,
                                                   const QString &symbol);
  [[nodiscard]] static QNetworkRequest
  makeGroupedRequest(Provider provider,
                     const QVector<std::shared_ptr<Pending>> &pendings);
  [[nodiscard]] static IntradayResult
  parseTencentIntraday(const QString &key, const QByteArray &body);
  [[nodiscard]] static IntradayResult
  parseYahooIntraday(const QByteArray &body, const QString &market);
  void requestCnTradingCalendar();
  void rememberTradingState(const QuoteResult &result);
  [[nodiscard]] QDateTime marketNow(const QString &market) const;

  QNetworkAccessManager m_network;
  QHash<int, std::shared_ptr<Batch>> m_batches;
  QHash<QString, TradingState> m_tradingStates;
  QSet<QDate> m_cnTradingDates;
  bool m_cnTradingCalendarLoaded = false;
  QDate m_cnCalendarFirst;
  QDate m_cnCalendarLast;
  QVector<QPointer<QNetworkReply>> m_replies;
  QPointer<QNetworkReply> m_calendarReply;
};
