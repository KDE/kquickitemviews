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
#include "viewportapter.h"

//WARNING This module was dropped from the first release and doesn't compile

class ViewportAdapterPrivate : public QObject
{
public:
    qreal m_DelayedHeightDelta {   0   };
    bool  m_DisableApply       { false };
    qreal m_CurrentHeight      {   0   };

    void applyDelayedSize();
    qreal getSectionHeight(const QModelIndex& parent, int first, int last);

    void connectModel(QAbstractItemModel *m);
    void disconnectModel(QAbstractItemModel *m);

public Q_SLOTS:
    void slotReset();
    void slotRowsInserted(const QModelIndex& parent, int first, int last);
    void slotRowsRemoved(const QModelIndex& parent, int first, int last);
};

void ViewportAdapter::setModel(QAbstractItemModel *m)
{
    if (d_ptr->m_pModel)
        d_ptr->disconnectModel(d_ptr->m_pModel);

    d_ptr->m_pModel = m;

    if (m)
        d_ptr->connectModel(m);
}

void ViewportAdapterPrivate::slotReset()
{
    m_DisableApply = true;
    m_CurrentHeight = 0.0;
    if (int rc = m_pModelAdapter->rawModel()->rowCount())
        slotRowsInserted({}, 0, rc - 1);
    m_DisableApply = false;
    applyDelayedSize();
}


//TODO add a way to allow model to provide this information manually and
// generally don't cleanup when this code-path is used. Maybe trun into an
// adapter

void ViewportAdapterPrivate::applyDelayedSize()
{
    if (m_DisableApply)
        return;

    m_CurrentHeight += m_DelayedHeightDelta;
    m_DelayedHeightDelta = 0;

    m_pModelAdapter->view()->contentItem()->setHeight(m_CurrentHeight);
}

void ViewportAdapterPrivate::slotRowsInserted(const QModelIndex& parent, int first, int last)
{
    const bool apply = m_DisableApply;
    m_DisableApply = false;

    m_DelayedHeightDelta += getSectionHeight(parent, first, last);

    if (m_pModelAdapter->maxDepth() != 1) {
        const auto idx = m_pModelAdapter->rawModel()->index(first, 0, parent);
        Q_ASSERT(idx.isValid());
        if (int rc = m_pModelAdapter->rawModel()->rowCount(idx))
            slotRowsInserted(idx, 0, rc - 1);
    }

    m_DisableApply = apply;
    applyDelayedSize();
}

void ViewportAdapterPrivate::slotRowsRemoved(const QModelIndex& parent, int first, int last)
{
    const bool apply = m_DisableApply;
    m_DisableApply = false;

    m_DelayedHeightDelta -= getSectionHeight(parent, first, last);

    if (m_pModelAdapter->maxDepth() != 1) {
        const auto idx = m_pModelAdapter->rawModel()->index(first, 0, parent);
        Q_ASSERT(idx.isValid());
        if (int rc = m_pModelAdapter->rawModel()->rowCount(idx))
            slotRowsRemoved(idx, 0, rc - 1);
    }

    m_DisableApply = apply;
    applyDelayedSize();
}

void ViewportAdapterPrivate::connectModel(QAbstractItemModel *m)
{
    connect(m, &QAbstractItemModel::rowsInserted,
        this, &ViewportPrivate::slotRowsInserted);
    connect(m, &QAbstractItemModel::rowsAboutToBeRemoved,
        this, &ViewportPrivate::slotRowsRemoved);
    connect(m, &QAbstractItemModel::modelReset,
        this, &ViewportPrivate::slotReset);
    connect(m, &QAbstractItemModel::layoutChanged,
        this, &ViewportPrivate::slotReset);
}

void ViewportAdapterPrivate::disconnectModel(QAbstractItemModel *m)
{
    disconnect(m, &QAbstractItemModel::rowsInserted,
        this, &ViewportPrivate::slotRowsInserted);
    disconnect(m, &QAbstractItemModel::rowsAboutToBeRemoved,
        this, &ViewportPrivate::slotRowsRemoved);
    disconnect(m, &QAbstractItemModel::modelReset,
        this, &ViewportPrivate::slotReset);
    disconnect(m, &QAbstractItemModel::layoutChanged,
        this, &ViewportPrivate::slotReset);
}


qreal ViewportPrivate::getSectionHeight(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(last) //TODO not implemented
    Q_ASSERT(m_pModelAdapter->rawModel());
    switch(m_SizeStrategy) {
        case Strategy::AOT:
        case Strategy::JIT:
        case Strategy::DELEGATE:
            Q_ASSERT(false); // Should not get here
            return 0.0;
        case Strategy::UNIFORM:
            Q_ASSERT(m_KnowsRowHeight);
            break;
        case Strategy::PROXY: {
            Q_ASSERT(q_ptr->modelAdapter()->hasSizeHints());
            const auto idx = m_pModelAdapter->rawModel()->index(first, 0, parent);
            return qobject_cast<SizeHintProxyModel*>(m_pModelAdapter->rawModel())
                ->sizeHintForIndex(idx).height();
        }
        case Strategy::ROLE:
            Q_ASSERT(false); //TODO not implemented
            break;
    }

    Q_ASSERT(false);
    return 0.0;
}

void Viewport::setSizeHintStrategy(Strategy s)
{
    const auto old = d_ptr->m_SizeStrategy;

    auto needConnect = [](Strategy s) {
        return s == Strategy::PROXY || s == Strategy::UNIFORM || s == Strategy::ROLE;
    };

    const bool wasConnected = needConnect(old);
    const bool willConnect  = needConnect( s );

    s_ptr->m_pReflector->modelTracker() << StateTracker::Model::Action::RESET;
    d_ptr->m_SizeStrategy = s;

    const auto m = d_ptr->m_pModelAdapter->rawModel();

    if (m && wasConnected && !willConnect) {
        dsfdsfdsDisconnecT();
    }
    else if (d_ptr->m_pModelAdapter->rawModel() && willConnect && !wasConnected) {
        dslfdslkfhConnect();
    }
}


bool Viewport::isTotalSizeKnown() const
{
    if (!d_ptr->m_pModelAdapter->delegate())
        return false;

    if (!d_ptr->m_pModelAdapter->rawModel())
        return true;

}
