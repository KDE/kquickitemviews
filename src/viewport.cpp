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
#include <QQmlContext>

// KQuickItemViews
#include "viewport_p.h"
#include "proxies/sizehintproxymodel.h"
#include "treetraversalreflector_p.h"
#include "adapters/modeladapter.h"
#include "contextadapterfactory.h"
#include "adapters/contextadapter.h"
#include "adapters/abstractitemadapter_p.h"
#include "adapters/abstractitemadapter.h"
#include "viewbase.h"
#include "indexmetadata_p.h"


class ViewportPrivate : public QObject
{
    Q_OBJECT
public:
    QQmlEngine* m_pEngine {nullptr};
    ModelAdapter* m_pModelAdapter;

    Viewport::SizeHintStrategy m_SizeStrategy { Viewport::SizeHintStrategy::JIT };

    QByteArray m_SizeHintRole;
    int m_SizeHintRoleIndex {-1};
    bool m_KnowsRowHeight {false};

    qreal m_CurrentHeight      {0};
    qreal m_DelayedHeightDelta {0};
    bool  m_DisableApply {false};

    TreeTraversalReflector *m_pReflector {nullptr};

    // The viewport rectangle
    QRectF m_ViewRect;
    QRectF m_UsedRect;

    QPair<Qt::Edge,Qt::Edge> fromGravity() const;
    void updateAvailableEdges();
    qreal getSectionHeight(const QModelIndex& parent, int first, int last);
    void applyDelayedSize();

    Viewport *q_ptr;

public Q_SLOTS:
    void slotModelChanged(QAbstractItemModel* m, QAbstractItemModel* o);
    void slotModelAboutToChange(QAbstractItemModel* m, QAbstractItemModel* o);
    void slotViewportChanged(const QRectF &viewport);
    void slotDataChanged(const QModelIndex& tl, const QModelIndex& br, const QVector<int> &roles);
    void slotRowsInserted(const QModelIndex& parent, int first, int last);
    void slotRowsRemoved(const QModelIndex& parent, int first, int last);
    void slotReset();
};

enum Pos {Top, Left, Right, Bottom};
static constexpr const Qt::Edge edgeMap[] = {
    Qt::TopEdge, Qt::LeftEdge, Qt::RightEdge, Qt::BottomEdge
};

Viewport::Viewport(ModelAdapter* ma) : QObject(),
    s_ptr(new ViewportSync()), d_ptr(new ViewportPrivate())
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
    connect(ma, &ModelAdapter::delegateChanged, d_ptr->m_pReflector, [this]() {
        d_ptr->m_pReflector->performAction(
            TreeTraversalReflector::TrackingAction::RESET
        );
    });

    d_ptr->slotModelChanged(ma->rawModel(), nullptr);

    connect(d_ptr->m_pReflector, &TreeTraversalReflector::contentChanged,
        this, &Viewport::contentChanged);
}

QRectF Viewport::currentRect() const
{
    return d_ptr->m_UsedRect;
}

QSizeF AbstractItemAdapter::sizeHint() const
{
    Q_ASSERT(false);
    /*return s_ptr->m_pRange->d_ptr->sizeHint(
        const_cast<AbstractItemAdapter*>(this)
    );*/
    return {};
}

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
    Q_UNUSED(m)
    if (o)
        disconnect(o, &QAbstractItemModel::dataChanged,
            this, &ViewportPrivate::slotDataChanged);
}

