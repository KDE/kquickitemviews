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
#include "viewport.h"

// Qt
#include <QtCore/QDebug>
#include <QQmlEngine>

// KQuickItemViews
#include "viewport_p.h"
#include "proxies/sizehintproxymodel.h"
#include "treetraversalreflector_p.h"
#include "adapters/modeladapter.h"
#include "adapters/abstractitemadapter_p.h"
#include "adapters/abstractitemadapter.h"
#include "viewbase.h"

class ViewportPrivate : public QObject
{
    Q_OBJECT
public:
    QQmlEngine* m_pEngine {nullptr};
    ModelAdapter* m_pModelAdapter;

    Viewport::SizeHintStrategy m_SizeStrategy { Viewport::SizeHintStrategy::JIT };

    bool m_ModelHasSizeHints {false};
    QByteArray m_SizeHintRole;
    int m_SizeHintRoleIndex {-1};
    TreeTraversalReflector *m_pReflector {nullptr};

    // The viewport rectangle
    QRectF m_ViewRect;
    QRectF m_UsedRect;

    QSizeF sizeHint(BlockMetadata* item) const;
    void updateEdges(BlockMetadata *item);
    QPair<Qt::Edge,Qt::Edge> fromGravity() const;

    BlockMetadata *m_lpLoadedEdges [4] {nullptr}; //top, left, right, bottom
    BlockMetadata *m_lpVisibleEdges[4] {nullptr}; //top, left, right, bottom //TODO

    Viewport *q_ptr;

public Q_SLOTS:
    void slotModelChanged(QAbstractItemModel* m);
    void slotModelAboutToChange(QAbstractItemModel* m, QAbstractItemModel* o);
    void slotViewportChanged(const QRectF &viewport);
    void slotDataChanged(const QModelIndex& tl, const QModelIndex& br);
};

Viewport::Viewport(ModelAdapter* ma) : QObject(),
    d_ptr(new ViewportPrivate()), s_ptr(new ViewportSync())
{
    d_ptr->q_ptr = this;
    s_ptr->q_ptr = this;
    d_ptr->m_pModelAdapter = ma;
    d_ptr->m_pReflector    = new TreeTraversalReflector(this);

    resize(QRectF { 0.0, 0.0, ma->view()->width(), ma->view()->height() });

    connect(ma, &ModelAdapter::modelChanged,
        d_ptr, &ViewportPrivate::slotModelChanged);
    connect(ma->view(), &Flickable::viewportChanged,
        d_ptr, &ViewportPrivate::slotViewportChanged);
    connect(ma, &ModelAdapter::delegateChanged,
        d_ptr->m_pReflector, &TreeTraversalReflector::resetEverything);

    d_ptr->slotModelChanged(ma->rawModel());

    connect(d_ptr->m_pReflector, &TreeTraversalReflector::contentChanged,
        this, &Viewport::contentChanged);
}

QRectF Viewport::currentRect() const
{
    return {};
}

QSizeF AbstractItemAdapter::sizeHint() const
{
    Q_ASSERT(false);
    /*return s_ptr->m_pRange->d_ptr->sizeHint(
        const_cast<AbstractItemAdapter*>(this)
    );*/
    return {};
}

// QSizeF Viewport::sizeHint(const QModelIndex& index) const
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

QPair<Qt::Edge,Qt::Edge> ViewportPrivate::fromGravity() const
{
    switch (m_pModelAdapter->view()->gravity()) {
        case Qt::Corner::TopLeftCorner:
            return {Qt::Edge::TopEdge, Qt::Edge::LeftEdge};
        case Qt::Corner::TopRightCorner:
            return {Qt::Edge::TopEdge, Qt::Edge::RightEdge};
        case Qt::Corner::BottomLeftCorner:
            return {Qt::Edge::BottomEdge, Qt::Edge::LeftEdge};
        case Qt::Corner::BottomRightCorner:
            return {Qt::Edge::BottomEdge, Qt::Edge::RightEdge};
    }

    Q_ASSERT(false);
    return {};
}

