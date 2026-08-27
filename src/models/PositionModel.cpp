#include "PositionModel.h"

#include <QLocale>
#include <algorithm>

PositionModel::PositionModel(QObject *parent) : QAbstractListModel(parent) {}

int PositionModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : m_positions.size();
}

QVariant PositionModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_positions.size())
    return {};
  const auto &item = m_positions.at(index.row());
  switch (role) {
  case IdRole:
    return item.id;
  case SymbolRole:
    return item.symbol;
  case NameRole:
    return item.name;
  case PriceTextRole:
    return item.price > 0.0 ? moneySymbol(item.currency) +
                                  formatPrice(item.price, item.precision)
                            : QStringLiteral("--");
  case ErrorRole:
    return !item.error.isEmpty();
  default:
    return {};
  }
}

QHash<int, QByteArray> PositionModel::roleNames() const {
  return {{IdRole, "positionId"},  {SymbolRole, "symbol"},
          {NameRole, "name"},      {PriceTextRole, "priceText"},
          {ErrorRole, "hasError"}};
}

const QVector<Position> &PositionModel::positions() const noexcept {
  return m_positions;
}
QVector<Position> &PositionModel::positions() noexcept { return m_positions; }

void PositionModel::replaceAll(QVector<Position> positions) {
  beginResetModel();
  m_positions = std::move(positions);
  endResetModel();
}

void PositionModel::append(Position position) {
  const int row = m_positions.size();
  beginInsertRows({}, row, row);
  m_positions.push_back(std::move(position));
  endInsertRows();
}

bool PositionModel::removeById(const QString &id) {
  const int row = indexOfId(id);
  if (row < 0)
    return false;
  beginRemoveRows({}, row, row);
  m_positions.removeAt(row);
  endRemoveRows();
  return true;
}

bool PositionModel::movePosition(int source, int destination) {
  if (source == destination || source < 0 || destination < 0 ||
      source >= m_positions.size() || destination >= m_positions.size())
    return false;
  const int target = destination > source ? destination + 1 : destination;
  if (!beginMoveRows({}, source, source, {}, target))
    return false;
  m_positions.move(source, destination);
  endMoveRows();
  return true;
}

Position *PositionModel::find(const QString &id) {
  const int row = indexOfId(id);
  return row >= 0 ? &m_positions[row] : nullptr;
}
const Position *PositionModel::find(const QString &id) const {
  const int row = indexOfId(id);
  return row >= 0 ? &m_positions.at(row) : nullptr;
}
bool PositionModel::containsSymbol(const QString &symbol,
                                   const QString &exceptId) const {
  return std::any_of(m_positions.cbegin(), m_positions.cend(),
                     [&](const Position &item) {
                       return item.symbol == symbol && item.id != exceptId;
                     });
}


void PositionModel::notifyPosition(const QString &id, const QList<int> &roles) {
  const int row = indexOfId(id);
  if (row >= 0)
    emit dataChanged(index(row), index(row), roles);
}

QString PositionModel::moneySymbol(const QString &currency) {
  if (currency == QStringLiteral("CNY"))
    return QStringLiteral("¥ ");
  if (currency == QStringLiteral("USD"))
    return QStringLiteral("$ ");
  if (currency == QStringLiteral("HKD"))
    return QStringLiteral("HK$ ");
  if (currency == QStringLiteral("JPY"))
    return QStringLiteral("JP¥ ");
  return currency + QLatin1Char(' ');
}
QString PositionModel::formatPrice(double value, int precision) {
  return QLocale(QLocale::English, QLocale::UnitedStates)
      .toString(value, 'f', std::clamp(precision, 0, 6));
}
int PositionModel::indexOfId(const QString &id) const {
  for (int i = 0; i < m_positions.size(); ++i)
    if (m_positions.at(i).id == id)
      return i;
  return -1;
}
