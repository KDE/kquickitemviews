/***************************************************************************
 *   Copyright (C) 2017 by Emmanuel Lepage Vallee                          *
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
#include "abstractitemadapter.h"

// libstdc++
#include <atomic>

// Qt
#include <QtCore/QTimer>
#include <QQmlContext>
#include <QQuickItem>
#include <QQmlEngine>

// KQuickItemViews
#include "private/statetracker/viewitem_p.h"
#include "adapters/selectionadapter.h"
#include "adapters/geometryadapter.h"
#include "private/selectionadapter_p.h"
#include "private/geostrategyselector_p.h"
#include "viewport.h"
#include "private/indexmetadata_p.h"
#include "private/viewport_p.h"
#include "modeladapter.h"
#include "private/statetracker/index_p.h"
#include "viewbase.h"
#include "contextadapterfactory.h"
#include "contextadapter.h"

class AbstractItemAdapterPrivate : public QObject
{
    Q_OBJECT
public:
    typedef bool(AbstractItemAdapterPrivate::*StateF)();

    // Helpers
    inline void load();

    // Actions
    bool attach ();
    bool refresh();
    bool move   ();
    bool flush  ();
    bool remove ();
    bool nothing();
    bool error  ();
    bool destroy();
    bool detach ();
    bool hide   ();

    static const StateTracker::ViewItem::State  m_fStateMap    [7][7];
    static const StateF                 m_fStateMachine[7][7];

    mutable QSharedPointer<AbstractItemAdapter::SelectionLocker> m_pLocker;

    mutable QQuickItem  *m_pContainer {nullptr};
    mutable QQuickItem  *m_pContent   {nullptr};
    mutable QQmlContext *m_pContext   {nullptr};

    // Helpers
    bool loadDelegate(QQuickItem* parentI) const;

    // Attributes
    AbstractItemAdapter* q_ptr;

public Q_SLOTS:
    void slotDestroyed();
};

/*
 * The visual elements state changes.
 *
 * Note that the ::FAILED elements will always try to self-heal themselves and
 * go back into FAILED once the self-healing itself failed.
 */
#define S StateTracker::ViewItem::State::
const StateTracker::ViewItem::State AbstractItemAdapterPrivate::m_fStateMap[7][7] = {
/*              ATTACH ENTER_BUFFER ENTER_VIEW UPDATE     MOVE   LEAVE_BUFFER   DETACH  */
/*POOLING */ { S POOLING, S BUFFER, S ERROR , S ERROR , S ERROR , S ERROR  , S POOLED   },
/*POOLED  */ { S POOLED , S BUFFER, S ACTIVE, S ERROR , S ERROR , S ERROR  , S DANGLING },
/*BUFFER  */ { S ERROR  , S ERROR , S ACTIVE, S BUFFER, S ERROR , S POOLING, S DANGLING },
/*ACTIVE  */ { S ERROR  , S BUFFER, S ERROR , S ACTIVE, S ACTIVE, S BUFFER , S POOLING  },
/*FAILED  */ { S ERROR  , S BUFFER, S ACTIVE, S ACTIVE, S ACTIVE, S POOLED , S DANGLING },
/*DANGLING*/ { S ERROR  , S ERROR , S ERROR , S ERROR , S ERROR , S ERROR  , S DANGLING },
/*ERROR   */ { S ERROR  , S ERROR , S ERROR , S ERROR , S ERROR , S ERROR  , S DANGLING },
};
#undef S