QSizeF ViewportPrivate::sizeHint(BlockMetadata *item) const
{
    QSizeF ret;

    //TODO switch to function table
    switch (m_SizeStrategy) {
        case Viewport::SizeHintStrategy::AOT:
            Q_ASSERT(false);
            break;
        case Viewport::SizeHintStrategy::JIT:
            if (item->m_pItem)
                ret = item->m_pItem->geometry().size();
            else {
                // JIT cannot be used past the loaded bounds, the value isn't known
                Q_ASSERT(false);
            }
            break;
        case Viewport::SizeHintStrategy::UNIFORM:
            Q_ASSERT(false);
            break;
        case Viewport::SizeHintStrategy::PROXY:
            Q_ASSERT(m_ModelHasSizeHints);

            ret = qobject_cast<SizeHintProxyModel*>(m_pModelAdapter->rawModel())
                ->sizeHintForIndex(item->index());

            static int i = 0;
            qDebug() << "SH" << ret << i++;

            break;
        case Viewport::SizeHintStrategy::ROLE:
        case Viewport::SizeHintStrategy::DELEGATE:
            Q_ASSERT(false);
            break;
    }

    item->m_Size = ret;

    if (auto prev = item->up()) {
        Q_ASSERT(prev->m_Position.y() != -1);
        item->m_Position.setY(prev->m_Position.y());
    }

    return ret;
}

QString Viewport::sizeHintRole() const
{
    return d_ptr->m_SizeHintRole;
}

void Viewport::setSizeHintRole(const QString& s)
{
    d_ptr->m_SizeHintRole = s.toLatin1();

    if (!d_ptr->m_SizeHintRole.isEmpty() && d_ptr->m_pModelAdapter->rawModel())
        d_ptr->m_SizeHintRoleIndex = d_ptr->m_pModelAdapter->rawModel()->roleNames().key(
            d_ptr->m_SizeHintRole
        );
}

void ViewportPrivate::slotModelAboutToChange(QAbstractItemModel* m, QAbstractItemModel* o)
{
    if (o)
        disconnect(o, &QAbstractItemModel::dataChanged,
            this, &ViewportPrivate::slotDataChanged);
}

void ViewportPrivate::slotModelChanged(QAbstractItemModel* m)
{
    m_pReflector->setModel(m);

    // Check if the proxyModel is used
    m_ModelHasSizeHints = m && m->metaObject()->inherits(
        &SizeHintProxyModel::staticMetaObject
    );

    if (m_ModelHasSizeHints)
        q_ptr->setSizeHintStrategy(Viewport::SizeHintStrategy::PROXY);

    if (!m_SizeHintRole.isEmpty() && m)
        m_SizeHintRoleIndex = m->roleNames().key(
            m_SizeHintRole
        );

    if (m)
        connect(m, &QAbstractItemModel::dataChanged,
            this, &ViewportPrivate::slotDataChanged);

    m_pReflector->populate(fromGravity().first);
    //m_pReflector->populate(fromGravity().second); //TODO
}

void ViewportPrivate::slotViewportChanged(const QRectF &viewport)
{
    m_UsedRect = viewport;
}

ModelAdapter *Viewport::modelAdapter() const
{
    return d_ptr->m_pModelAdapter;
}

QSizeF Viewport::size() const
{
    return d_ptr->m_UsedRect.size();
}

QPointF Viewport::position() const
{
    return d_ptr->m_UsedRect.topLeft();
}

Viewport::SizeHintStrategy Viewport::sizeHintStrategy() const
{
    return d_ptr->m_SizeStrategy;
}

void Viewport::setSizeHintStrategy(Viewport::SizeHintStrategy s)
{
    d_ptr->m_pReflector->resetGeometry();
    d_ptr->m_SizeStrategy = s;
}

bool Viewport::isTotalSizeKnown() const
{
    if (!d_ptr->m_pModelAdapter->delegate())
        return false;

    if (!d_ptr->m_pModelAdapter->rawModel())
        return true;

    switch(d_ptr->m_SizeStrategy) {
        case Viewport::SizeHintStrategy::JIT:
            return false;
        default:
            return true;
    }
}

QSizeF Viewport::totalSize() const
{
    if ((!d_ptr->m_pModelAdapter->delegate()) || !d_ptr->m_pModelAdapter->rawModel())
        return {0.0, 0.0};

    return {}; //TODO
}

AbstractItemAdapter* Viewport::itemForIndex(const QModelIndex& idx) const
{
    return d_ptr->m_pReflector->itemForIndex(idx);
}

//TODO remove this content and check each range
void ViewportPrivate::slotDataChanged(const QModelIndex& tl, const QModelIndex& br)
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

void Viewport::setItemFactory(ViewBase::ItemFactoryBase *factory)
{
    d_ptr->m_pReflector->setItemFactory([this, factory]() -> AbstractItemAdapter* {
        return factory->create(this);
    });
}

Qt::Edges Viewport::availableEdges() const
{
    return d_ptr->m_pReflector->availableEdges();
}

