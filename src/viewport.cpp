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
#include "private/viewport_p.h"
#include "proxies/sizehintproxymodel.h"
#include "private/treetraversalreflector_p.h"
#include "adapters/modeladapter.h"
#include "contextadapterfactory.h"
#include "adapters/contextadapter.h"
#include "private/statetracker/viewitem_p.h"
#include "private/statetracker/model_p.h"
#include "adapters/abstractitemadapter.h"
#include "viewbase.h"
#include "private/indexmetadata_p.h"


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
    s_ptr->m_pReflector    = new TreeTraversalReflector(this);

    resize(QRectF { 0.0, 0.0, ma->view()->width(), ma->view()->height() });

    connect(ma, &ModelAdapter::modelChanged,
        d_ptr, &ViewportPrivate::slotModelChanged);
    connect(ma->view(), &Flickable::viewportChanged,
        d_ptr, &ViewportPrivate::slotViewportChanged);
    connect(ma, &ModelAdapter::delegateChanged, s_ptr->m_pReflector, [this]() {
        s_ptr->m_pReflector->modelTracker()->performAction(
            StateTracker::Model::Action::RESET
        );
    });

    d_ptr->slotModelChanged(ma->rawModel(), nullptr);

    connect(s_ptr->m_pReflector, &TreeTraversalReflector::contentChanged,
        this, &Viewport::contentChanged);
}

QRectF Viewport::currentRect() const
{
    return d_ptr->m_UsedRect;
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

    q_ptr->s_ptr->m_pReflector->setModel(m);

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
        q_ptr->s_ptr->m_pReflector->modelTracker()
         << StateTracker::Model::Action::POPULATE
         << StateTracker::Model::Action::ENABLE;
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
    q_ptr->s_ptr->m_pReflector->modelTracker() << StateTracker::Model::Action::MOVE;
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

    s_ptr->m_pReflector->modelTracker() << StateTracker::Model::Action::RESET;
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
    return s_ptr->m_pReflector->itemForIndex(idx);
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

    if (!q_ptr->s_ptr->m_pReflector->isActive(tl.parent(), tl.row(), br.row()))
        return;

    //FIXME tolerate other cases
    Q_ASSERT(m_pModelAdapter->rawModel());
    Q_ASSERT(tl.model() == m_pModelAdapter->rawModel() && br.model() == m_pModelAdapter->rawModel());
    Q_ASSERT(tl.parent() == br.parent());

    //TODO Use a smaller range when possible

    //itemForIndex(const QModelIndex& idx) const final override;
    for (int i = tl.row(); i <= br.row(); i++) {
        const auto idx = m_pModelAdapter->rawModel()->index(i, tl.column(), tl.parent());
        if (auto item = q_ptr->s_ptr->m_pReflector->geometryForIndex(idx)) {
            // Prevent performing action if we know the changes wont affect the
            // view.
            if (item->contextAdapter()->updateRoles(roles)) {
                // (maybe) dismiss geometry cache
                q_ptr->s_ptr->notifyChange(item);

                if (item->viewTracker())
                    item << IndexMetadata::ViewAction::UPDATE;
            }
        }
    }
}

void Viewport::setItemFactory(ViewBase::ItemFactoryBase *factory)
{
    s_ptr->m_pReflector->setItemFactory([this, factory]() -> AbstractItemAdapter* {
        return factory->create(this);
    });
}

Qt::Edges Viewport::availableEdges() const
{
    return s_ptr->m_pReflector->availableEdges(
        IndexMetadata::EdgeType::FREE
    );
}

void ViewportPrivate::updateAvailableEdges()
{
    if (q_ptr->s_ptr->m_pReflector->modelTracker()->state() == StateTracker::Model::State::RESETING)
        return; //TODO it needs another state machine to get rid of the `if`

    if (!q_ptr->s_ptr->m_pReflector->model())
        return;

    Qt::Edges available;

    auto v = m_pModelAdapter->view();

    auto tve = q_ptr->s_ptr->m_pReflector->getEdge(
        IndexMetadata::EdgeType::VISIBLE, Qt::TopEdge
    );

    Q_ASSERT((!tve) || (!tve->up()) || (!tve->up()->isVisible()));

    auto bve = q_ptr->s_ptr->m_pReflector->getEdge(
        IndexMetadata::EdgeType::VISIBLE, Qt::BottomEdge
    );

    Q_ASSERT((!bve) || (!bve->down()) || (!bve->down()->isVisible()));

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
        Q_UNUSED(valid) //TODO
        return vp.intersects(geo) || (
            geo.y() >= vp.y() && geo.height() == 0 && geo.y() <= vp.y() + vp.height()
        );
    };

    QRectF vp = m_ViewRect;

    // Add an extra pixel to the height to prevent off-by-one where the view is
    // perfectly full and can't scroll any more (and thus load the next item)
    vp.setHeight(vp.height()+1.0);

    if ((!tve) || (fixedIntersect(tveValid, vp, tvg) && tvg.y() > 0))
        available |= Qt::TopEdge;

    if ((!bve) || fixedIntersect(bveValid, vp, bvg))
        available |= Qt::BottomEdge;

    q_ptr->s_ptr->m_pReflector->setAvailableEdges(
        available, IndexMetadata::EdgeType::FREE
    );

    q_ptr->s_ptr->m_pReflector->setAvailableEdges(
        available, IndexMetadata::EdgeType::VISIBLE
    );

