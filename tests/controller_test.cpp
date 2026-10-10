#include "MianA.UI/AppController.h"
#include "MianA.Windows/WindowManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QLocalServer>
#include <QLockFile>
#include <QTemporaryDir>
#include <cstdlib>

namespace {
int requestedBatch = 0;

void require(bool condition, const char *message) {
  if (!condition)
    qFatal("%s", message);
}
} // namespace

// Only external side effects are replaced; controller, model and storage are real.
MarketService::MarketService(QObject *parent) : QObject(parent) {}
bool MarketService::isMarketOpen(const QString &) const { return true; }
QString MarketService::marketStatus(const QString &) const {
  return QStringLiteral("开市");
}
QDate MarketService::marketDate(const QString &) const {
  return QDate::currentDate();
}
bool MarketService::isTradingDay(const QString &, const QDate &) const {
  return true;
}
void MarketService::requestQuotes(const QVector<QuoteRequest> &, int batchId) {
  requestedBatch = batchId;
}
void MarketService::requestIntraday(const QVector<QuoteRequest> &, int) {}
void MarketService::retryTradingCalendar(bool) {}
void MarketService::abortAll() {}

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent) {}
void UpdateChecker::check() {}

WindowManager::WindowManager(QObject *parent) : QObject(parent) {}
WindowManager::~WindowManager() = default;
void WindowManager::refreshWindowStyle() {}
void WindowManager::setTrayState(bool) {}
QColor WindowManager::sampleUnderlay(int, int, int, int) const { return {}; }
bool WindowManager::autostartEnabled() const { return false; }
bool WindowManager::setAutostart(bool) { return true; }
void WindowManager::showTrayMessage(const QString &, const QString &) {}
void WindowManager::showUpdateNotification(const QString &, const QUrl &) {}
bool WindowManager::nativeEventFilter(const QByteArray &, void *, qintptr *) {
  return false;
}
bool WindowManager::eventFilter(QObject *, QEvent *) { return false; }

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QTemporaryDir dataDirectory(QCoreApplication::applicationDirPath() +
                              QStringLiteral("/controller-test-XXXXXX"));
  require(dataDirectory.isValid(), "temporary data directory");
  require(qputenv("MIANA_DESK_DATA_DIR", dataDirectory.path().toUtf8()),
          "isolated data path");
  WindowManager windows;
  AppController controller(windows);
  auto *market = controller.findChild<MarketService *>();
  require(market != nullptr, "controller market service");
  const QString addError = controller.addPosition(QStringLiteral("600000"), {});
  if (!addError.isEmpty())
    qWarning() << "addPosition:" << addError << "data path:" << dataDirectory.path();
  require(addError.isEmpty(), "add position");
  const QString id = controller.positions()->positions().first().id;
  const auto position = [&] { return controller.positions()->find(id); };
  QuoteResult quote;
  quote.positionId = id;
  quote.symbol = QStringLiteral("600000.SS");
  quote.success = true;
  quote.price = 10.0;
  quote.currency = QStringLiteral("CNY");

  controller.toggle(QStringLiteral("paused"));
  require(controller.paused(), "pause while quote is in flight");
  const QString pausedText = controller.statusText();
  const QString pausedColor = controller.statusColor();
  require(pausedText == QStringLiteral("行情已暂停"), "initial paused status");
  const int firstBatch = requestedBatch;
  emit market->batchReady(firstBatch, {quote});
  require(position()->price == 10.0, "in-flight quote still updates price");
  require(controller.statusText() == pausedText &&
              controller.statusColor() == pausedColor,
          "quote completion preserves paused status");

  controller.manualRefresh();
  const int failedBatch = requestedBatch;
  QuoteResult failure = quote;
  failure.success = false;
  failure.error = QStringLiteral("offline");
  emit market->batchReady(failedBatch, {failure});
  require(position()->error == failure.error && position()->failureCount == 1,
          "new failure is applied");
  require(controller.statusText() == pausedText &&
              controller.statusColor() == pausedColor,
          "failed completion preserves paused status");
  quote.price = 99.0;
  emit market->batchReady(firstBatch, {quote});
  emit market->batchReady(firstBatch, {failure});
  require(position()->price == 10.0 && position()->error == failure.error &&
              position()->failureCount == 1,
          "old success and failure cannot replace a newer result");

  controller.manualRefresh();
  const int beforeEditBatch = requestedBatch;
  require(controller.editPosition(id, QStringLiteral("000001"), {}).isEmpty(),
          "edit A to B");
  require(controller.editPosition(id, QStringLiteral("600000"), {}).isEmpty(),
          "edit B back to A");
  emit market->batchReady(beforeEditBatch, {quote});
  require(position()->price == 0.0 && position()->error.isEmpty(),
          "A to B to A rejects the original A request");
  const int afterEditBatch = requestedBatch;
  require(afterEditBatch > beforeEditBatch, "pending refresh starts a new batch");
  quote.price = 12.0;
  emit market->batchReady(afterEditBatch, {quote});
  require(position()->price == 12.0 && position()->error.isEmpty(),
          "new quote after edits is accepted");
  emit market->batchReady(beforeEditBatch, {failure});
  require(position()->price == 12.0 && position()->error.isEmpty() &&
              position()->failureCount == 0,
          "old failure cannot overwrite a newer success");

  // No event loop: constructor refresh/update timers never run during this test.
  qInfo("controller regressions passed");
  return EXIT_SUCCESS;
}
