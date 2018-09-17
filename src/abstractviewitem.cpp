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
#include "abstractviewitem.h"

// libstdc++
#include <atomic>

// Qt
#include <QtCore/QTimer>
#include <QQmlContext>
#include <QQuickItem>
#include <QQmlEngine>

#include "abstractviewitem_p.h"
#include "abstractquickview_p.h"
#include "abstractquickview.h"
#include "contextmanager.h"

class AbstractViewItemPrivate
{
public:
    typedef bool(AbstractViewItemPrivate::*StateF)();

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
    static const StateF m_fStateMachine[7][7];

    mutable QSharedPointer<AbstractViewItem::SelectionLocker> m_pLocker;

    mutable QQuickItem  *m_pItem    {nullptr};
    mutable QQmlContext *m_pContext {nullptr};

    // Helpers
    QPair<QQuickItem*, QQmlContext*> loadDelegate(QQuickItem* parentI, QQmlContext* parentCtx) const;

    // Attributes
    AbstractViewItem* q_ptr;
};


/**
 * Extend the class for the need of needs of the AbstractViewItem.
 */
class ViewItemContextBuilder final : public ContextBuilder
{
public:
    explicit ViewItemContextBuilder(ContextManager* m) : ContextBuilder(m){}

    virtual ~ViewItemContextBuilder() {
        m_pItem->flushCache();
        m_pItem->context()->setContextObject(nullptr);
    }

    virtual QQmlContext      *context() const override;
    virtual QModelIndex      index   () const override;
    virtual AbstractViewItem *item   () const override;

    mutable std::atomic_flag m_InitContext = ATOMIC_FLAG_INIT;

    AbstractViewItem* m_pItem;
};

/*
 * The visual elements state changes.
 *
 * Note that the ::FAILED elements will always try to self-heal themselves and
 * go back into FAILED once the self-healing itself failed.
 */
