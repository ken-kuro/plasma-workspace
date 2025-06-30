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
    // Initialize sorting - sort by first column in ascending order to maintain original behavior when not prioritizing starred
    sort(0, Qt::AscendingOrder);

    connect(this, &QSortFilterProxyModel::rowsInserted, this, &DeclarativeHistoryModel::countChanged);
    connect(this, &QSortFilterProxyModel::rowsRemoved, this, &DeclarativeHistoryModel::countChanged);
    connect(this, &QSortFilterProxyModel::modelReset, this, &DeclarativeHistoryModel::countChanged);
    connect(m_model.get(), &HistoryModel::changed, this, &DeclarativeHistoryModel::currentTextChanged);
    
    // Force re-sort when new items are added and starred prioritization is enabled
    connect(m_model.get(), &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent, int first, int last) {
        Q_UNUSED(parent)
        Q_UNUSED(last)
        // Only trigger re-sort if starred prioritization is enabled and a new item was added at the top
        if (m_starredPrioritized && first == 0) {
            // Force a complete re-sort to ensure starred items are properly prioritized
            invalidate();
        }
    });
}

DeclarativeHistoryModel::~DeclarativeHistoryModel()
{
}

QVariant DeclarativeHistoryModel::data(const QModelIndex &index, int role) const
{
    if (role == SectionRole) {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
            return QStringLiteral("");
        }
        
        // Map proxy row to source row to get starred status and identify current item
        QModelIndex sourceIndex = mapToSource(index);
        if (!sourceIndex.isValid()) {
            return QStringLiteral("");
        }
        
        int sourceRow = sourceIndex.row();
        int proxyRow = index.row();
        
        // The current clipboard item is always at source row 0 and proxy row 0
        if (sourceRow == 0) {
            return QStringLiteral("Current");
        }
        
        // If starred prioritization is disabled, everything else is just "History"
        if (!m_starredPrioritized) {
            return QStringLiteral("History");
        }
        
        // When starred prioritization is enabled, we need to determine section boundaries
        // by looking at the contiguous groups in the sorted proxy model
        
        // Strategy: Find the transition point between starred and non-starred items
        // in the proxy model to create clean section boundaries
        
        // Check if current item is starred
        QVariant starredData = sourceIndex.data(HistoryModel::StarredRole);
        bool currentItemStarred = starredData.isValid() && starredData.toBool();
        
        // Look through the proxy model to find where the transition happens
        bool foundNonStarredItem = false;
        
        // Scan from row 1 (after current) to current position to see if we've passed
        // the starred section boundary
        for (int i = 1; i <= proxyRow; ++i) {
            QModelIndex proxyIndex = this->index(i, 0);
            if (proxyIndex.isValid()) {
                QModelIndex srcIndex = mapToSource(proxyIndex);
                if (srcIndex.isValid()) {
                    QVariant starred = srcIndex.data(HistoryModel::StarredRole);
                    bool isStarred = starred.isValid() && starred.toBool();
                    if (!isStarred) {
                        foundNonStarredItem = true;
                        break;
                    }
                }
            }
        }
        
        // If we're starred and haven't reached the non-starred section yet, we're in "Starred"
        // If we're non-starred or have passed into the non-starred section, we're in "History"
        if (currentItemStarred && !foundNonStarredItem) {
            return QStringLiteral("Starred");
        } else {
            return QStringLiteral("History");
        }
    }
    
    // For all other roles, delegate to base class
    return QSortFilterProxyModel::data(index, role);
}

QHash<int, QByteArray> DeclarativeHistoryModel::roleNames() const
{
    QHash<int, QByteArray> roles = QSortFilterProxyModel::roleNames();
    roles[SectionRole] = "section";
    return roles;
}

QString DeclarativeHistoryModel::currentText() const
{
    return m_model->rowCount() == 0 ? QString() : m_model->index(0).data(Qt::DisplayRole).toString();
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

bool DeclarativeHistoryModel::starredPrioritized() const
{
    return m_starredPrioritized;
}

void DeclarativeHistoryModel::setStarredPrioritized(bool value)
{
    if (m_starredPrioritized == value) {
        return;
    }
    m_starredPrioritized = value;
    invalidate();
    Q_EMIT starredPrioritizedChanged();
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
        // Always show the current clipboard item (row 0) to prevent UI lockout
        if (sourceRow == 0) {
            return true;
        }
        
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

bool DeclarativeHistoryModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    const int leftRow = source_left.row();
    const int rightRow = source_right.row();

    // When starred prioritization is disabled, maintain original chronological order
    if (!m_starredPrioritized) {
        return leftRow < rightRow;
    }

    // When starred prioritization is enabled, implement the correct sorting logic:
    // 1. Current clipboard item (row 0) always first
    // 2. Starred items come next (in chronological order among themselves)  
    // 3. Non-starred items come last (in chronological order among themselves)

    // Always prioritize the current clipboard item (row 0 in source model)
    if (leftRow == 0) {
        return true;
    } else if (rightRow == 0) {
        return false;
    }

    // For non-current items, check starred status
    const bool leftStarred = source_left.data(HistoryModel::StarredRole).toBool();
    const bool rightStarred = source_right.data(HistoryModel::StarredRole).toBool();

    // If one is starred and the other isn't, starred comes first
    if (leftStarred && !rightStarred) {
        return true;
    }
    if (rightStarred && !leftStarred) {
        return false;
    }

    // If both have the same starred status, maintain chronological order
    return leftRow < rightRow;
}

#include "moc_declarativehistorymodel.cpp"