void ViewportPrivate::updateEdges(BlockMetadata *item)
{
    if (item->m_pItem) {
        Q_ASSERT(item->m_pItem->m_State != VisualTreeItem::State::POOLING);
        Q_ASSERT(item->m_pItem->m_State != VisualTreeItem::State::POOLED);
        Q_ASSERT(item->m_pItem->m_State != VisualTreeItem::State::ERROR);
        Q_ASSERT(item->m_pItem->m_State != VisualTreeItem::State::DANGLING);
    }

    enum Pos {Top, Left, Right, Bottom};

    const auto geo = item->geometry();

    static const Qt::Edge edgeMap[] = {
        Qt::TopEdge, Qt::LeftEdge, Qt::RightEdge, Qt::BottomEdge
    };

#define CHECK_EDGE(code) [](const QRectF& old, const QRectF& self) {return code;}
    static const std::function<bool(const QRectF&, const QRectF&)> isEdge[] = {
        CHECK_EDGE(old.y() > self.y()),
        CHECK_EDGE(old.x() > self.x()),
        CHECK_EDGE(old.bottomRight().x() < self.bottomRight().x()),
        CHECK_EDGE(old.bottomRight().y() < self.bottomRight().y()),
    };
#undef CHECK_EDGE

    Qt::Edges ret;

    for (int i = Pos::Top; i <= Pos::Bottom; i++) {
        if ((!m_lpLoadedEdges[i]) || m_lpLoadedEdges[i] == item || isEdge[i](m_lpLoadedEdges[i]->geometry(), geo)) {
            ret |= edgeMap[i];

            // Unset the edge
            if (m_lpLoadedEdges[i] != item) {
                if (m_lpLoadedEdges[i])
                    m_lpLoadedEdges[i]->m_IsEdge &= ~edgeMap[i];

                m_lpLoadedEdges[i] = item;
            }
        }
    }

    // Ensure that the m_ViewRect is in sync
    Q_ASSERT(m_ViewRect.isValid() || (
            m_pModelAdapter->view()->width() == 0 &&
            m_pModelAdapter->view()->height() == 0
        )
    );

//     qDebug() << "UPDATE EDGES1" << (int) item->m_pItem->m_State;
    if (m_ViewRect.intersects(geo)) {
        //item->performAction(BlockMetadata::Action::SHOW);
    }
    else
        item->performAction(BlockMetadata::Action::HIDE);

//TODO update m_lpVisibleEdges
    Qt::Edges available;

//     qDebug() << "UPDATE EDGES2" << (int) item->m_pItem->m_State;
    for (int i = Pos::Top; i <= Pos::Bottom; i++) {
        if ((!m_lpLoadedEdges[i]) || m_lpLoadedEdges[i]->geometry().contains(m_ViewRect)) {
            available |= edgeMap[i];
        }
        else {
            //TODO batch them instead of doing this over and over
            //m_pReflector->detachUntil(edgeMap[i], m_lpLoadedEdges[i]);
//             m_lpLoadedEdges[i]->performAction(VisualTreeItem::ViewAction::LEAVE_BUFFER);
        }
    }

    qDebug() << "\n\nSET VAIL " << available;

    m_pReflector->setAvailableEdges(available);
}

void ViewportSync::geometryUpdated(BlockMetadata *item)
{
    //TODO assert if the size hints don't match reality

    auto geo = item->geometry();

    if (!geo.isValid()) {
        geo = QRectF {item->m_Position, item->m_Size = q_ptr->d_ptr->sizeHint(item)};
    }

//     Q_ASSERT((!item->m_pItem) || item->m_pItem->geometry().size() == geo.size());

    const auto old  = q_ptr->d_ptr->m_UsedRect;
    const auto view = q_ptr->d_ptr->m_ViewRect;

    // Update the used rect
    const QRectF r = q_ptr->d_ptr->m_UsedRect = q_ptr->d_ptr->m_UsedRect.united(geo);

    const bool hasSpaceOnTop = view.y();

    q_ptr->d_ptr->updateEdges(item);
}

void ViewportSync::updateGeometry(BlockMetadata* item)
{
    if (q_ptr->d_ptr->m_SizeStrategy != Viewport::SizeHintStrategy::JIT)
        q_ptr->d_ptr->sizeHint(item);

    q_ptr->d_ptr->updateEdges(item);
}

void Viewport::resize(const QRectF& rect)
{
    d_ptr->m_ViewRect = rect;
}

#include <viewport.moc>
