#pragma once

#include "MianA.Core/Position.h"

#include <QAbstractListModel>
#include <QDate>
#include <QString>
#include <QVector>
#include <QtQml/qqmlregistration.h>

class PositionModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("PositionModel is owned by AppController")
public:
  enum Role {
    IdRole = Qt::UserRole + 1,
    SymbolRole,
    NameRole,
    PriceTextRole,
    ErrorRole,
    PriceRole,
    ChangePercentRole,
    PreviousCloseRole,
    HasChangeRole,
    IntradayRole
  };
  Q_ENUM(Role)

  explicit PositionModel(QObject *parent = nullptr);
  [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex &index,
                              int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  [[nodiscard]] const QVector<Position> &positions() const noexcept;
  [[nodiscard]] QVector<Position> &positions() noexcept;
  void replaceAll(QVector<Position> positions);
  void append(Position position);
  bool removeById(const QString &id);
  bool movePosition(int source, int destination);
  [[nodiscard]] Position *find(const QString &id);
  [[nodiscard]] const Position *find(const QString &id) const;
  [[nodiscard]] bool containsSymbol(const QString &symbol,
                                    const QString &exceptId = {}) const;
  void notifyPosition(const QString &id, const QList<int> &roles = {});

  static QString formatPrice(double value, int precision);

private:
  [[nodiscard]] int indexOfId(const QString &id) const;
  QVector<Position> m_positions;
};
