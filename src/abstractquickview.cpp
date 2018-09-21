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
#include "abstractquickview.h"

#include <QtCore/QTimer>
#include <QQmlComponent>
#include <QQmlContext>

#include <functional>

#include "abstractviewitem_p.h"
#include "abstractquickview_p.h"
#include "abstractviewitem.h"
#include "treetraversalreflector_p.h"
#include "visiblerange.h"
#include "abstractselectableview.h"
#include "abstractselectableview_p.h"
#include "contextmanager.h"
#include "modeladapter.h"

/*
 * Design:
 *
 * This class implement 3 embedded state machines.
 *
 * The first machine has a single instance and manage the whole view. It allows
 * different behavior (corner case) to be handled in a stateful way without "if".
 * It tracks a moving window of elements coarsely equivalent to the number of
 * elements on screen.
 *
 * The second layer reflects the model and corresponds to a QModelIndex currently
 * being tracked by the system. They are lazy-loaded and mostly unaware of the
 * model topology. While they are hierarchical, they are so compared to themselves,
 * not the actual model hierarchy. This allows the model to have unlimited depth
 * without any performance impact.
 *
 * The last layer of state machine represents the visual elements. Being separated
 * from the model view allows the QQuickItem elements to fail to load without
 * spreading havoc. This layer also implements an abstraction that allows the
 * tree to be browsed as a list. This greatly simplifies the widget code. The
 * abstract class has to be implemented by the view implementations. All the
 * heavy lifting is done in this class and the implementation can assume all is
 * valid and perform minimal validation.
 *
 */

class AbstractQuickViewPrivate final : public QObject
{
    Q_OBJECT
public:

    enum class State {
        UNFILLED, /*!< There is less items that the space available          */
        ANCHORED, /*!< Some items are out of view, but it's at the beginning */
        SCROLLED, /*!< It's scrolled to a random point                       */
        AT_END  , /*!< It's at the end of the items                          */
        ERROR   , /*!< Something went wrong                                  */
    };

    enum class Action {
        INSERTION    = 0,
        REMOVAL      = 1,
        MOVE         = 2,
        RESET_SCROLL = 3,
        SCROLL       = 4,
    };

    typedef bool(AbstractQuickViewPrivate::*StateF)();

    static const State  m_fStateMap    [5][5];
    static const StateF m_fStateMachine[5][5];

    QQmlEngine*    m_pEngine    {nullptr};
    QQmlComponent* m_pComponent {nullptr};

    QVector<ModelAdapter*> m_lAdapters;

    State m_State {State::UNFILLED};

    AbstractQuickView* q_ptr;

private:
    bool nothing     ();
    bool resetScoll  ();
    bool refresh     ();
    bool refreshFront();
    bool refreshBack ();
    bool error       () __attribute__ ((noreturn));

public Q_SLOTS:
    void slotContentChanged();
    void slotCountChanged();
};

/// Add the same property as the QtQuick.ListView
class ModelIndexGroup final : public ContextManager::PropertyGroup
{
public:
    virtual ~ModelIndexGroup() {}
    virtual QVector<QByteArray>& propertyNames() const override;
    virtual QVariant getProperty(AbstractViewItem* item, uint id, const QModelIndex& index) const override;
};

#define S AbstractQuickViewPrivate::State::
const AbstractQuickViewPrivate::State AbstractQuickViewPrivate::m_fStateMap[5][5] = {
/*              INSERTION    REMOVAL       MOVE     RESET_SCROLL    SCROLL  */
/*UNFILLED */ { S ANCHORED, S UNFILLED , S UNFILLED , S UNFILLED, S UNFILLED },
/*ANCHORED */ { S ANCHORED, S ANCHORED , S ANCHORED , S ANCHORED, S SCROLLED },
/*SCROLLED */ { S SCROLLED, S SCROLLED , S SCROLLED , S ANCHORED, S SCROLLED },
/*AT_END   */ { S AT_END  , S AT_END   , S AT_END   , S ANCHORED, S SCROLLED },
/*ERROR    */ { S ERROR   , S ERROR    , S ERROR    , S ERROR   , S ERROR    },
};
#undef S

#define A &AbstractQuickViewPrivate::
const AbstractQuickViewPrivate::StateF AbstractQuickViewPrivate::m_fStateMachine[5][5] = {
/*              INSERTION           REMOVAL          MOVE        RESET_SCROLL     SCROLL  */
/*UNFILLED*/ { A refreshFront, A refreshFront , A refreshFront , A nothing   , A nothing  },
/*ANCHORED*/ { A refreshFront, A refreshFront , A refreshFront , A nothing   , A refresh  },
/*SCROLLED*/ { A refresh     , A refresh      , A refresh      , A resetScoll, A refresh  },
/*AT_END  */ { A refreshBack , A refreshBack  , A refreshBack  , A resetScoll, A refresh  },
/*ERROR   */ { A error       , A error        , A error        , A error     , A error    },
};
#undef A