void ViewportPrivate::slotModelChanged(QAbstractItemModel* m, QAbstractItemModel* o)
{
    if (o) {
        disconnect(m, &QAbstractItemModel::rowsInserted,
            this, &ViewportPrivate::slotRowsInserted);
        disconnect(m, &QAbstractItemModel::rowsAboutToBeRemoved,
            this, &ViewportPrivate::slotRowsRemoved);
        disconnect(m, &QAbstractItemModel::modelReset,
            this, &ViewportPrivate::slotReset);
        disconnect(m, &QAbstractItemModel::layoutChanged,
            this, &ViewportPrivate::slotReset);
    }

    m_pModelAdapter->view()->setCurrentY(0);

    m_pReflector->setModel(m);

    Q_ASSERT(m_pModelAdapter->rawModel() == m);

    if (m_pModelAdapter->hasSizeHints())
        q_ptr->setSizeHintStrategy(Viewport::SizeHintStrategy::PROXY);

    if (!m_SizeHintRole.isEmpty() && m)
        m_SizeHintRoleIndex = m->roleNames().key(
            m_SizeHintRole
        );

    if (m)
        connect(m, &QAbstractItemModel::dataChanged,
            this, &ViewportPrivate::slotDataChanged);

    if (m_ViewRect.size().isValid()) {
        m_pReflector->performAction(TreeTraversalReflector::TrackingAction::POPULATE);
        m_pReflector->performAction(TreeTraversalReflector::TrackingAction::ENABLE);
    }

    using SHS = Viewport::SizeHintStrategy;

    if (m && (m_SizeStrategy == SHS::PROXY || m_SizeStrategy == SHS::UNIFORM || m_SizeStrategy == SHS::ROLE)) {
        connect(m, &QAbstractItemModel::rowsInserted,
            this, &ViewportPrivate::slotRowsInserted);
        connect(m, &QAbstractItemModel::rowsAboutToBeRemoved,
            this, &ViewportPrivate::slotRowsRemoved);
        connect(m, &QAbstractItemModel::modelReset,
            this, &ViewportPrivate::slotReset);
        connect(m, &QAbstractItemModel::layoutChanged,
            this, &ViewportPrivate::slotReset);

        slotReset();
    }
}

void ViewportPrivate::slotViewportChanged(const QRectF &viewport)
{
//     Q_ASSERT(viewport.y() == 0); //FIXME I broke it
    m_ViewRect = viewport;
    m_UsedRect = viewport; //FIXME remove wrong
    updateAvailableEdges();
    m_pReflector->performAction(TreeTraversalReflector::TrackingAction::MOVE);
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
    using SHS = Viewport::SizeHintStrategy;
    const auto old = d_ptr->m_SizeStrategy;

    auto needConnect = [](SHS s) {
        return s == SHS::PROXY || s == SHS::UNIFORM || s == SHS::ROLE;
    };

    bool wasConnected = needConnect(old);
    bool willConnect  = needConnect( s );

    d_ptr->m_pReflector->performAction(TreeTraversalReflector::TrackingAction::RESET);
    d_ptr->m_SizeStrategy = s;

    const auto m = d_ptr->m_pModelAdapter->rawModel();

    if (m && wasConnected && !willConnect) {
        disconnect(m, &QAbstractItemModel::rowsInserted,
            d_ptr, &ViewportPrivate::slotRowsInserted);
        disconnect(m, &QAbstractItemModel::rowsAboutToBeRemoved,
            d_ptr, &ViewportPrivate::slotRowsRemoved);
        disconnect(m, &QAbstractItemModel::modelReset,
            d_ptr, &ViewportPrivate::slotReset);
        disconnect(m, &QAbstractItemModel::layoutChanged,
            d_ptr, &ViewportPrivate::slotReset);
    }
    else if (d_ptr->m_pModelAdapter->rawModel() && willConnect && !wasConnected) {
        connect(m, &QAbstractItemModel::rowsInserted,
            d_ptr, &ViewportPrivate::slotRowsInserted);
        connect(m, &QAbstractItemModel::rowsAboutToBeRemoved,
            d_ptr, &ViewportPrivate::slotRowsRemoved);
        connect(m, &QAbstractItemModel::modelReset,
            d_ptr, &ViewportPrivate::slotReset);
        connect(m, &QAbstractItemModel::layoutChanged,
            d_ptr, &ViewportPrivate::slotReset);
    }
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
void ViewportPrivate::slotDataChanged(const QModelIndex& tl, const QModelIndex& br, const QVector<int> &roles)
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
        if (auto item = m_pReflector->geometryForIndex(idx)) {
            // Prevent performing action if we know the changes wont affect the
            // view.
            if (item->contextAdapter()->updateRoles(roles)) {
                // (maybe) dismiss geometry cache
                q_ptr->s_ptr->notifyChange(item);

                if (auto vi = item->viewTracker())
                    vi->performAction(VisualTreeItem::ViewAction::UPDATE);
            }
        }
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
    return d_ptr->m_pReflector->availableEdges(
        TreeTraversalReflector::EdgeType::FREE
    );
}

