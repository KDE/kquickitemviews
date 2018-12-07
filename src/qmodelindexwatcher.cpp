/***************************************************************************
 *   Copyright (C) 2018 by Emmanuel Lepage Vallee                          *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@kde.org>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#include "qmodelindexwatcher.h"

// Qt
#include <QQmlEngine>
#include <QQmlContext>

// KQuickItemViews
#include <contextadapterfactory.h>
#include <adapters/contextadapter.h>

class QModelIndexWatcherPrivate : public QObject
{
    Q_OBJECT
public:
    QAbstractItemModel    *m_pModel {nullptr};
    QPersistentModelIndex  m_Index  {       };

    QModelIndexWatcher *q_ptr;

public Q_SLOTS:
    void slotDismiss();
    void slotDataChanged(const QModelIndex &tl, const QModelIndex &br, const QVector<int> &roles);
    void slotRemoved(const QModelIndex &parent, int first, int last);
    void slotRowsMoved(const QModelIndex &p, int start, int end,
                       const QModelIndex &dest, int row);
};

QModelIndexWatcher::QModelIndexWatcher(QObject *parent) : QObject(parent),
    d_ptr(new QModelIndexWatcherPrivate())
{
    d_ptr->q_ptr = this;
}

QModelIndexWatcher::~QModelIndexWatcher()
{
    delete d_ptr;
}

QModelIndex QModelIndexWatcher::modelIndex() const
{
    return d_ptr->m_Index;
}

QAbstractItemModel *QModelIndexWatcher::model() const
{
    return d_ptr->m_pModel;
}

bool QModelIndexWatcher::isValid() const
{
    return d_ptr->m_Index.isValid();
}

void QModelIndexWatcher::setModelIndex(const QModelIndex &index)
{
    if (index == d_ptr->m_Index)
        return;

    // Disconnect old models
    if (d_ptr->m_pModel && d_ptr->m_pModel != index.model())
        d_ptr->slotDismiss();


    if (d_ptr->m_pModel != index.model()) {

        d_ptr->m_pModel = const_cast<QAbstractItemModel*>(index.model());

        connect(d_ptr->m_pModel, &QAbstractItemModel::destroyed,
            d_ptr, &QModelIndexWatcherPrivate::slotDismiss);
        connect(d_ptr->m_pModel, &QAbstractItemModel::dataChanged,
            d_ptr, &QModelIndexWatcherPrivate::slotDataChanged);
        connect(d_ptr->m_pModel, &QAbstractItemModel::rowsAboutToBeRemoved,
            d_ptr, &QModelIndexWatcherPrivate::slotRemoved);
        connect(d_ptr->m_pModel, &QAbstractItemModel::rowsMoved,
            d_ptr, &QModelIndexWatcherPrivate::slotRowsMoved);
    }

    d_ptr->m_Index = index;

    emit indexChanged();
    emit validChanged();
}

void QModelIndexWatcherPrivate::slotDismiss()
{
    disconnect(m_pModel, &QAbstractItemModel::destroyed,
        this, &QModelIndexWatcherPrivate::slotDismiss);
    disconnect(m_pModel, &QAbstractItemModel::dataChanged,
        this, &QModelIndexWatcherPrivate::slotDataChanged);
    disconnect(m_pModel, &QAbstractItemModel::rowsAboutToBeRemoved,
        this, &QModelIndexWatcherPrivate::slotRemoved);
    disconnect(m_pModel, &QAbstractItemModel::rowsMoved,
        this, &QModelIndexWatcherPrivate::slotRowsMoved);

    m_pModel = nullptr;
    m_Index  = QModelIndex();
    emit q_ptr->indexChanged();
    emit q_ptr->removed();
}

void QModelIndexWatcherPrivate::slotDataChanged(const QModelIndex &tl, const QModelIndex &br, const QVector<int> &roles)
{
    if (tl.parent() != m_Index.parent())
        return;

    if (tl.row() > m_Index.row() || br.row() < m_Index.row())
        return;

    if (tl.column() > m_Index.column() || br.column() < m_Index.column())
        return;

    emit q_ptr->dataChanged(roles);
}

void QModelIndexWatcherPrivate::slotRemoved(const QModelIndex &parent, int first, int last)
{
    if (parent != m_Index.parent() || !(m_Index.row() >= first && m_Index.row() <= last))
        return;

    emit q_ptr->removed();
    emit q_ptr->validChanged();

    slotDismiss();
}

void QModelIndexWatcherPrivate::slotRowsMoved(const QModelIndex &p, int start, int end, const QModelIndex &dest, int row)
{
    Q_UNUSED(p)
    Q_UNUSED(start)
    Q_UNUSED(end)
    Q_UNUSED(dest)
    Q_UNUSED(row)
    //TODO
}

#include <qmodelindexwatcher.moc>
