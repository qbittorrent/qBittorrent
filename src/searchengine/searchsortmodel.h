#ifndef SEARCHSORTMODEL_H
#define SEARCHSORTMODEL_H

#include <QSortFilterProxyModel>
#include "core/utils/string.h"

class SearchSortModel : public QSortFilterProxyModel {
  Q_OBJECT

public:
  enum SearchColumn { NAME, SIZE, SEEDS, LEECHS, ENGINE_URL, DL_LINK, DESC_LINK, NB_SEARCH_COLUMNS };

  SearchSortModel(QObject *parent = 0) : QSortFilterProxyModel(parent) {}

protected:
  virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const {
    if (sortColumn() == NAME || sortColumn() == ENGINE_URL) {
      QVariant vL = sourceModel()->data(left);
      QVariant vR = sourceModel()->data(right);
      if (!(vL.isValid() && vR.isValid()))
        return QSortFilterProxyModel::lessThan(left, right);
      Q_ASSERT(vL.isValid());
      Q_ASSERT(vR.isValid());

      bool res = false;
      if (Utils::String::naturalSort(vL.toString(), vR.toString(), res))
        return res;

      return QSortFilterProxyModel::lessThan(left, right);
    }
    return QSortFilterProxyModel::lessThan(left, right);
  }
};

#endif // SEARCHSORTMODEL_H