//     Q_ASSERT((~hasInvisible)&available == available || !available);
//     m_pReflector->setAvailableEdges(
//         (~hasInvisible)&15, IndexMetadata::EdgeType::VISIBLE
//     );

    q_ptr->s_ptr->m_pReflector->modelTracker() << StateTracker::Model::Action::TRIM;

    bve = q_ptr->s_ptr->m_pReflector->getEdge(
        IndexMetadata::EdgeType::VISIBLE, Qt::BottomEdge
    );

    //BEGIN test
    tve = q_ptr->s_ptr->m_pReflector->getEdge(
        IndexMetadata::EdgeType::VISIBLE, Qt::TopEdge
    );

    Q_ASSERT((!tve) || (!tve->up()) || (!tve->up()->isVisible()));


    Q_ASSERT((!bve) || (!bve->down()) || (!bve->down()->isVisible()));
    //END test

    // Size is normal as the state as not converged yet
    Q_ASSERT((!bve) || (
        bve->removeMe() == (int)StateTracker::Geometry::State::VALID ||
        bve->removeMe() == (int)StateTracker::Geometry::State::SIZE)
    );

    // Resize the contend height, it has to be done after the geometry has been
    // updated.
    if (bve && m_SizeStrategy == Viewport::SizeHintStrategy::JIT) {

        const auto geo = bve->decoratedGeometry();

        v->contentItem()->setHeight(std::max(
            geo.y()+geo.height(), v->height()
        ));

        emit v->contentHeightChanged( v->contentItem()->height() );
    }
}

void ViewportSync::geometryUpdated(IndexMetadata *item)
{
    if (m_pReflector->modelTracker()->state() == StateTracker::Model::State::RESETING)
        return; //TODO it needs another state machine to get rid of the `if`

    //TODO assert if the size hints don't match reality

    if (q_ptr->d_ptr->m_SizeStrategy == Viewport::SizeHintStrategy::JIT)
        q_ptr->d_ptr->updateAvailableEdges();
}

void ViewportSync::updateGeometry(IndexMetadata* item)
{
    if (q_ptr->d_ptr->m_SizeStrategy != Viewport::SizeHintStrategy::JIT)
        item->sizeHint();

    if (auto i = item->down())
        notifyInsert(i);

    q_ptr->d_ptr->updateAvailableEdges();

    refreshVisible();
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
            item << IndexMetadata::GeometryAction::MODIFY;
    }
}

void ViewportSync::notifyRemoval(IndexMetadata* item)
{
    Q_UNUSED(item)
    if (m_pReflector->modelTracker()->state() == StateTracker::Model::State::RESETING)
        return; //TODO it needs another state machine to get rid of the `if`

//     auto tve = m_pReflector->getEdge(
//         IndexMetadata::EdgeType::VISIBLE, Qt::TopEdge
//     );

//     auto bve = q_ptr->d_ptr->m_pReflector->getEdge(
//         IndexMetadata::EdgeType::VISIBLE, Qt::BottomEdge
//     );
//     Q_ASSERT(item != bve); //TODO

//     //FIXME this is horrible
//     while(auto next = item->down()) { //FIXME dead code
//         if (next->m_State.state() != StateTracker::Geometry::State::VALID ||
//           next->m_State.state() != StateTracker::Geometry::State::POSITION)
//             break;
//
//         next->m_State.performAction(
//             GeometryAction::MOVE, nullptr, nullptr
//         );
//
//         Q_ASSERT(next != bve); //TODO
//
//         Q_ASSERT(next->m_State.state() != StateTracker::Geometry::State::VALID);
//     }
//
//     refreshVisible();
//
//     q_ptr->d_ptr->updateAvailableEdges();
}

