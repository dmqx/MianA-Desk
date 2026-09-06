#pragma once

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

struct QuoteRequest {
  QString positionId;
  QString symbol;
};
struct QuoteResult {
  QString positionId;
  QString symbol;
  bool success = false;
  double price = 0.0;
  QString currency;
  QString name;
  QString source;
  int precision = 2;
  qint64 marketTimestamp = 0;
  double changePercent = 0.0;
  double previousClose = 0.0;
  bool hasChange = false;
  QString error;
};
struct IntradayResult {
  QString positionId;
  QString symbol;
  bool success = false;
  QDate tradingDate;
  QVector<double> prices;
  QVector<qint64> minutes;
  QString error;
};
Q_DECLARE_METATYPE(IntradayResult)

class QNetworkReply;

class MarketService final : public QObject {
  Q_OBJECT
public:
  enum class Provider { Tencent, Sina, EastMoney, Yahoo };
  explicit MarketService(QObject *parent = nullptr);
  static QString normalizeSymbol(const QString &raw);
  static QString marketKey(const QString &symbol);
  static int pricePrecision(const QString &symbol);
  static int cnIntradaySlot(int minuteOfDay);
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
  [[nodiscard]] static QString queryKey(Provider provider,
                                        const QString &symbol);
  [[nodiscard]] static QNetworkRequest makeRequest(Provider provider,
                                                   const QString &symbol);
  [[nodiscard]] static QNetworkRequest
  makeGroupedRequest(Provider provider,
                     const QVector<std::shared_ptr<Pending>> &pendings);
  [[nodiscard]] static QuoteResult parseReply(Provider provider,
                                              const QString &positionId,
                                              const QString &symbol,
                                              const QByteArray &body);
  [[nodiscard]] static QuoteResult parseReplyText(
      Provider provider, const QString &positionId, const QString &symbol,
      const QString &text);
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
