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
#include "abstractitemadapter_p.h"
#include "adapters/selectionadapter.h"
#include "adapters/selectionadapter_p.h"
#include "viewport.h"
#include "viewport_p.h"
#include "modeladapter.h"
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

    static const VisualTreeItem::State  m_fStateMap    [7][7];
    static const StateF                 m_fStateMachine[7][7];

    mutable QSharedPointer<AbstractItemAdapter::SelectionLocker> m_pLocker;

    mutable QQuickItem  *m_pItem    {nullptr};
    mutable QQmlContext *m_pContext {nullptr};

    // Helpers
    QPair<QQuickItem*, QQmlContext*> loadDelegate(QQuickItem* parentI, QQmlContext* parentCtx) const;
    ViewBaseItemVariables *sharedVariables() const;

    // Attributes
    AbstractItemAdapter* q_ptr;
};


/**
 * Extend the class for the need of needs of the AbstractItemAdapter.
 */
class ViewItemContextAdapter final : public ContextAdapter
{
public:
    explicit ViewItemContextAdapter(QQmlContext* p) : ContextAdapter(p){}
    virtual ~ViewItemContextAdapter() {
        m_pItem->flushCache();
        m_pItem->context()->setContextObject(nullptr);
    }

    virtual QQmlContext         *context() const override;
    virtual QModelIndex          index  () const override;
    virtual AbstractItemAdapter *item   () const override;

    mutable std::atomic_flag m_InitContext = ATOMIC_FLAG_INIT;

    AbstractItemAdapter* m_pItem;
};

/*
 * The visual elements state changes.
 *
 * Note that the ::FAILED elements will always try to self-heal themselves and
 * go back into FAILED once the self-healing itself failed.
 */
#define S VisualTreeItem::State::
const VisualTreeItem::State AbstractItemAdapterPrivate::m_fStateMap[7][7] = {
/*              ATTACH ENTER_BUFFER ENTER_VIEW UPDATE    MOVE   LEAVE_BUFFER  DETACH  */
/*POOLING */ { S POOLING, S BUFFER, S ERROR , S ERROR , S ERROR , S ERROR  , S DANGLING },
/*POOLED  */ { S POOLED , S BUFFER, S ERROR , S ERROR , S ERROR , S ERROR  , S DANGLING },
/*BUFFER  */ { S ERROR  , S ERROR , S ACTIVE, S BUFFER, S ERROR , S POOLING, S DANGLING },
/*ACTIVE  */ { S ERROR  , S BUFFER, S ERROR , S ACTIVE, S ACTIVE, S POOLING, S POOLING  },
/*FAILED  */ { S ERROR  , S BUFFER, S ACTIVE, S ACTIVE, S ACTIVE, S POOLING, S DANGLING },
/*DANGLING*/ { S ERROR  , S ERROR , S ERROR , S ERROR , S ERROR , S ERROR  , S DANGLING },
/*ERROR   */ { S ERROR  , S ERROR , S ERROR , S ERROR , S ERROR , S ERROR  , S DANGLING },
};
#undef S

#define A &AbstractItemAdapterPrivate::
const AbstractItemAdapterPrivate::StateF AbstractItemAdapterPrivate::m_fStateMachine[7][7] = {
/*             ATTACH  ENTER_BUFFER  ENTER_VIEW   UPDATE     MOVE   LEAVE_BUFFER  DETACH  */
/*POOLING */ { A error  , A error  , A error  , A error  , A error  , A error  , A error   },
/*POOLED  */ { A nothing, A attach , A error  , A error  , A error  , A error  , A destroy },
/*BUFFER  */ { A error  , A error  , A move   , A refresh, A error  , A detach , A destroy },
/*ACTIVE  */ { A error  , A nothing, A error  , A refresh, A move   , A detach , A detach  },
/*FAILED  */ { A error  , A attach , A attach , A attach , A attach , A detach , A destroy },
/*DANGLING*/ { A error  , A error  , A error  , A error  , A error  , A error  , A destroy },
/*error   */ { A error  , A error  , A error  , A error  , A error  , A error  , A destroy },
};
#undef A

AbstractItemAdapter::AbstractItemAdapter(Viewport* r) :
    d_ptr(new AbstractItemAdapterPrivate),
    s_ptr(new VisualTreeItem(r->modelAdapter()->view(), r))
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

AbstractItemAdapter* AbstractItemAdapter::up() const
{
    const auto i = s_ptr->up();
    return i ? i->d_ptr : nullptr;
}

AbstractItemAdapter* AbstractItemAdapter::down() const
{
    const auto i = s_ptr->down();
    return i ? i->d_ptr : nullptr;
}

AbstractItemAdapter* AbstractItemAdapter::left() const
{
    const auto i = s_ptr->left();
    return i ? i->d_ptr : nullptr;
}