#define S VisualTreeItem::State::
const VisualTreeItem::State AbstractViewItemPrivate::m_fStateMap[7][7] = {
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

#define A &AbstractViewItemPrivate::
const AbstractViewItemPrivate::StateF AbstractViewItemPrivate::m_fStateMachine[7][7] = {
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

AbstractViewItem::AbstractViewItem(AbstractQuickView* v) :
    d_ptr(new AbstractViewItemPrivate),
    s_ptr(new VisualTreeItem(v))
{
    d_ptr->q_ptr = this;
    s_ptr->d_ptr = this;
}

AbstractViewItem::~AbstractViewItem()
{
    if (d_ptr->m_pItem)
        delete d_ptr->m_pItem;

    if (d_ptr->m_pContext)
        delete d_ptr->m_pContext;

    delete d_ptr;
}

AbstractQuickView* AbstractViewItem::view() const
{
    return s_ptr->m_pView;
}


void AbstractViewItem::resetPosition()
{
    //
}


int AbstractViewItem::row() const
{
    return s_ptr->row();
}

int AbstractViewItem::column() const
{
    return s_ptr->column();
}

QPersistentModelIndex AbstractViewItem::index() const
{
    return s_ptr->index();
}

AbstractViewItem* AbstractViewItem::up() const
{
    const auto i = s_ptr->up();
    return i ? i->d_ptr : nullptr;
}

AbstractViewItem* AbstractViewItem::down() const
{
    const auto i = s_ptr->down();
    return i ? i->d_ptr : nullptr;
}

AbstractViewItem* AbstractViewItem::left() const
{
    const auto i = s_ptr->left();
    return i ? i->d_ptr : nullptr;
}

AbstractViewItem* AbstractViewItem::right() const
{
    const auto i = s_ptr->right();
    return i ? i->d_ptr : nullptr;
}

AbstractViewItem* AbstractViewItem::parent() const
{
    return nullptr;
}

void AbstractViewItem::updateGeometry()
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

bool AbstractViewItemPrivate::attach()
{
    return q_ptr->attach();
}

bool AbstractViewItemPrivate::refresh()
{
    return q_ptr->refresh();
}

bool AbstractViewItemPrivate::move()
{
    return q_ptr->move();
}

bool AbstractViewItemPrivate::flush()
{
    return q_ptr->flush();
}

bool AbstractViewItemPrivate::remove()
{
    return q_ptr->remove();
}

// This methodwrap the removal of the element from the view
bool AbstractViewItemPrivate::detach()
{
    remove();

    //FIXME
    q_ptr->s_ptr->m_State = VisualTreeItem::State::POOLED;

    return true;
}

bool AbstractViewItemPrivate::nothing()
{
    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
bool AbstractViewItemPrivate::error()
{
    Q_ASSERT(false);
    return true;
}
#pragma GCC diagnostic pop

bool AbstractViewItemPrivate::destroy()
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

bool VisualTreeItem::fitsInView() const
{
    const auto geo  = geometry();
    const auto v = view()->visibleRect();

    //TODO support horizontal visibility
    return geo.y() >= v.y()
        && (geo.y() <= v.y() + v.height());
}

QPair<QWeakPointer<AbstractViewItem::SelectionLocker>, AbstractViewItem*>
AbstractViewItem::weakReference() const
{
    if (d_ptr->m_pLocker)
        return {d_ptr->m_pLocker, const_cast<AbstractViewItem*>(this)};

    d_ptr->m_pLocker = QSharedPointer<SelectionLocker>(new SelectionLocker());


    return {d_ptr->m_pLocker, const_cast<AbstractViewItem*>(this)};
}

QSizeF AbstractViewItem::sizeHint() const
{
    return view()->sizeHint(index());
}

void AbstractViewItemPrivate::load()
{
    if (m_pContext || m_pItem)
        return;

    if (!q_ptr->view()->delegate()) {
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

QQmlContext *AbstractViewItem::context() const
{
    d_ptr->load();
    return d_ptr->m_pContext;
}

QQuickItem *AbstractViewItem::item() const
{
    d_ptr->load();
    return d_ptr->m_pItem;
}


QRectF AbstractViewItem::geometry() const
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

bool AbstractViewItem::refresh()
{
    if (context())
        s_ptr->updateContext();

    return true;
}


QPair<QQuickItem*, QQmlContext*> AbstractViewItemPrivate::loadDelegate(QQuickItem* parentI, QQmlContext* parentCtx) const
{
    if (!q_ptr->view()->delegate())
        return {};

    // Create a context for the container, it's the only way to force anchors
    // to work
    auto pctx = new QQmlContext(parentCtx);

    // Create a parent item to hold the delegate and all children
    auto container = qobject_cast<QQuickItem *>(q_ptr->view()->s_ptr->component()->create(pctx));
    container->setWidth(q_ptr->view()->width());
    q_ptr->view()->s_ptr->engine()->setObjectOwnership(container, QQmlEngine::CppOwnership);
    container->setParentItem(parentI);

    // Create a context with all the tree roles
    auto ctx = new QQmlContext(pctx);

    // Create the delegate
    auto item = qobject_cast<QQuickItem *>(q_ptr->view()->delegate()->create(ctx));

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

ContextBuilder* VisualTreeItem::contextBuilder() const
{
    if (!m_pContextBuilder) {
        m_pContextBuilder = new ViewItemContextBuilder(view()->contextManager());
        m_pContextBuilder->m_pItem = d_ptr;
    }

    return m_pContextBuilder;
}

QModelIndex ViewItemContextBuilder::index() const
{
    return m_pItem->index();
}

QQmlContext* ViewItemContextBuilder::context() const
{
    if (!m_InitContext.test_and_set()) {
        m_pItem->d_ptr->m_pContext->setContextObject(contextObject());
    }

    return m_pItem->d_ptr->m_pContext;
}

AbstractViewItem* ViewItemContextBuilder::item() const
{
    return m_pItem;
}

void VisualTreeItem::updateContext()
{
    if (m_pContextBuilder)
        return;

    ContextManager* cm = d_ptr->view()->contextManager();
    const QModelIndex self = d_ptr->index();
    Q_ASSERT(self.model());
    cm->setModel(const_cast<QAbstractItemModel*>(self.model()));

    // This will init the context now. If the method has been re-implemented in
    // a busclass, it might do important thing and otherwise never be called.
    contextBuilder()->context();

    Q_ASSERT(m_pContextBuilder);
}
