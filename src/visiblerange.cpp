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
#include "adapters/abstractitemadapter_p.h"
#include "adapters/abstractitemadapter.h"

class VisibleRangePrivate
{
public:
    QQmlEngine* m_pEngine {nullptr};
    ModelAdapter* m_pModelAdapter;

    VisibleRange::SizeHintStrategy m_SizeStrategy { VisibleRange::SizeHintStrategy::PROXY };

    bool m_ModelHasSizeHints {false};
    QByteArray m_SizeHintRole;
    int m_SizeHintRoleIndex {-1};
    QAbstractItemModel* m_pModel {nullptr};

    // The viewport rectangle
    QPointF m_Position;
    QSizeF m_Size;

    QSizeF sizeHint(AbstractItemAdapter* item) const;
};

VisibleRange::VisibleRange(ModelAdapter* ma) :
    d_ptr(new VisibleRangePrivate()), s_ptr(new VisibleRangeSync())
{
    d_ptr->m_pModelAdapter = ma;
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

            ret = qobject_cast<SizeHintProxyModel*>(m_pModel)
                ->sizeHintForIndex(item->index());

            static int i = 0;
            qDebug() << "SH" << ret << i++;

            break;
        case VisibleRange::SizeHintStrategy::ROLE:
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

    if (!d_ptr->m_SizeHintRole.isEmpty() && d_ptr->m_pModel)
        d_ptr->m_SizeHintRoleIndex = d_ptr->m_pModel->roleNames().key(
            d_ptr->m_SizeHintRole
        );
}

void VisibleRange::applyModelChanges(QAbstractItemModel* m)
{
    d_ptr->m_pModel = m;

    // Check if the proxyModel is used
    d_ptr->m_ModelHasSizeHints = m->metaObject()->inherits(
        &SizeHintProxyModel::staticMetaObject
    );

    if (!d_ptr->m_SizeHintRole.isEmpty() && m)
        d_ptr->m_SizeHintRoleIndex = m->roleNames().key(
            d_ptr->m_SizeHintRole
        );
}

ModelAdapter *VisibleRange::modelAdapter() const
{
    return d_ptr->m_pModelAdapter;
}

QSizeF VisibleRange::size() const
{
    return d_ptr->m_Size;
}

void VisibleRange::setSize(const QSizeF &size)
{
    d_ptr->m_Size = size;
    Q_ASSERT(s_ptr->m_pReflector);
    s_ptr->m_pReflector->moveEverything();
}

QPointF VisibleRange::position() const
{
    return d_ptr->m_Position;
}

void VisibleRange::setPosition(const QPointF& point)
{
    Q_ASSERT(false); //FIXME so I don't forget
    d_ptr->m_Position = point;
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