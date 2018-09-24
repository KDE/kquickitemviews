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
#include "visiblerange.h"

// Qt
#include <QtCore/QDebug>
#include <QQmlEngine>

// KQuickItemViews
#include "visiblerange_p.h"
#include "proxies/sizehintproxymodel.h"
#include "treetraversalreflector_p.h"
#include "adapters/modeladapter.h"
#include "adapters/abstractitemadapter_p.h"
#include "adapters/abstractitemadapter.h"
#include "viewbase.h"

class VisibleRangePrivate : public QObject
{
    Q_OBJECT
public:
    QQmlEngine* m_pEngine {nullptr};
    ModelAdapter* m_pModelAdapter;

    VisibleRange::SizeHintStrategy m_SizeStrategy { VisibleRange::SizeHintStrategy::PROXY };

    bool m_ModelHasSizeHints {false};
    QByteArray m_SizeHintRole;
    int m_SizeHintRoleIndex {-1};
    TreeTraversalReflector *m_pReflector {nullptr};

    // The viewport rectangle
    QPointF m_Position;
    QSizeF m_Size;

    QSizeF sizeHint(AbstractItemAdapter* item) const;

public Q_SLOTS:
    void slotModelChanged(QAbstractItemModel* m);
    void slotModelAboutToChange(QAbstractItemModel* m, QAbstractItemModel* o);
    void slotViewportChanged(const QRectF &viewport);
    void slotDataChanged(const QModelIndex& tl, const QModelIndex& br);
};

VisibleRange::VisibleRange(ModelAdapter* ma) : QObject(),
    d_ptr(new VisibleRangePrivate()), s_ptr(new VisibleRangeSync())
{
    d_ptr->m_pModelAdapter = ma;
    d_ptr->m_pReflector    = new TreeTraversalReflector(ma->view());

    connect(ma, &ModelAdapter::modelChanged,
        d_ptr, &VisibleRangePrivate::slotModelChanged);
    connect(ma->view(), &Flickable::viewportChanged,
        d_ptr, &VisibleRangePrivate::slotViewportChanged);
    connect(ma, &ModelAdapter::delegateChanged,
        d_ptr->m_pReflector, &TreeTraversalReflector::resetEverything);

    d_ptr->slotModelChanged(ma->rawModel());

    connect(d_ptr->m_pReflector, &TreeTraversalReflector::contentChanged,
        this, &VisibleRange::contentChanged);
}

QRectF VisibleRange::currentRect() const
{
    return {};
}

QSizeF AbstractItemAdapter::sizeHint() const
{
    return s_ptr->m_pRange->d_ptr->sizeHint(
        const_cast<AbstractItemAdapter*>(this)
    );
}

// QSizeF VisibleRange::sizeHint(const QModelIndex& index) const
// {
//     if (!d_ptr->m_SizeHintRole.isEmpty())
//         return index.data(d_ptr->m_SizeHintRoleIndex).toSize();
//
//     if (!d_ptr->m_pEngine)
//         d_ptr->m_pEngine = d_ptr->m_pView->rootContext()->engine();
//
//     if (d_ptr->m_ModelHasSizeHints)
//         return qobject_cast<SizeHintProxyModel*>(model().data())
//             ->sizeHintForIndex(index);
//
//     return {};
// }

QSizeF VisibleRangePrivate::sizeHint(AbstractItemAdapter* item) const
{
    QSizeF ret;

    //TODO switch to function table
    switch (m_SizeStrategy) {
        case VisibleRange::SizeHintStrategy::AOT:
            Q_ASSERT(false);
            break;
        case VisibleRange::SizeHintStrategy::JIT:
            Q_ASSERT(false);
            break;
        case VisibleRange::SizeHintStrategy::UNIFORM:
            Q_ASSERT(false);
            break;
        case VisibleRange::SizeHintStrategy::PROXY:
            Q_ASSERT(m_ModelHasSizeHints);

            ret = qobject_cast<SizeHintProxyModel*>(m_pModelAdapter->rawModel())
                ->sizeHintForIndex(item->index());

            static int i = 0;
            qDebug() << "SH" << ret << i++;

            break;
        case VisibleRange::SizeHintStrategy::ROLE:
        case VisibleRange::SizeHintStrategy::DELEGATE:
            Q_ASSERT(false);
            break;
    }

    if (!item->s_ptr->m_pPos)
        item->s_ptr->m_pPos = new BlockMetadata();

    item->s_ptr->m_pPos->m_Size = ret;

    if (auto prev = item->up()) {
        Q_ASSERT(prev->s_ptr->m_pPos->m_Position.y() != -1);
        item->s_ptr->m_pPos->m_Position.setY(prev->s_ptr->m_pPos->m_Position.y());
    }

    return ret;
}

