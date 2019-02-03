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
#include "private/statetracker/content_p.h"
#include "adapters/modeladapter.h"
#include "adapters/viewportadapter.h"
#include "contextadapterfactory.h"
#include "adapters/contextadapter.h"
#include "private/statetracker/viewitem_p.h"
#include "private/statetracker/model_p.h"
#include "adapters/abstractitemadapter.h"
#include "viewbase.h"
#include "private/indexmetadata_p.h"
#include "private/geostrategyselector_p.h"

class ViewportPrivate : public QObject
{
    Q_OBJECT
public:
    QQmlEngine          *m_pEngine       {    nullptr    };
    ModelAdapter        *m_pModelAdapter {    nullptr    };
    ViewportAdapter     *m_pViewAdapter  {    nullptr    };

    // The viewport rectangle
    QRectF m_ViewRect;
    QRectF m_UsedRect;

    void updateAvailableEdges();

    Viewport *q_ptr;

public Q_SLOTS:
    void slotModelChanged(QAbstractItemModel* m, QAbstractItemModel* o);
    void slotModelAboutToChange(QAbstractItemModel* m, QAbstractItemModel* o);
    void slotViewportChanged(const QRectF &viewport);
};

Viewport::Viewport(ModelAdapter* ma) : QObject(),
    s_ptr(new ViewportSync()), d_ptr(new ViewportPrivate())
{
    d_ptr->q_ptr           = this;
    s_ptr->q_ptr           = this;
    d_ptr->m_pModelAdapter = ma;
    s_ptr->m_pReflector    = new StateTracker::Content(this);
    s_ptr->m_pGeoAdapter   = new GeoStrategySelector(this);

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

    connect(s_ptr->m_pReflector, &StateTracker::Content::contentChanged,
        this, &Viewport::contentChanged);
}

Viewport::~Viewport()
{
    delete s_ptr;
    delete d_ptr;
}

QRectF Viewport::currentRect() const
{
    return d_ptr->m_UsedRect;
}

void ViewportPrivate::slotModelAboutToChange(QAbstractItemModel* m, QAbstractItemModel* o)
{
    Q_UNUSED(m)
}

void ViewportPrivate::slotModelChanged(QAbstractItemModel* m, QAbstractItemModel* o)
{
    Q_UNUSED(o)
    m_pModelAdapter->view()->setCurrentY(0);

    q_ptr->s_ptr->m_pReflector->modelTracker()->setModel(m);

    Q_ASSERT(m_pModelAdapter->rawModel() == m);

    q_ptr->s_ptr->m_pGeoAdapter->setModel(m);

    if (m && m_ViewRect.size().isValid() && m_pModelAdapter->delegate()) {
        q_ptr->s_ptr->m_pReflector->modelTracker()
         << StateTracker::Model::Action::POPULATE
         << StateTracker::Model::Action::ENABLE;
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

QSizeF Viewport::totalSize() const
{
    if ((!d_ptr->m_pModelAdapter->delegate()) || !d_ptr->m_pModelAdapter->rawModel())
        return {0.0, 0.0};

    return {}; //TODO
}

void Viewport::setItemFactory(ViewBase::ItemFactoryBase *factory)
{
    s_ptr->m_fFactory = ([this, factory]() -> AbstractItemAdapter* {
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

    if (!q_ptr->s_ptr->m_pReflector->modelTracker()->modelCandidate())
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
    const bool tveValid = tve && tve->isValid();
    const bool bveValid = bve && bve->isValid();

    // Given 42x0 sized item are possible. However just "fixing" this by adding
    // a minimum size wont help because it will trigger the out of sync view
    // correction in an infinite loop.
    QRectF tvg(tve?tve->decoratedGeometry():QRectF()), bvg(bve?bve->decoratedGeometry():QRectF());

    const auto fixedIntersect = [](bool valid, QRectF& vp, QRectF& geo) -> bool {
        Q_UNUSED(valid) //TODO
        return vp.intersects(geo) || (
            geo.y() >= vp.y() && geo.width() == 0 && geo.y() <= vp.y() + vp.height()
        ) || (
            geo.height() == 0 && geo.width() > 0  && geo.x() <= vp.x() + vp.width()
            && geo.x() >= vp.x()
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
        bve->geometryTracker()->state() == StateTracker::Geometry::State::VALID ||
        bve->geometryTracker()->state() == StateTracker::Geometry::State::SIZE)
    );

    // Resize the contend height, it has to be done after the geometry has been
    // updated.
    if (bve && q_ptr->s_ptr->m_pGeoAdapter->capabilities() & GeometryAdapter::Capabilities::TRACKS_QQUICKITEM_GEOMETRY) {

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

    // This will recompute the geometry
    item->decoratedGeometry();

    if (m_pGeoAdapter->capabilities() & GeometryAdapter::Capabilities::TRACKS_QQUICKITEM_GEOMETRY)
        q_ptr->d_ptr->updateAvailableEdges();
}

void ViewportSync::updateGeometry(IndexMetadata* item)
{
    if (m_pGeoAdapter->capabilities() & GeometryAdapter::Capabilities::TRACKS_QQUICKITEM_GEOMETRY)
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
    item << IndexMetadata::GeometryAction::MODIFY;
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

        Q_ASSERT(item->geometryTracker()->state() != StateTracker::Geometry::State::INIT);
        Q_ASSERT(item->geometryTracker()->state() != StateTracker::Geometry::State::POSITION);

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

    const bool needsPosition = item->geometryTracker()->state() == GeoState::INIT ||
      item->geometryTracker()->state() == GeoState::SIZE;

    // If the item is new and is inserted near valid items, skip some back and
    // forth and set the position now.
    if (needsPosition && item->up() && item->up()->geometryTracker()->state() ==  GeoState::VALID) {
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
          item->geometryTracker()->state() != GeoState::POSITION) {
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

    if ((!d_ptr->m_pModelAdapter) || (!d_ptr->m_pModelAdapter->rawModel()))
        return;

    if (!d_ptr->m_pModelAdapter->delegate())
        return;

    if (!rect.isValid())
        return;

    // Refresh the content
    if (!wasValid)
        s_ptr->m_pReflector->modelTracker()
            << StateTracker::Model::Action::POPULATE
            << StateTracker::Model::Action::ENABLE;
    else {
        //FIXME make sure the state machine handle the lack of delegate properly
        s_ptr->m_pReflector->modelTracker() << StateTracker::Model::Action::MOVE;
    }
}

IndexMetadata *ViewportSync::metadataForIndex(const QModelIndex& idx) const
{
    return m_pReflector->metadataForIndex(idx);
}

QQmlEngine *ViewportSync::engine()
{
    if (!m_pEngine)
        m_pEngine = q_ptr->modelAdapter()->view()->rootContext()->engine();

    return m_pEngine;
}

QQmlComponent *ViewportSync::component()
{
    engine();

    m_pComponent = new QQmlComponent(m_pEngine);
    m_pComponent->setData("import QtQuick 2.4; Item {property QtObject content: null;}", {});

    return m_pComponent;
}

GeometryAdapter *Viewport::geometryAdapter() const
{
    return s_ptr->m_pGeoAdapter;
}

void Viewport::setGeometryAdapter(GeometryAdapter *a)
{
    s_ptr->m_pGeoAdapter->setCurrentAdapter(a);
}

#include <viewport.moc>