void ViewportPrivate::updateAvailableEdges()
{
    if (!m_pReflector->model())
        return;

    Qt::Edges available;

    auto v = m_pModelAdapter->view();
    auto e = m_pReflector->getEdge(
        TreeTraversalReflector::EdgeType::VISIBLE, Qt::BottomEdge
    );

    // Size is normal as the state as not converged yet
    Q_ASSERT((!e) || (
        e->removeMe() == (int)GeometryCache::State::VALID ||
        e->removeMe() == (int)GeometryCache::State::SIZE)
    );

    // Resize the contend height
    if (e && m_SizeStrategy == Viewport::SizeHintStrategy::JIT) {

        const auto geo = e->decoratedGeometry();

        v->contentItem()->setHeight(std::max(
            geo.y()+geo.height(), v->height()
        ) + 200);

        emit v->contentHeightChanged( v->contentItem()->height() );
    }

    auto tve = m_pReflector->getEdge(
        TreeTraversalReflector::EdgeType::VISIBLE, Qt::TopEdge
    );

    auto bve = m_pReflector->getEdge(
        TreeTraversalReflector::EdgeType::VISIBLE, Qt::BottomEdge
    );

    // If they don't have a valid size, then there is a bug elsewhere
    Q_ASSERT((!tve) || tve->isValid());
    Q_ASSERT((!bve) || tve->isValid());

    // Do not attempt to load the geometry yet, let the loading code do it later
    bool tveValid = tve && tve->isValid();
    bool bveValid = bve && bve->isValid();

    // Given 42x0 sized item are possible. However just "fixing" this by adding
    // a minimum size wont help because it will trigger the out of sync view
    // correction in an infinite loop.
    QRectF tvg(tve?tve->decoratedGeometry():QRectF()), bvg(bve?bve->decoratedGeometry():QRectF());

    const auto fixedIntersect = [](bool valid, QRectF& vp, QRectF& geo) -> bool {
        return vp.intersects(geo) || (
            geo.y() >= vp.y() && geo.height() == 0 && geo.y() <= vp.y() + geo.height()
        );
    };

    if ((!tve) || fixedIntersect(tveValid, m_ViewRect, tvg))
        available |= Qt::TopEdge;

    if ((!bve) || fixedIntersect(bveValid, m_ViewRect, bvg))
        available |= Qt::BottomEdge;

    qDebug() << "=====" << (bool)(available & Qt::TopEdge)
        << (bool)(available & Qt::BottomEdge) << tvg << bvg;

    m_pReflector->setAvailableEdges(
        available, TreeTraversalReflector::EdgeType::FREE
    );

    m_pReflector->setAvailableEdges(
        available, TreeTraversalReflector::EdgeType::VISIBLE
    );

//     Q_ASSERT((~hasInvisible)&available == available || !available);
//     m_pReflector->setAvailableEdges(
//         (~hasInvisible)&15, TreeTraversalReflector::EdgeType::VISIBLE
//     );
//     qDebug() << "CHECK TOP" <<available << tve << (tve ? tve->geometry() : QRectF()) << (tve ? m_ViewRect.intersects(tve->geometry()) : true) << m_ViewRect;

    m_pReflector->performAction(TreeTraversalReflector::TrackingAction::TRIM);
}

void ViewportSync::geometryUpdated(IndexMetadata *item)
{
    //TODO assert if the size hints don't match reality

    auto geo = item->decoratedGeometry();

//     if (!geo.isValid()) {
//         item->m_State.setSize(q_ptr->d_ptr->sizeHint(item));
//         geo = item->geometry();
//     }

//     Q_ASSERT((!item->viewTracker()) || item->viewTracker()->geometry().size() == geo.size());

    // Update the used rect
    const QRectF r = q_ptr->d_ptr->m_UsedRect = q_ptr->d_ptr->m_UsedRect.united(geo);

    const bool hasSpaceOnTop = q_ptr->d_ptr->m_ViewRect.y();

    if (q_ptr->d_ptr->m_SizeStrategy == Viewport::SizeHintStrategy::JIT)
        q_ptr->d_ptr->updateAvailableEdges();
}