QString VisibleRange::sizeHintRole() const
{
    return d_ptr->m_SizeHintRole;
}

void VisibleRange::setSizeHintRole(const QString& s)
{
    d_ptr->m_SizeHintRole = s.toLatin1();

    if (!d_ptr->m_SizeHintRole.isEmpty() && d_ptr->m_pModelAdapter->rawModel())
        d_ptr->m_SizeHintRoleIndex = d_ptr->m_pModelAdapter->rawModel()->roleNames().key(
            d_ptr->m_SizeHintRole
        );
}

void VisibleRangePrivate::slotModelAboutToChange(QAbstractItemModel* m, QAbstractItemModel* o)
{
    if (o)
        disconnect(o, &QAbstractItemModel::dataChanged,
            this, &VisibleRangePrivate::slotDataChanged);
}

void VisibleRangePrivate::slotModelChanged(QAbstractItemModel* m)
{
    m_pReflector->setModel(m);

    // Check if the proxyModel is used
    m_ModelHasSizeHints = m && m->metaObject()->inherits(
        &SizeHintProxyModel::staticMetaObject
    );

    if (!m_SizeHintRole.isEmpty() && m)
        m_SizeHintRoleIndex = m->roleNames().key(
            m_SizeHintRole
        );

    if (m)
        connect(m, &QAbstractItemModel::dataChanged,
            this, &VisibleRangePrivate::slotDataChanged);

    m_pReflector->populate();
}

void VisibleRangePrivate::slotViewportChanged(const QRectF &viewport)
{
    m_Position = viewport.topLeft();
    m_Size = viewport.size();
}

ModelAdapter *VisibleRange::modelAdapter() const
{
    return d_ptr->m_pModelAdapter;
}

QSizeF VisibleRange::size() const
{
    return d_ptr->m_Size;
}

QPointF VisibleRange::position() const
{
    return d_ptr->m_Position;
}

VisibleRange::SizeHintStrategy VisibleRange::sizeHintStrategy() const
{
    return d_ptr->m_SizeStrategy;
}

void VisibleRange::setSizeHintStrategy(VisibleRange::SizeHintStrategy s)
{
    Q_ASSERT(false); //TODO invalidate the cache
    d_ptr->m_SizeStrategy = s;
}

bool VisibleRange::isTotalSizeKnown() const
{
    if (!d_ptr->m_pModelAdapter->delegate())
        return false;

    if (!d_ptr->m_pModelAdapter->rawModel())
        return true;

    switch(d_ptr->m_SizeStrategy) {
        case VisibleRange::SizeHintStrategy::JIT:
            return false;
        default:
            return true;
    }
}

QSizeF VisibleRange::totalSize() const
{
    if ((!d_ptr->m_pModelAdapter->delegate()) || !d_ptr->m_pModelAdapter->rawModel())
        return {0.0, 0.0};

    return {}; //TODO
}

AbstractItemAdapter* VisibleRange::itemForIndex(const QModelIndex& idx) const
{
    return d_ptr->m_pReflector->itemForIndex(idx);
}

//TODO remove this content and check each range
void VisibleRangePrivate::slotDataChanged(const QModelIndex& tl, const QModelIndex& br)
{
    if (tl.model() && tl.model() != m_pModelAdapter->rawModel()) {
        Q_ASSERT(false);
        return;
    }

    if (br.model() && br.model() != m_pModelAdapter->rawModel()) {
        Q_ASSERT(false);
        return;
    }

    if ((!tl.isValid()) || (!br.isValid()))
        return;

    if (!m_pReflector->isActive(tl.parent(), tl.row(), br.row()))
        return;

    //FIXME tolerate other cases
    Q_ASSERT(m_pModelAdapter->rawModel());
    Q_ASSERT(tl.model() == m_pModelAdapter->rawModel() && br.model() == m_pModelAdapter->rawModel());
    Q_ASSERT(tl.parent() == br.parent());

    //TODO Use a smaller range when possible

    //itemForIndex(const QModelIndex& idx) const final override;
    for (int i = tl.row(); i <= br.row(); i++) {
        const auto idx = m_pModelAdapter->rawModel()->index(i, tl.column(), tl.parent());
        if (auto item = m_pReflector->itemForIndex(idx))
            item->s_ptr->performAction(VisualTreeItem::ViewAction::UPDATE);
    }
}

void VisibleRange::setItemFactory(ViewBase::ItemFactoryBase *factory)
{
    d_ptr->m_pReflector->setItemFactory([this, factory]() -> AbstractItemAdapter* {
        return factory->create(this);
    });
}

#include <visiblerange.moc>
