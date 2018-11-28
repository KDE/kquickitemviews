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
#include "private/selectionadapter_p.h"
#include "viewport.h"
#include "private/indexmetadata_p.h"
#include "private/viewport_p.h"
#include "modeladapter.h"
#include "private/statetracker/index_p.h"
#include "viewbase.h"
#include "contextadapterfactory.h"
#include "contextadapter.h"

// Store variables shared by all items of the same view
struct ViewBaseItemVariables
{
    QQmlEngine    *m_pEngine    {nullptr};
    QQmlComponent *m_pComponent {nullptr};

    QQmlEngine    *engine();
    QQmlComponent *component();

    ViewBase* q_ptr;
};

class AbstractItemAdapterPrivate
{
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

    mutable QQuickItem  *m_pItem    {nullptr};
    mutable QQmlContext *m_pContext {nullptr};

    // Helpers
    QPair<QQuickItem*, QQmlContext*> loadDelegate(QQuickItem* parentI) const;
    ViewBaseItemVariables *sharedVariables() const;

    // Attributes
    AbstractItemAdapter* q_ptr;
};

/*
 * The visual elements state changes.
 *
 * Note that the ::FAILED elements will always try to self-heal themselves and
 * go back into FAILED once the self-healing itself failed.
 */
#define S StateTracker::ViewItem::State::
const StateTracker::ViewItem::State AbstractItemAdapterPrivate::m_fStateMap[7][7] = {
/*              ATTACH ENTER_BUFFER ENTER_VIEW UPDATE    MOVE   LEAVE_BUFFER  DETACH  */
/*POOLING */ { S POOLING, S BUFFER, S ERROR , S ERROR , S ERROR , S ERROR  , S POOLED   },
/*POOLED  */ { S POOLED , S BUFFER, S ACTIVE, S ERROR , S ERROR , S ERROR  , S DANGLING },
/*BUFFER  */ { S ERROR  , S ERROR , S ACTIVE, S BUFFER, S ERROR , S POOLING, S DANGLING },
/*ACTIVE  */ { S ERROR  , S BUFFER, S ERROR , S ACTIVE, S ACTIVE, S BUFFER , S POOLING  },
/*FAILED  */ { S ERROR  , S BUFFER, S ACTIVE, S ACTIVE, S ACTIVE, S POOLING, S DANGLING },
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
    d_ptr(new AbstractItemAdapterPrivate),
    s_ptr(new StateTracker::ViewItem(r->modelAdapter()->view(), r))
{
    d_ptr->q_ptr = this;
    s_ptr->d_ptr = this;
}

AbstractItemAdapter::~AbstractItemAdapter()
{
    if (d_ptr->m_pItem)
        delete d_ptr->m_pItem;

    if (d_ptr->m_pContext)
        delete d_ptr->m_pContext;

    delete d_ptr;
}

ViewBase* AbstractItemAdapter::view() const
{
    return s_ptr->m_pView;
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

QRectF StateTracker::ViewItem::geometry() const
{
    return d_ptr->geometry();
}

QQuickItem* StateTracker::ViewItem::item() const
{
    return d_ptr->item();
}

bool AbstractItemAdapterPrivate::attach()
{
    if (m_pItem)
        m_pItem->setVisible(true);

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
//     Q_ASSERT((!ret) || q_ptr->s_ptr->m_pGeometry->isInSync());

    return ret;
}

bool AbstractItemAdapterPrivate::flush()
{
    return q_ptr->flush();
}

bool AbstractItemAdapterPrivate::remove()
{
    bool ret = q_ptr->remove();

    m_pItem->setParentItem(nullptr);
    q_ptr->s_ptr->m_pTTI = nullptr;

    return ret;
}

bool AbstractItemAdapterPrivate::hide()
{
    Q_ASSERT(m_pItem);
    if (!m_pItem)
        return false;

    m_pItem->setVisible(false);

    return true;
}

// This methodwrap the removal of the element from the view
bool AbstractItemAdapterPrivate::detach()
{
    m_pItem->setParentItem(nullptr);
    remove();

    //FIXME
    q_ptr->s_ptr->m_State = StateTracker::ViewItem::State::POOLED;

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
    m_pItem->setParentItem(nullptr);
    delete m_pItem;
    m_pItem = nullptr;

    QTimer::singleShot(0,[this, ptrCopy]() {
        if (!ptrCopy)
            delete this;
        // else the reference will be dropped and the destructor called
    });

    if (m_pLocker)
        m_pLocker.clear();

    return true;
}

bool StateTracker::ViewItem::performAction(IndexMetadata::ViewAction a)
{
    //if (m_State == StateTracker::ViewItem::State::FAILED)
    //    m_pTTI->d_ptr->m_FailedCount--;

    const int s = (int)m_State;

    m_State     = d_ptr->d_ptr->m_fStateMap [s][(int)a];
    Q_ASSERT(m_State != StateTracker::ViewItem::State::ERROR);

    const bool ret = (d_ptr->d_ptr ->* d_ptr->d_ptr->m_fStateMachine[s][(int)a])();

    /*
     * It can happen normally. For example, if the QML initialization sets the
     * model before the delegate.
     */
    if (m_State == StateTracker::ViewItem::State::FAILED || !ret) {
        //Q_ASSERT(false);
        m_State = StateTracker::ViewItem::State::FAILED;
        //m_pTTI->d_ptr->m_FailedCount++;
    }

    return ret;
}

int StateTracker::ViewItem::depth() const
{
    return m_pGeometry->indexTracker()->depth();
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
    if (m_pContext || m_pItem)
        return;

    if (!q_ptr->s_ptr->m_pRange->modelAdapter()->delegate()) {
        qDebug() << "Cannot attach, there is no delegate";
        return;
    }

    auto pair = loadDelegate(q_ptr->view()->contentItem());

    if (!pair.first) {
        qDebug() << "Item failed to load" << q_ptr->index().data();
        return;
    }

    if (!pair.first->z())
        pair.first->setZ(1);

    /*d()->m_DepthChart[depth()] = std::max(
        d()->m_DepthChart[depth()],
        pair.first->height()
    );*/

    m_pContext = pair.second;
    m_pItem    = pair.first;

    Q_ASSERT(q_ptr->s_ptr->m_pGeometry);

    q_ptr->s_ptr->m_pGeometry->contextAdapter()->context();

    Q_ASSERT(q_ptr->s_ptr->m_pGeometry->contextAdapter()->context() == m_pContext);

    // Update the geometry cache
    switch (q_ptr->s_ptr->m_pRange->sizeHintStrategy()){
        case Viewport::SizeHintStrategy::JIT:
        case Viewport::SizeHintStrategy::AOT:
            q_ptr->s_ptr->m_pGeometry->setSize(
                QSizeF(m_pItem->width(), m_pItem->height())
            );
            break;
        case Viewport::SizeHintStrategy::PROXY:
        case Viewport::SizeHintStrategy::ROLE:
        case Viewport::SizeHintStrategy::DELEGATE:
            q_ptr->s_ptr->m_pGeometry->performAction(
                IndexMetadata::GeometryAction::MODIFY
            );
            break;
        case Viewport::SizeHintStrategy::UNIFORM:
            break;
    }

    Q_ASSERT(q_ptr->s_ptr->m_pGeometry->removeMe() != (int)StateTracker::Geometry::State::INIT);
}

QQmlContext *AbstractItemAdapter::context() const
{
    d_ptr->load();
    return d_ptr->m_pContext;
}

QQuickItem *AbstractItemAdapter::item() const
{
    d_ptr->load();
    return d_ptr->m_pItem;
}

Viewport *AbstractItemAdapter::viewport() const
{
    return s_ptr->m_pRange;
}

QRectF AbstractItemAdapter::geometry() const
{
    if (!d_ptr->m_pItem)
        return {};

    const QPointF p = item()->mapFromItem(view()->contentItem(), {0,0});
    return {
        -p.x(),
        -p.y(),
        item()->width(),
        item()->height()
    };
}

QRectF AbstractItemAdapter::decoratedGeometry() const
{
    return s_ptr->m_pGeometry->decoratedGeometry();
}

qreal AbstractItemAdapter::borderDecoration(Qt::Edge e) const
{
    return s_ptr->m_pGeometry->borderDecoration(e);
}

void AbstractItemAdapter::setBorderDecoration(Qt::Edge e, qreal r)
{
    s_ptr->m_pGeometry->setBorderDecoration(e, r);
}

bool AbstractItemAdapter::refresh()
{
    return true;
}

bool AbstractItemAdapter::attach()
{
    Q_ASSERT(index().isValid());

    return item();// && move();
}

bool AbstractItemAdapter::flush()
{
    return true;
}

void AbstractItemAdapter::setSelected(bool /*s*/)
{}

QPair<QQuickItem*, QQmlContext*> AbstractItemAdapterPrivate::loadDelegate(QQuickItem* parentI) const
{
    if (!q_ptr->s_ptr->m_pRange->modelAdapter()->delegate())
        return {};

    auto pctx = q_ptr->s_ptr->m_pGeometry->contextAdapter()->context();

    // Create a parent item to hold the delegate and all children
    auto container = qobject_cast<QQuickItem *>(sharedVariables()->component()->create(pctx));
    container->setWidth(q_ptr->view()->width());
    sharedVariables()->engine()->setObjectOwnership(container, QQmlEngine::CppOwnership);
    container->setParentItem(parentI);

    // Create a context with all the tree roles
    auto ctx = new QQmlContext(pctx);

    // Create the delegate
    auto item = qobject_cast<QQuickItem *>(
        q_ptr->s_ptr->m_pRange->modelAdapter()->delegate()->create(ctx)
    );

    // It allows the children to be added anyway
    if(!item)
        return {container, pctx};

    item->setWidth(q_ptr->view()->width());
    item->setParentItem(container);

    // Resize the container
    container->setHeight(item->height());

    // Make sure it can be resized dynamically
    QObject::connect(item, &QQuickItem::heightChanged, container, [container, item](){
        container->setHeight(item->height());
    });

    container->setProperty("content", QVariant::fromValue(item));

    return {container, pctx};
}

QQmlEngine *ViewBaseItemVariables::engine()
{
    if (!m_pEngine)
        m_pEngine = q_ptr->rootContext()->engine();

    return m_pEngine;
}

QQmlComponent *ViewBaseItemVariables::component()
{
    engine();

    m_pComponent = new QQmlComponent(m_pEngine);
    m_pComponent->setData("import QtQuick 2.4; Item {property QtObject content: null;}", {});

    return m_pComponent;
}

ViewBaseItemVariables *AbstractItemAdapterPrivate::sharedVariables() const
{
    if (!q_ptr->s_ptr->m_pView->m_pItemVars) {
        q_ptr->s_ptr->m_pView->m_pItemVars = new ViewBaseItemVariables();
        q_ptr->s_ptr->m_pView->m_pItemVars->q_ptr = q_ptr->s_ptr->m_pView;
    }

    return q_ptr->s_ptr->m_pView->m_pItemVars;
}

ViewBase* StateTracker::ViewItem::view() const
{
    return m_pView;
}

void StateTracker::ViewItem::updateGeometry()
{
    geometry();

    //TODO handle up/left/right too

    const auto sm = m_pRange->modelAdapter()->selectionAdapter();

    if (sm && sm->selectionModel() && sm->selectionModel()->currentIndex() == index())
        sm->s_ptr->updateSelection(index());

    m_pRange->s_ptr->geometryUpdated(m_pGeometry);
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
    return s_ptr->m_pGeometry->sizeHint();
}

QPersistentModelIndex StateTracker::ViewItem::index() const
{
    return QModelIndex(m_pGeometry->indexTracker()->index());
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

    auto ret = m_pGeometry->indexTracker()->up();
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

    auto ret = m_pGeometry->indexTracker();
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
    return m_pGeometry->indexTracker()->effectiveRow();
}

int StateTracker::ViewItem::column() const
{
    return m_pGeometry->indexTracker()->effectiveColumn();
}