//TODO temporary hack to avoid having to implement a complex way to move the
// limited number of loaded elements. Given once trimming is enabled, the
// number of visible items should be fairely low/constant, it is a good enough
// temporary solution.
void ViewportSync::refreshVisible()
{
    if (m_pReflector->modelTracker()->state() == StateTracker::Model::State::RESETING)
        return; //TODO it needs another state machine to get rid of the `if`

    //TODO eventually move to a relative origin so moving an item to the top
    // doesn't need to move everything

    IndexMetadata *item = m_pReflector->getEdge(
        IndexMetadata::EdgeType::VISIBLE, Qt::TopEdge
    );

    if (!item)
        return;

    Q_ASSERT((!item->up()) || !item->up()->isVisible());

    auto bve = m_pReflector->getEdge(
        IndexMetadata::EdgeType::VISIBLE, Qt::BottomEdge
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
        item->sizeHint();

        Q_ASSERT(item->removeMe() != (int)StateTracker::Geometry::State::INIT);
        Q_ASSERT(item->removeMe() != (int)StateTracker::Geometry::State::POSITION);

        if (prev) {
            //FIXME this isn't ok, it needs to take into account the point size
            //it could by multiple pixels
            item->setPosition(prev->decoratedGeometry().bottomLeft());
        }

        item->sizeHint();
        Q_ASSERT(item->isVisible());
        Q_ASSERT(item->isValid());
        //FIXME As of 0.1, this still fails from time to time
        //Q_ASSERT(item->isInSync());//TODO THIS_COMMIT

        // This `performAction` exists to recover from runtime failures
        if (!item->isInSync())
            item << IndexMetadata::LoadAction::MOVE;

    } while((!hasSingleItem) && item->up() != bve && (item = item->down()));
}

void ViewportSync::notifyInsert(IndexMetadata* item)
{
    using GeoState = StateTracker::Geometry::State;
    Q_ASSERT(item);

    if (m_pReflector->modelTracker()->state() == StateTracker::Model::State::RESETING)
        return; //TODO it needs another state machine to get rid of the `if`

    if (!item)
        return;

    const bool needsPosition = item->removeMe() == (int)GeoState::INIT ||
      item->removeMe() == (int)GeoState::SIZE;

    // If the item is new and is inserted near valid items, skip some back and
    // forth and set the position now.
    if (needsPosition && item->up() && item->up()->removeMe() == (int) GeoState::VALID) {
        item->setPosition(item->up()->decoratedGeometry().bottomLeft());
    }

    // If the item is inserted in front, set the position
    if (item->isTopItem()) {
        item->setPosition({0.0, 0.0});
    }

    auto bve = m_pReflector->getEdge(
        IndexMetadata::EdgeType::VISIBLE, Qt::BottomEdge
    );

    //FIXME this is also horrible
    do {
        if (item == bve) {
            item << IndexMetadata::GeometryAction::MOVE;
            break;
        }

        if (!item->isValid() &&
          item->removeMe() != (int)GeoState::POSITION) {
            break;
        }

        item << IndexMetadata::GeometryAction::MOVE;
    } while((item = item->down()));

    refreshVisible();

    q_ptr->d_ptr->updateAvailableEdges();
}

void Viewport::resize(const QRectF& rect)
{
    if (s_ptr->m_pReflector->modelTracker()->state() == StateTracker::Model::State::RESETING)
        return; //TODO it needs another state machine to get rid of the `if`

    const bool wasValid = d_ptr->m_ViewRect.size().isValid();

    Q_ASSERT(rect.x() == 0);

    // The {x, y} may not be at {0, 0}, but given it is a relative viewport,
    // then the content doesn't care about where it is on the screen.
    d_ptr->m_ViewRect = rect;
    Q_ASSERT(rect.y() == 0);

    // For now ignore the case where the content is smaller, it doesn't change
    // anything. This could eventually change
    d_ptr->m_UsedRect.setSize(rect.size()); //FIXME remove, wrong

    s_ptr->refreshVisible();

    d_ptr->updateAvailableEdges();

    if ((!wasValid) && rect.isValid()) {
        s_ptr->m_pReflector->modelTracker()
            << StateTracker::Model::Action::POPULATE
            << StateTracker::Model::Action::ENABLE;
    }
    else if (rect.isValid()) {
        s_ptr->m_pReflector->modelTracker() << StateTracker::Model::Action::MOVE;
    }
}

qreal ViewportPrivate::getSectionHeight(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(last) //TODO not implemented
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
            Q_ASSERT(false); //TODO not implemented
            break;
    }

    Q_ASSERT(false);
    return 0.0;
}

//TODO add a way to allow model to provide this information manually and
// generally don't cleanup when this code-path is used. Maybe trun into an
// adapter

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
