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
#include "viewbase.h"

// Qt
#include <QtCore/QTimer>
#include <QQmlContext>

// LibStdC++
#include <functional>

// KQuickItemViews
#include "adapters/abstractitemadapter_p.h"
#include "adapters/abstractitemadapter.h"
#include "adapters/selectionadapter.h"
#include "treetraversalreflector_p.h"
#include "viewport.h"
#include "contextadapterfactory.h"
#include "adapters/contextadapter.h"
#include "adapters/modeladapter.h"
#include "adapters/selectionadapter_p.h"
#include "extensions/contextextension.h"

class ViewBasePrivate final : public QObject
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

    typedef bool(ViewBasePrivate::*StateF)();

    static const State  m_fStateMap    [5][5];
    static const StateF m_fStateMachine[5][5];

    QQmlEngine *m_pEngine {      nullptr    };
    Qt::Corner  m_Corner  {Qt::TopLeftCorner};

    QVector<ModelAdapter*> m_lAdapters;

    State m_State {State::UNFILLED};

    ViewBase* q_ptr;

private:
    bool nothing     ();
    bool resetScoll  ();
    bool refresh     ();
    bool refreshFront();
    bool refreshBack ();
    bool error       ();

public Q_SLOTS:
    void slotContentChanged();
    void slotCountChanged();
};

/// Add the same property as the QtQuick.ListView
class ModelIndexGroup final : public ContextExtension
{
public:
    virtual ~ModelIndexGroup() {}
    virtual QVector<QByteArray>& propertyNames() const override;
    virtual QVariant getProperty(AbstractItemAdapter* item, uint id, const QModelIndex& index) const override;
};

#define S ViewBasePrivate::State::
const ViewBasePrivate::State ViewBasePrivate::m_fStateMap[5][5] = {
/*              INSERTION    REMOVAL       MOVE     RESET_SCROLL    SCROLL  */
/*UNFILLED */ { S ANCHORED, S UNFILLED , S UNFILLED , S UNFILLED, S UNFILLED },
/*ANCHORED */ { S ANCHORED, S ANCHORED , S ANCHORED , S ANCHORED, S SCROLLED },
/*SCROLLED */ { S SCROLLED, S SCROLLED , S SCROLLED , S ANCHORED, S SCROLLED },
/*AT_END   */ { S AT_END  , S AT_END   , S AT_END   , S ANCHORED, S SCROLLED },
/*ERROR    */ { S ERROR   , S ERROR    , S ERROR    , S ERROR   , S ERROR    },
};
#undef S

#define A &ViewBasePrivate::
const ViewBasePrivate::StateF ViewBasePrivate::m_fStateMachine[5][5] = {
/*              INSERTION           REMOVAL          MOVE        RESET_SCROLL     SCROLL  */
/*UNFILLED*/ { A refreshFront, A refreshFront , A refreshFront , A nothing   , A nothing  },
/*ANCHORED*/ { A refreshFront, A refreshFront , A refreshFront , A nothing   , A refresh  },
/*SCROLLED*/ { A refresh     , A refresh      , A refresh      , A resetScoll, A refresh  },
/*AT_END  */ { A refreshBack , A refreshBack  , A refreshBack  , A resetScoll, A refresh  },
/*ERROR   */ { A error       , A error        , A error        , A error     , A error    },
};
#undef A

ViewBase::ViewBase(QQuickItem* parent) : Flickable(parent), d_ptr(new ViewBasePrivate())
{
    d_ptr->q_ptr = this;
}

ViewBase::~ViewBase()
{
    if (m_pItemVars)
        delete m_pItemVars;

    delete d_ptr;
}

void ViewBase::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    Q_UNUSED(oldGeometry)
    contentItem()->setWidth(newGeometry.width());

    // Resize the viewport(s)
    for (auto ma : qAsConst(d_ptr->m_lAdapters)) {
        const auto vps = ma->viewports();
        for (auto vp : qAsConst(vps))
            vp->resize(newGeometry);
    }
}

AbstractItemAdapter* ViewBase::itemForIndex(const QModelIndex& idx) const
{
    Q_ASSERT(d_ptr->m_lAdapters.size()); //FIXME worry about this later

    return d_ptr->m_lAdapters.constFirst()->itemForIndex(idx);
}

void ViewBase::reload()
{
    Q_ASSERT(false);
}

void ViewBasePrivate::slotContentChanged()
{
    emit q_ptr->contentChanged();
}

void ViewBasePrivate::slotCountChanged()
{
//     emit q_ptr->countChanged();
}

bool ViewBasePrivate::nothing()
{ return true; }

bool ViewBasePrivate::resetScoll()
{
    return true;
}

bool ViewBasePrivate::refresh()
{
    return true;
}

bool ViewBasePrivate::refreshFront()
{
    return true;
}

bool ViewBasePrivate::refreshBack()
{
    return true;
}

bool ViewBasePrivate::error()
{
    Q_ASSERT(false);
    return true;
}

void ViewBase::refresh()
{
    //TODO
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

QVariant ModelIndexGroup::getProperty(AbstractItemAdapter* item, uint id, const QModelIndex& index) const
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

void ViewBase::addModelAdapter(ModelAdapter* a)
{
    connect(a, &ModelAdapter::contentChanged,
        d_ptr, &ViewBasePrivate::slotContentChanged);
//FIXME it was wrong and broken anyway
//     connect(a, &ModelAdapter::countChanged,
//         d_ptr, &ViewBasePrivate::slotCountChanged);

    a->contextAdapterFactory()->addContextExtension(new ModelIndexGroup());

    d_ptr->m_lAdapters << a;
}

void ViewBase::removeModelAdapter(ModelAdapter* a)
{
    Q_ASSERT(false); //TODO
    d_ptr->m_lAdapters.removeAll(a);
}

QVector<ModelAdapter*> ViewBase::modelAdapters() const
{
    return d_ptr->m_lAdapters;
}

bool ViewBase::isEmpty() const
{
    for (auto a : qAsConst(d_ptr->m_lAdapters)) {
        if (a->isEmpty())
            return false;
    }

    return true;
}

Qt::Corner ViewBase::gravity() const
{
    return d_ptr->m_Corner;
}

void ViewBase::setGravity(Qt::Corner g)
{
    d_ptr->m_Corner = g;
    refresh();
}

#include <viewbase.moc>