AbstractQuickView::AbstractQuickView(QQuickItem* parent) : SimpleFlickable(parent),
    s_ptr(new AbstractQuickViewSync()), d_ptr(new AbstractQuickViewPrivate())
{
    d_ptr->q_ptr = this;
    s_ptr->d_ptr = d_ptr;
}

AbstractQuickView::~AbstractQuickView()
{;
    delete s_ptr;
    delete d_ptr;
}

void AbstractQuickView::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    // Resize the visible ranges
    for (auto ma : qAsConst(d_ptr->m_lAdapters)) {
        const auto ranges = ma->visibleRanges();
        for (auto vr : qAsConst(ranges))
            vr->setSize(newGeometry.size());
    }

    contentItem()->setWidth(newGeometry.width());
}

AbstractViewItem* AbstractQuickView::itemForIndex(const QModelIndex& idx) const
{
    Q_ASSERT(d_ptr->m_lAdapters.size()); //FIXME worry about this later

    return d_ptr->m_lAdapters.constFirst()->itemForIndex(idx);
}

void AbstractQuickView::reload()
{
    Q_ASSERT(false);
}

void AbstractQuickViewPrivate::slotContentChanged()
{
    emit q_ptr->contentChanged();
}

void AbstractQuickViewPrivate::slotCountChanged()
{
    emit q_ptr->countChanged();
}

bool AbstractQuickViewPrivate::nothing()
{ return true; }

bool AbstractQuickViewPrivate::resetScoll()
{
    return true;
}

bool AbstractQuickViewPrivate::refresh()
{
    // Move the visible ranges
    for (auto ma : qAsConst(m_lAdapters)) {
        const auto ranges = ma->visibleRanges();
        for (auto vr : qAsConst(ranges))
            vr->setPosition(QPointF(0.0, q_ptr->currentY()));
    }

    return true;
}

bool AbstractQuickViewPrivate::refreshFront()
{
    return true;
}

bool AbstractQuickViewPrivate::refreshBack()
{
    return true;
}

bool AbstractQuickViewPrivate::error()
{
    Q_ASSERT(false);
    return true;
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

    const auto sm = m_pRange->modelAdapter()->selectionManager();

    if (sm && sm->selectionModel() && sm->selectionModel()->currentIndex() == index())
        sm->s_ptr->updateSelection(index());
}

AbstractQuickView* VisualTreeItem::view() const
{
    return m_pView;
}


void AbstractQuickView::refresh()
{
    if (!d_ptr->m_pEngine) {
        d_ptr->m_pEngine = rootContext()->engine();
        d_ptr->m_pComponent = new QQmlComponent(d_ptr->m_pEngine);
        d_ptr->m_pComponent->setData("import QtQuick 2.4; Item {property QtObject content: null;}", {});
    }
}

QQmlEngine *AbstractQuickViewSync::engine() const
{
    if (!d_ptr->m_pEngine)
        d_ptr->q_ptr->refresh();

    return d_ptr->m_pEngine;
}

QQmlComponent *AbstractQuickViewSync::component() const
{
    if (!d_ptr->m_pComponent)
        d_ptr->q_ptr->refresh();

    return d_ptr->m_pComponent;
}

QVector<QByteArray>& ModelIndexGroup::propertyNames() const
{
    static QVector<QByteArray> ret {
        "index"     ,
        "rootIndex" ,
        "rowCount"  ,
    };

    return ret;
}

QVariant ModelIndexGroup::getProperty(AbstractViewItem* item, uint id, const QModelIndex& index) const
{
    switch(id) {
        case 0 /*index*/:
            return index.row();
        case 1 /*rootIndex*/:
            return item->index(); // That's a QPersistentModelIndex and is a better fit
        case 2 /*rowCount*/:
            return index.model()->rowCount(index);
        //FIXME add parent index
        //FIXME add parent item?
        //FIXME depth
    }

    Q_ASSERT(false);
    return {};
}

void AbstractQuickView::addModelAdapter(ModelAdapter* a)
{
    connect(a, &ModelAdapter::contentChanged,
        d_ptr, &AbstractQuickViewPrivate::slotContentChanged);
    connect(a, &ModelAdapter::countChanged,
        d_ptr, &AbstractQuickViewPrivate::slotCountChanged);

    a->contextManager()->addPropertyGroup(new ModelIndexGroup());

    d_ptr->m_lAdapters << a;
}

void AbstractQuickView::removeModelAdapter(ModelAdapter* a)
{
    Q_ASSERT(false); //TODO
    d_ptr->m_lAdapters.removeAll(a);
}

QVector<ModelAdapter*> AbstractQuickView::modelAdapters() const
{
    return d_ptr->m_lAdapters;
}

#include <abstractquickview.moc>