void ViewportSync::updateGeometry(IndexMetadata* item)
{
    if (q_ptr->d_ptr->m_SizeStrategy != Viewport::SizeHintStrategy::JIT)
        item->sizeHint();

    notifyInsert(item->down());

    refreshVisible();

    q_ptr->d_ptr->updateAvailableEdges();
    //notifyInsert(item->down());

}

// When the QModelIndex role change
void ViewportSync::notifyChange(IndexMetadata* item)
{
    switch(q_ptr->d_ptr->m_SizeStrategy) {
        case Viewport::SizeHintStrategy::UNIFORM:
        case Viewport::SizeHintStrategy::DELEGATE: //TODO
        case Viewport::SizeHintStrategy::ROLE: //TODO
            return;
        case Viewport::SizeHintStrategy::AOT:
        case Viewport::SizeHintStrategy::JIT:
        case Viewport::SizeHintStrategy::PROXY:
            item->performAction(IndexMetadata::GeometryAction::MODIFY);
    }
}

void ViewportSync::notifyRemoval(IndexMetadata* item)
{

//     auto tve = m_pReflector->getEdge(
//         TreeTraversalReflector::EdgeType::VISIBLE, Qt::TopEdge
//     );

//     auto bve = q_ptr->d_ptr->m_pReflector->getEdge(
//         TreeTraversalReflector::EdgeType::VISIBLE, Qt::BottomEdge
//     );
//     Q_ASSERT(item != bve); //TODO

//     //FIXME this is horrible
//     while(auto next = item->down()) { //FIXME dead code
//         if (next->m_State.state() != GeometryCache::State::VALID ||
//           next->m_State.state() != GeometryCache::State::POSITION)
//             break;
//
//         next->m_State.performAction(
//             GeometryAction::MOVE, nullptr, nullptr
//         );
//
//         Q_ASSERT(next != bve); //TODO
//
//         Q_ASSERT(next->m_State.state() != GeometryCache::State::VALID);
//     }
//
//     refreshVisible();
//
//     q_ptr->d_ptr->updateAvailableEdges();
}

void ViewportSync::refreshVisible()
{
    qDebug() << "\n\nREFRESH!!!";
    IndexMetadata *item = q_ptr->d_ptr->m_pReflector->getEdge(
        TreeTraversalReflector::EdgeType::VISIBLE, Qt::TopEdge
    );

    if (!item)
        return;

    auto bve = q_ptr->d_ptr->m_pReflector->getEdge(
        TreeTraversalReflector::EdgeType::VISIBLE, Qt::BottomEdge
    );

    auto prev = item->up();

    // First, make sure the previous elem has a valid size. If, for example,
    // rowsMoved moves a previously unloaded item to the front, this information
    // will be lost.
    if (prev && !prev->isValid()) {

        // This is the slow path, it can be /very/ slow. Possible mitigation
        // include adding more lower level methods to never lose track of the
        // list state, but this makes everything more (too) complex. Another
        // better solution is more aggressive unloading to the list becomes
        // smaller.
        if (!prev->isTopItem()) {
            qDebug() << "Slow path";
            //FIXME this is slow
            auto i = prev;
            while((i = i->up()) && i && !i->isValid());
            Q_ASSERT(i);

            while ((i = i->down()) != item)
                i->decoratedGeometry();
        }
        else
            prev->setPosition({0.0, 0.0});
    }

    const bool hasSingleItem = item == bve;

    do {
        Q_ASSERT(item->removeMe() != (int)GeometryCache::State::INIT);
        Q_ASSERT(item->removeMe() != (int)GeometryCache::State::POSITION);

        item->sizeHint();
        Q_ASSERT(item->isValid());

        if (!item->isInSync())
            item->performAction(IndexMetadata::TrackingAction::MOVE);
//         qDebug() << "R" << item << item->down();

    } while((!hasSingleItem) && item->up() != bve && (item = item->down()));
}