AbstractItemAdapter* AbstractItemAdapter::right() const
{
    const auto i = s_ptr->right();
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

void VisualTreeItem::setSelected(bool v)
{
    d_ptr->setSelected(v);
}

QRectF VisualTreeItem::geometry() const
{
    return d_ptr->geometry();
}

QQuickItem* VisualTreeItem::item() const
{
    return d_ptr->item();
}

bool AbstractItemAdapterPrivate::attach()
{
    return q_ptr->attach();
}

bool AbstractItemAdapterPrivate::refresh()
{
    return q_ptr->refresh();
}

bool AbstractItemAdapterPrivate::move()
{
    return q_ptr->move();
}

bool AbstractItemAdapterPrivate::flush()
{
    return q_ptr->flush();
}

bool AbstractItemAdapterPrivate::remove()
{
    return q_ptr->remove();
}

// This methodwrap the removal of the element from the view
bool AbstractItemAdapterPrivate::detach()
{
    remove();

    //FIXME
    q_ptr->s_ptr->m_State = VisualTreeItem::State::POOLED;

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

    QTimer::singleShot(0,[this, ptrCopy]() {
        if (!ptrCopy)
            delete this;
        // else the reference will be dropped and the destructor called
    });

    if (m_pLocker)
        m_pLocker.clear();

    return true;
}

bool VisualTreeItem::performAction(VisualTreeItem::ViewAction a)
{
    //if (m_State == VisualTreeItem::State::FAILED)
    //    m_pTTI->d_ptr->m_FailedCount--;

    const int s = (int)m_State;
    m_State     = d_ptr->d_ptr->m_fStateMap [s][(int)a];
    Q_ASSERT(m_State != VisualTreeItem::State::ERROR);

    const bool ret = (d_ptr->d_ptr ->* d_ptr->d_ptr->m_fStateMachine[s][(int)a])();

    /*
     * It can happen normally. For example, if the QML initialization sets the
     * model before the delegate.
     */
    if (m_State == VisualTreeItem::State::FAILED || !ret) {
        //Q_ASSERT(false);
        m_State = VisualTreeItem::State::FAILED;
        //m_pTTI->d_ptr->m_FailedCount++;
    }

    return ret;
}

int VisualTreeItem::depth() const
{
    return 0;//FIXME m_pTTI->m_Depth;
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

    auto pair = loadDelegate(
        q_ptr->view()->contentItem(),
        q_ptr->view()->rootContext()
    );

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

    q_ptr->s_ptr->updateContext();
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

bool AbstractItemAdapter::refresh()
{
    if (context())
        s_ptr->updateContext();

    return true;
}

bool AbstractItemAdapter::attach()
{
    Q_ASSERT(index().isValid());
    return item() && move();
}

bool AbstractItemAdapter::flush()
{
    return true;
}

void AbstractItemAdapter::setSelected(bool /*s*/)
{}

QPair<QQuickItem*, QQmlContext*> AbstractItemAdapterPrivate::loadDelegate(QQuickItem* parentI, QQmlContext* parentCtx) const
{
    if (!q_ptr->s_ptr->m_pRange->modelAdapter()->delegate())
        return {};

    // Create a context for the container, it's the only way to force anchors
    // to work
    auto pctx = new QQmlContext(parentCtx);

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

ContextAdapter* VisualTreeItem::contextAdapter() const
{
    if (!m_pContextAdapter) {
        auto cm = m_pRange->modelAdapter()->contextAdapterFactory();
        m_pContextAdapter = cm->createAdapter<ViewItemContextAdapter>();
        m_pContextAdapter->m_pItem = d_ptr;
    }

    return m_pContextAdapter;
}

QModelIndex ViewItemContextAdapter::index() const
{
    return m_pItem->index();
}

QQmlContext* ViewItemContextAdapter::context() const
{
    if (!m_InitContext.test_and_set()) {
        m_pItem->d_ptr->m_pContext->setContextObject(contextObject());
    }

    return m_pItem->d_ptr->m_pContext;
}

AbstractItemAdapter* ViewItemContextAdapter::item() const
{
    return m_pItem;
}

void VisualTreeItem::updateContext()
{
    if (m_pContextAdapter)
        return;

    ContextAdapterFactory* cm = m_pRange->modelAdapter()->contextAdapterFactory();

    const QModelIndex self = d_ptr->index();
    Q_ASSERT(self.model());
    cm->setModel(const_cast<QAbstractItemModel*>(self.model()));

    // This will init the context now. If the method has been re-implemented in
    // a busclass, it might do important thing and otherwise never be called.
    contextAdapter()->context();

    Q_ASSERT(m_pContextAdapter);
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

ViewBase* VisualTreeItem::view() const
{
    return m_pView;
}

void VisualTreeItem::updateGeometry()
{
    const auto geo = geometry();

    //TODO handle up/left/right too

    if (!down()) {
        view()->contentItem()->setHeight(std::max(
            geo.y()+geo.height(), view()->height()
        ));

        emit view()->contentHeightChanged(view()->contentItem()->height());
    }

    const auto sm = m_pRange->modelAdapter()->selectionAdapter();

    if (sm && sm->selectionModel() && sm->selectionModel()->currentIndex() == index())
        sm->s_ptr->updateSelection(index());

    m_pRange->s_ptr->geometryUpdated(this);
}

void AbstractItemAdapter::setCollapsed(bool v)
{
    s_ptr->m_IsCollapsed = v;
}

bool AbstractItemAdapter::isCollapsed() const
{
    return s_ptr->m_IsCollapsed;
}
