/*
    SPDX-FileCopyrightText: 2024 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "declarativehistorymodel.h"
#include "historyitem.h"
#include "historymodel.h"

DeclarativeHistoryModel::DeclarativeHistoryModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_model(HistoryModel::self())
{
    setSourceModel(m_model.get());
    setDynamicSortFilter(true);

    connect(this, &QSortFilterProxyModel::rowsInserted, this, &DeclarativeHistoryModel::countChanged);
    connect(this, &QSortFilterProxyModel::rowsRemoved, this, &DeclarativeHistoryModel::countChanged);
    connect(this, &QSortFilterProxyModel::modelReset, this, &DeclarativeHistoryModel::countChanged);
    
    // Connect source model changes to sourceCountChanged signal
    connect(m_model.get(), &HistoryModel::rowsInserted, this, &DeclarativeHistoryModel::sourceCountChanged);
    connect(m_model.get(), &HistoryModel::rowsRemoved, this, &DeclarativeHistoryModel::sourceCountChanged);
    connect(m_model.get(), &HistoryModel::modelReset, this, &DeclarativeHistoryModel::sourceCountChanged);
    
    connect(m_model.get(), &HistoryModel::changed, this, &DeclarativeHistoryModel::currentTextChanged);
    
    // Invalidate filter when source model changes to ensure starred-only view updates correctly
    // Use more targeted invalidation to avoid performance issues with large histories
    connect(m_model.get(), &HistoryModel::rowsInserted, this, [this](const QModelIndex &parent, int first, int last) {
        Q_UNUSED(parent)
        Q_UNUSED(last)
        if (m_starredOnly) {
            // Only invalidate if we're in starred-only mode and items were inserted at the top
            if (first == 0) {
                invalidateRowsFilter();
            }
        }
    });
    connect(m_model.get(), &HistoryModel::rowsMoved, this, [this](const QModelIndex &, int sourceStart, int, const QModelIndex &, int destStart) {
        if (m_starredOnly && (sourceStart == 0 || destStart == 0)) {
            // Only invalidate if the current item (row 0) is involved in the move
            invalidateRowsFilter();
        }
    });
    connect(m_model.get(), &HistoryModel::rowsRemoved, this, [this](const QModelIndex &parent, int first, int last) {
        Q_UNUSED(parent)
        Q_UNUSED(last)
        if (m_starredOnly && first == 0) {
            // Only invalidate if the current item was removed
            invalidateRowsFilter();
        }
    });
}

DeclarativeHistoryModel::~DeclarativeHistoryModel()
{
}

QString DeclarativeHistoryModel::currentText() const
{
    return m_model->rowCount() == 0 ? QString() : m_model->index(0).data(Qt::DisplayRole).toString();
}

int DeclarativeHistoryModel::sourceCount() const
{
    return m_model->rowCount();
}

bool DeclarativeHistoryModel::starredOnly() const
{
    return m_starredOnly;
}

void DeclarativeHistoryModel::setStarredOnly(bool value)
{
    if (m_starredOnly == value) {
        return;
    }
    m_starredOnly = value;
    invalidateRowsFilter();
    Q_EMIT starredOnlyChanged();
}

void DeclarativeHistoryModel::moveToTop(const QString &uuid)
{
    m_model->moveToTop(uuid);
}

void DeclarativeHistoryModel::remove(const QString &uuid)
{
    m_model->remove(uuid);
}

void DeclarativeHistoryModel::clearHistory()
{
    m_model->clearHistory();
}

void DeclarativeHistoryModel::invokeAction(const QString &uuid)
{
    if (const qsizetype row = m_model->indexOf(uuid); row >= 0) {
        Q_EMIT m_model->actionInvoked(m_model->m_items[row]);
    }
}

bool DeclarativeHistoryModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_starredOnly) {
        // Safety check: ensure we have a valid source model and row
        if (!sourceModel() || sourceRow < 0 || sourceRow >= sourceModel()->rowCount(sourceParent)) {
            return false;
        }
        
        QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
        if (!sourceIndex.isValid()) {
            return false;
        }
        
        QVariant starredData = sourceIndex.data(HistoryModel::StarredRole);
        return starredData.isValid() && starredData.toBool();
    }

    return true;
}

#include "moc_declarativehistorymodel.cpp"