#define A &AbstractItemAdapterPrivate::
const AbstractItemAdapterPrivate::StateF AbstractItemAdapterPrivate::m_fStateMachine[7][7] = {
/*             ATTACH  ENTER_BUFFER  ENTER_VIEW   UPDATE     MOVE   LEAVE_BUFFER  DETACH  */
/*POOLING */ { A error  , A error  , A error  , A error  , A error  , A error  , A nothing },
/*POOLED  */ { A nothing, A attach , A move   , A error  , A error  , A error  , A destroy },
/*BUFFER  */ { A error  , A error  , A move   , A refresh, A error  , A detach , A destroy },
/*ACTIVE  */ { A error  , A nothing, A nothing, A refresh, A move   , A hide   , A detach  },
/*FAILED  */ { A error  , A nothing, A nothing, A nothing, A nothing, A nothing, A destroy },
/*DANGLING*/ { A error  , A error  , A error  , A error  , A error  , A error  , A destroy },
/*error   */ { A error  , A error  , A error  , A error  , A error  , A error  , A destroy },
};
#undef A

AbstractItemAdapter::AbstractItemAdapter(Viewport* r) :
    s_ptr(new StateTracker::ViewItem(r)), d_ptr(new AbstractItemAdapterPrivate)
{
    d_ptr->q_ptr = this;
    s_ptr->d_ptr = this;
}

AbstractItemAdapter::~AbstractItemAdapter()
{
    if (d_ptr->m_pContainer)
        delete d_ptr->m_pContainer;

    if (d_ptr->m_pContext)
        delete d_ptr->m_pContext;

    delete d_ptr;
}

ViewBase* AbstractItemAdapter::view() const
{
    return s_ptr->m_pViewport->modelAdapter()->view();
}

void AbstractItemAdapter::resetPosition()
{
    //
}

int AbstractItemAdapter::row() const
{
    return s_ptr->row();
}

int AbstractItemAdapter::column() const
{
    return s_ptr->column();
}

QPersistentModelIndex AbstractItemAdapter::index() const
{
    return s_ptr->index();
}

AbstractItemAdapter *AbstractItemAdapter::next(Qt::Edge e) const
{
    const auto i = s_ptr->next(e);
    return i ? i->d_ptr : nullptr;
}

AbstractItemAdapter* AbstractItemAdapter::parent() const
{
    return nullptr;
}

void AbstractItemAdapter::updateGeometry()
{
    s_ptr->updateGeometry();
}

void StateTracker::ViewItem::setSelected(bool v)
{
    d_ptr->setSelected(v);
}

QRectF StateTracker::ViewItem::currentGeometry() const
{
    return d_ptr->geometry();
}

QQuickItem* StateTracker::ViewItem::item() const
{
    return d_ptr->container();
}

bool AbstractItemAdapterPrivate::attach()
{
    if (m_pContainer)
        m_pContainer->setVisible(true);

    return q_ptr->attach();
}

bool AbstractItemAdapterPrivate::refresh()
{
    return q_ptr->refresh();
}

bool AbstractItemAdapterPrivate::move()
{
    const bool ret = q_ptr->move();

    // Views should apply the geometry they have been told to apply. Otherwise
    // all optimizations brakes.
//     Q_ASSERT((!ret) || q_ptr->s_ptr->m_pMetadata->isInSync());

    return ret;
}

bool AbstractItemAdapterPrivate::flush()
{
    return q_ptr->flush();
}

bool AbstractItemAdapterPrivate::remove()
{
    bool ret = q_ptr->remove();

    m_pContainer->setParentItem(nullptr);
    q_ptr->s_ptr->m_pMetadata = nullptr;

    return ret;
}

bool AbstractItemAdapterPrivate::hide()
{
    Q_ASSERT(m_pContainer);
    if (!m_pContainer)
        return false;

    m_pContainer->setVisible(false);

    return true;
}

// This methodwrap the removal of the element from the view
bool AbstractItemAdapterPrivate::detach()
{
    m_pContainer->setParentItem(nullptr);
    remove();

    //FIXME
    q_ptr->s_ptr->m_pMetadata << IndexMetadata::ViewAction::DETACH;
    Q_ASSERT(q_ptr->s_ptr->state() == StateTracker::ViewItem::State::POOLED);

    return true;
}