void ViewportSync::notifyInsert(IndexMetadata* item)
{
    if (!item)
        return;

    auto bve = q_ptr->d_ptr->m_pReflector->getEdge(
        TreeTraversalReflector::EdgeType::VISIBLE, Qt::BottomEdge
    );

//     qDebug() << "START" << item;
    //FIXME this is also horrible
    do {
        if (item == bve) {
            item->performAction(IndexMetadata::GeometryAction::MOVE);
//             qDebug() << "BREAK1" << item << item->m_pTTI << (item->down() != nullptr? (int) item->down()->m_State.state() : -1);
            break;
        }

        if (!item->isValid() &&
          item->removeMe() != (int)GeometryCache::State::POSITION) {
//             qDebug() << "BREAK2";
            break;
        }

        item->performAction(IndexMetadata::GeometryAction::MOVE);

        Q_ASSERT(item->isValid());
//         qDebug() << "ONE" << (int) item->removeMe();
    } while((item = item->down()));
}

void Viewport::resize(const QRectF& rect)
{
    const bool wasValid = d_ptr->m_ViewRect.size().isValid();

    Q_ASSERT(rect.x() == 0);

    // The {x, y} may not be at {0, 0}, but given it is a relative viewport,
    // then the content doesn't care about where it is on the screen.
    d_ptr->m_ViewRect = rect;
    Q_ASSERT(rect.y() == 0);

    // For now ignore the case where the content is smaller, it doesn't change
    // anything. This could eventually change
    d_ptr->m_UsedRect.setSize(rect.size()); //FIXME remove, wrong

    d_ptr->updateAvailableEdges();

    if ((!wasValid) && rect.isValid()) {
        d_ptr->m_pReflector->performAction(TreeTraversalReflector::TrackingAction::POPULATE);
        d_ptr->m_pReflector->performAction(TreeTraversalReflector::TrackingAction::ENABLE);
    }
    else if (rect.isValid()) {
        d_ptr->m_pReflector->performAction(TreeTraversalReflector::TrackingAction::MOVE);
    }
}

qreal ViewportPrivate::getSectionHeight(const QModelIndex& parent, int first, int last)
{
    Q_ASSERT(m_pModelAdapter->rawModel());
    switch(m_SizeStrategy) {
        case Viewport::SizeHintStrategy::AOT:
        case Viewport::SizeHintStrategy::JIT:
        case Viewport::SizeHintStrategy::DELEGATE:
            Q_ASSERT(false); // Should not get here
            return 0.0;
        case Viewport::SizeHintStrategy::UNIFORM:
            Q_ASSERT(m_KnowsRowHeight);
            break;
        case Viewport::SizeHintStrategy::PROXY: {
            Q_ASSERT(q_ptr->modelAdapter()->hasSizeHints());
            const auto idx = m_pModelAdapter->rawModel()->index(first, 0, parent);
            return qobject_cast<SizeHintProxyModel*>(m_pModelAdapter->rawModel())
                ->sizeHintForIndex(idx).height();
        }
        case Viewport::SizeHintStrategy::ROLE:
            Q_ASSERT(false); //TODO
            break;
    }

    Q_ASSERT(false);
    return 0.0;
}

void ViewportPrivate::applyDelayedSize()
{
    if (m_DisableApply)
        return;

    m_CurrentHeight += m_DelayedHeightDelta;
    m_DelayedHeightDelta = 0;

    m_pModelAdapter->view()->contentItem()->setHeight(m_CurrentHeight);
}

void ViewportPrivate::slotRowsInserted(const QModelIndex& parent, int first, int last)
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

void ViewportPrivate::slotRowsRemoved(const QModelIndex& parent, int first, int last)
{
    //
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

void ViewportPrivate::slotReset()
{
    m_DisableApply = true;
    m_CurrentHeight = 0.0;
    if (int rc = m_pModelAdapter->rawModel()->rowCount())
        slotRowsInserted({}, 0, rc - 1);
    m_DisableApply = false;
    applyDelayedSize();
}

#include <viewport.moc>