bool AbstractItemAdapterPrivate::nothing()
{
    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
bool AbstractItemAdapterPrivate::error()
{
    Q_ASSERT(false);
    return true;
}
#pragma GCC diagnostic pop

bool AbstractItemAdapterPrivate::destroy()
{
    auto ptrCopy = m_pLocker;

    //FIXME manage to add to the pool without a SEGFAULT
    if (m_pContainer) {
        m_pContainer->setParentItem(nullptr);
        delete m_pContainer;
    }

    m_pContainer = nullptr;
    m_pContent   = nullptr;

    if (ptrCopy) {
        QTimer::singleShot(0,[this, ptrCopy]() {
            if (!ptrCopy)
            // else the reference will be dropped and the destructor called
                delete q_ptr;
        });
        if (m_pLocker)
            m_pLocker.clear();
    }
    else {
        delete q_ptr;
    }

    return true;
}

bool StateTracker::ViewItem::performAction(IndexMetadata::ViewAction a)
{
    const int s = (int)m_State;

    m_State     = d_ptr->d_ptr->m_fStateMap [s][(int)a];
    Q_ASSERT(m_State != StateTracker::ViewItem::State::ERROR);

    return (d_ptr->d_ptr ->* d_ptr->d_ptr->m_fStateMachine[s][(int)a])();
}

int StateTracker::ViewItem::depth() const
{
    return m_pMetadata->indexTracker()->depth();
}

QPair<QWeakPointer<AbstractItemAdapter::SelectionLocker>, AbstractItemAdapter*>
AbstractItemAdapter::weakReference() const
{
    if (d_ptr->m_pLocker)
        return {d_ptr->m_pLocker, const_cast<AbstractItemAdapter*>(this)};

    d_ptr->m_pLocker = QSharedPointer<SelectionLocker>(new SelectionLocker());


    return {d_ptr->m_pLocker, const_cast<AbstractItemAdapter*>(this)};
}

void AbstractItemAdapterPrivate::load()
{
    if (m_pContext || m_pContainer)
        return;

    // Usually if we get here something already failed
    if (!q_ptr->s_ptr->m_pViewport->modelAdapter()->delegate()) {
        Q_ASSERT(false);
        qDebug() << "Cannot attach, there is no delegate";
        return;
    }

    loadDelegate(q_ptr->view()->contentItem());

    if (!m_pContainer) {
        qDebug() << "Item failed to load" << q_ptr->index().data();
        return;
    }

    if (!m_pContainer->z())
        m_pContainer->setZ(1);

    /*d()->m_DepthChart[depth()] = std::max(
        d()->m_DepthChart[depth()],
        pair.first->height()
    );*/

    // QtQuick can decide to destroy it even with C++ ownership, so be it
    connect(m_pContainer, &QObject::destroyed, this, &AbstractItemAdapterPrivate::slotDestroyed);

    Q_ASSERT(q_ptr->s_ptr->m_pMetadata);

    q_ptr->s_ptr->m_pMetadata->contextAdapter()->context();

    Q_ASSERT(q_ptr->s_ptr->m_pMetadata->contextAdapter()->context() == m_pContext);

    if (q_ptr->s_ptr->m_pViewport->s_ptr->m_pGeoAdapter->capabilities() & GeometryAdapter::Capabilities::HAS_AHEAD_OF_TIME) {
        q_ptr->s_ptr->m_pMetadata->performAction(
            IndexMetadata::GeometryAction::MODIFY
        );
    }
    else {
        //TODO it should still call the GeometryAdapter, but most of them are
        // currently too buggy for that to help.
        q_ptr->s_ptr->m_pMetadata->setSize(
            QSizeF(m_pContainer->width(), m_pContainer->height())
        );
    }

    Q_ASSERT(q_ptr->s_ptr->m_pMetadata->geometryTracker()->state() != StateTracker::Geometry::State::INIT);
}

QQmlContext *AbstractItemAdapter::context() const
{
    d_ptr->load();
    return d_ptr->m_pContext;
}

QQuickItem *AbstractItemAdapter::container() const
{
    d_ptr->load();
    return d_ptr->m_pContainer;
}

QQuickItem *AbstractItemAdapter::content() const
{
    d_ptr->load();
    return d_ptr->m_pContent;
}

Viewport *AbstractItemAdapter::viewport() const
{
    return s_ptr->m_pViewport;
}

QRectF AbstractItemAdapter::geometry() const
{
    if (!d_ptr->m_pContainer)
        return {};

    const QPointF p = container()->mapFromItem(view()->contentItem(), {0,0});
    return {
        -p.x(),
        -p.y(),
        container()->width(),
        container()->height()
    };
}

QRectF AbstractItemAdapter::decoratedGeometry() const
{
    return s_ptr->m_pMetadata->decoratedGeometry();
}

qreal AbstractItemAdapter::borderDecoration(Qt::Edge e) const
{
    return s_ptr->m_pMetadata->borderDecoration(e);
}

void AbstractItemAdapter::setBorderDecoration(Qt::Edge e, qreal r)
{
    s_ptr->m_pMetadata->setBorderDecoration(e, r);
}

bool AbstractItemAdapter::refresh()
{
    return true;
}

bool AbstractItemAdapter::remove()
{
    return true;
}

bool AbstractItemAdapter::move()
{
    const auto a = s_ptr->m_pViewport->geometryAdapter();
    Q_ASSERT(a);

    const auto pos = a->positionHint(index(), this);
    container()->setSize(a->sizeHint(index(), this));
    container()->setX(pos.x());
    container()->setY(pos.y());

    return true;
}

bool AbstractItemAdapter::attach()
{
    Q_ASSERT(index().isValid());

    return container();// && move();
}

bool AbstractItemAdapter::flush()
{
    return true;
}

void AbstractItemAdapter::setSelected(bool /*s*/)
{}

bool AbstractItemAdapterPrivate::loadDelegate(QQuickItem* parentI) const
{
    const auto delegate = q_ptr->s_ptr->m_pViewport->modelAdapter()->delegate();
    if (!delegate) {
        qWarning() << "No delegate is set";
        return false;
    }

    const auto engine = q_ptr->s_ptr->m_pViewport->s_ptr->engine();

    auto pctx = q_ptr->s_ptr->m_pMetadata->contextAdapter()->context();

    // Create a parent item to hold the delegate and all children
    auto container = qobject_cast<QQuickItem *>(q_ptr->s_ptr->m_pViewport->s_ptr->component()->create(pctx));
    container->setWidth(q_ptr->view()->width());
    engine->setObjectOwnership(container, QQmlEngine::CppOwnership);
    container->setParentItem(parentI);

    m_pContext   = pctx;
    m_pContainer = container;

    // Create a context with all the tree roles
    auto ctx = new QQmlContext(pctx);

    // Create the delegate
    m_pContent = qobject_cast<QQuickItem *>(delegate->create(ctx));

    // It allows the children to be added anyway
    if(!m_pContent) {
        if (!delegate->errorString().isEmpty())
            qWarning() << delegate->errorString();

        return true;
    }

    engine->setObjectOwnership(m_pContent, QQmlEngine::CppOwnership);

    m_pContent->setWidth(q_ptr->view()->width());
    m_pContent->setParentItem(container);

    // Resize the container
    container->setHeight(m_pContent->height());

    // Make sure it can be resized dynamically
    QObject::connect(m_pContent, &QQuickItem::heightChanged, container, [container, this]() {
        container->setHeight(m_pContent->height());
    });

    return true;
}

void StateTracker::ViewItem::updateGeometry()
{
    const auto sm = m_pViewport->modelAdapter()->selectionAdapter();

    if (sm && sm->selectionModel() && sm->selectionModel()->currentIndex() == index())
        sm->s_ptr->updateSelection(index());

    m_pViewport->s_ptr->geometryUpdated(m_pMetadata);
}

QQmlContext *StateTracker::ViewItem::context() const
{
    return d_ptr->d_ptr->m_pContext;
}

void AbstractItemAdapter::setCollapsed(bool v)
{
    s_ptr->setCollapsed(v);
}

bool AbstractItemAdapter::isCollapsed() const
{
    return s_ptr->isCollapsed();
}

QSizeF AbstractItemAdapter::sizeHint() const
{
    return s_ptr->m_pMetadata->sizeHint();
}

QPersistentModelIndex StateTracker::ViewItem::index() const
{
    return QModelIndex(m_pMetadata->indexTracker()->index());
}

/**
 * Flatten the tree as a linked list.
 *
 * Returns the previous non-failed item.
 */
StateTracker::ViewItem* StateTracker::ViewItem::up() const
{
    Q_ASSERT(m_State == State::ACTIVE
        || m_State == State::BUFFER
        || m_State == State::FAILED
        || m_State == State::POOLING
        || m_State == State::DANGLING //FIXME add a new state for
        // "deletion in progress" or swap the f call and set state
    );

    auto ret = m_pMetadata->indexTracker()->up();
    //TODO support collapsed nodes

    // Linearly look for a valid element. Doing this here allows the views
    // that implement this (abstract) class to work without having to always
    // check if some of their item failed to load. This is non-fatal in the
    // other Qt views, so it isn't fatal here either.
    while (ret && !ret->metadata()->viewTracker())
        ret = ret->up();

    if (ret && ret->metadata()->viewTracker())
        Q_ASSERT(ret->metadata()->viewTracker()->m_State != State::POOLING && ret->metadata()->viewTracker()->m_State != State::DANGLING);

    auto vi = ret ? ret->metadata()->viewTracker() : nullptr;

    // Do not allow navigating past the loaded edges
    if (vi && (vi->m_State == State::POOLING || vi->m_State == State::POOLED))
        return nullptr;

    return vi;
}

/**
 * Flatten the tree as a linked list.
 *
 * Returns the next non-failed item.
 */
StateTracker::ViewItem* StateTracker::ViewItem::down() const
{
    Q_ASSERT(m_State == State::ACTIVE
        || m_State == State::BUFFER
        || m_State == State::FAILED
        || m_State == State::POOLING
        || m_State == State::DANGLING //FIXME add a new state for
        // "deletion in progress" or swap the f call and set state
    );

    auto ret = m_pMetadata->indexTracker();
    //TODO support collapsed entries

    // Recursively look for a valid element. Doing this here allows the views
    // that implement this (abstract) class to work without having to always
    // check if some of their item failed to load. This is non-fatal in the
    // other Qt views, so it isn't fatal here either.
    while (ret && !ret->metadata()->viewTracker())
        ret = ret->down();

    auto vi = ret ? ret->metadata()->viewTracker() : nullptr;

    // Do not allow navigating past the loaded edges
    if (vi && (vi->m_State == State::POOLING || vi->m_State == State::POOLED))
        return nullptr;

    return vi;
}

StateTracker::ViewItem *StateTracker::ViewItem::next(Qt::Edge e) const
{
    switch (e) {
        case Qt::TopEdge:
            return up();
        case Qt::BottomEdge:
            return down();
        case Qt::LeftEdge:
            return left();
        case Qt::RightEdge:
            return right();
    }

    Q_ASSERT(false);
    return {};
}

int StateTracker::ViewItem::row() const
{
    return m_pMetadata->indexTracker()->effectiveRow();
}

int StateTracker::ViewItem::column() const
{
    return m_pMetadata->indexTracker()->effectiveColumn();
}

void StateTracker::ViewItem::setCollapsed(bool v)
{
    m_pMetadata->setCollapsed(v);
}

bool StateTracker::ViewItem::isCollapsed() const
{
    return m_pMetadata->isCollapsed();
}

StateTracker::ViewItem::State StateTracker::ViewItem::state() const
{
    return m_State;
}

void AbstractItemAdapterPrivate::slotDestroyed()
{
    m_pContainer = nullptr;
    m_pContent   = nullptr;
}

#include <abstractitemadapter.moc>
