/***************************************************************************
 *   Copyright (C) 2017-2018 by Emmanuel Lepage Vallee                     *
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
#include "treetraversalreflector_p.h"

// KQuickItemViews
#include "statetracker/viewitem_p.h"
#include "adapters/abstractitemadapter.h"
#include "viewport.h"
#include "viewport_p.h"
#include "indexmetadata_p.h"

// Qt
#include <QQmlComponent>
#include <QtCore/QDebug>
#include <QtCore/QTimer>

using EdgeType = TreeTraversalReflector::EdgeType;

#include <private/treestructure_p.h>

namespace StateTracker {

/**
 * Hold the metadata associated with the QModelIndex.
 */
struct ModelItem : public TreeTraversalBase
{
    explicit ModelItem(TreeTraversalReflectorPrivate* d);

    enum class State {
        NEW       = 0, /*!< During creation, not part of the tree yet         */
        BUFFER    = 1, /*!< Not in the viewport, but close                    */
        REMOVED   = 2, /*!< Currently in a removal transaction                */
        REACHABLE = 3, /*!< The [grand]parent of visible indexes              */
        VISIBLE   = 4, /*!< The element is visible on screen                  */
        ERROR     = 5, /*!< Something went wrong                              */
        DANGLING  = 6, /*!< Being destroyed                                   */
    };

    typedef bool(ModelItem::*StateF)();

    virtual ~ModelItem() {}

    static const State  m_fStateMap    [7][7];
    static const StateF m_fStateMachine[7][7];

    bool nothing();
    bool error  ();
    bool show   ();
    bool hide   ();
    bool remove ();
    bool attach ();
    bool detach ();
    bool refresh();
    bool move   ();
    bool destroy();
    bool reset  ();

    ModelItem* loadUp  () const;
    ModelItem* loadDown() const;

    uint m_Depth {0};
    State m_State {State::BUFFER};

    // Managed by the Viewport
    bool m_IsCollapsed {false}; //TODO change the default to true

    IndexMetadata m_Geometry;

    //TODO keep the absolute index of the GC circular buffer

    TreeTraversalReflectorPrivate* d_ptr;
};

}

#define TTI(i) static_cast<StateTracker::ModelItem*>(i)


/// Allows more efficient batching of actions related to consecutive items
class ModelItemRange //TODO
{
public:
    //TODO
    explicit ModelItemRange(StateTracker::ModelItem* topLeft, StateTracker::ModelItem* bottomRight);
    explicit ModelItemRange(
        Qt::Edge e, TreeTraversalReflector::EdgeType t, StateTracker::ModelItem* u
    );

    enum class Action {
        //TODO
    };

    bool apply();
};

/// Allow recycling of StateTracker::ViewItem
class ViewItemPool
{
    //TODO
};

class TreeTraversalReflectorPrivate : public QObject
{
    Q_OBJECT
public:

    // Helpers
    StateTracker::ModelItem* addChildren(StateTracker::ModelItem* parent, const QModelIndex& index);
    StateTracker::ModelItem* ttiForIndex(const QModelIndex& idx) const;

    bool isInsertActive(const QModelIndex& p, int first, int last) const;

    QList<TreeTraversalBase*> setTemporaryIndices(const QModelIndex &parent, int start, int end,
                             const QModelIndex &destination, int row);
    void resetTemporaryIndices(const QList<TreeTraversalBase*>&);

    void reloadEdges();
    void resetEdges();

    StateTracker::ModelItem *lastItem() const;

    StateTracker::ModelItem* m_pRoot {new StateTracker::ModelItem(this)};

    //TODO add a circular buffer to GC the items
    // * relative index: when to trigger GC
    // * absolute array index: kept in the StateTracker::ModelItem so it can
    //   remove itself

    /// All elements with loaded children
    QHash<QPersistentModelIndex, StateTracker::ModelItem*> m_hMapper;
    QAbstractItemModel* m_pModel {nullptr};
    QAbstractItemModel* m_pTrackedModel {nullptr};
    std::function<AbstractItemAdapter*()> m_fFactory;
    Viewport *m_pViewport;

    TreeTraversalReflector::TrackingState m_TrackingState {TreeTraversalReflector::TrackingState::NO_MODEL};

    ModelRect m_lRects[3];
    ModelRect* edges(TreeTraversalReflector::EdgeType e) const;
    void setEdge(TreeTraversalReflector::EdgeType et, TreeTraversalBase* tti, Qt::Edge e);

    TreeTraversalReflector* q_ptr;

    // Actions, do not call directly
    void track();
    void untrack();
    void nothing();
    void reset();
    void free();
    void error();
    void populate();
    void fill();
    void trim();

    // Update the ModelRect
    void enterState(StateTracker::ModelItem*, StateTracker::ModelItem::State);
    void leaveState(StateTracker::ModelItem*, StateTracker::ModelItem::State);
    void error     (StateTracker::ModelItem*, StateTracker::ModelItem::State);

    typedef void(TreeTraversalReflectorPrivate::*StateF)();
    typedef void(TreeTraversalReflectorPrivate::*StateFS)(StateTracker::ModelItem*, StateTracker::ModelItem::State);
    static const TreeTraversalReflector::TrackingState m_fStateMap[4][7];
    static const StateF m_fStateMachine[4][7];
    static const StateFS m_fStateLogging[7][2];

    // Tests
    void _test_validateTree(StateTracker::ModelItem* p);
    void _test_validateViewport(bool skipVItemState = false);
    void _test_validateLinkedList(bool skipVItemState = false);
    void _test_validate_edges();
    void _test_validate_move(TreeTraversalBase* parentTTI,TreeTraversalBase* startTTI,
                             TreeTraversalBase* endTTI, TreeTraversalBase* newPrevTTI,
                             TreeTraversalBase* newNextTTI, int row);


public Q_SLOTS:
    void cleanup();
    void slotRowsInserted  (const QModelIndex& parent, int first, int last);
    void slotRowsRemoved   (const QModelIndex& parent, int first, int last);
    void slotLayoutChanged (                                              );
    void slotRowsMoved     (const QModelIndex &p, int start, int end,
                            const QModelIndex &dest, int row);
};

#include <treetraversalreflector_debug.h>

#define S TreeTraversalReflector::TrackingState::
const TreeTraversalReflector::TrackingState TreeTraversalReflectorPrivate::m_fStateMap[4][7] = {
/*                POPULATE     DISABLE      ENABLE       RESET        FREE         MOVE         TRIM   */
/*NO_MODEL */ { S NO_MODEL , S NO_MODEL , S NO_MODEL, S NO_MODEL, S NO_MODEL , S NO_MODEL , S NO_MODEL },
/*PAUSED   */ { S POPULATED, S PAUSED   , S TRACKING, S PAUSED  , S PAUSED   , S PAUSED   , S PAUSED   },
/*POPULATED*/ { S TRACKING , S PAUSED   , S TRACKING, S PAUSED  , S POPULATED, S POPULATED, S POPULATED},
/*TRACKING */ { S TRACKING , S POPULATED, S TRACKING, S TRACKING, S TRACKING , S TRACKING , S TRACKING },
};
#undef S

// This state machine is self healing, error can be called in release mode
// and it will only disable the view without further drama.
#define A &TreeTraversalReflectorPrivate::
const TreeTraversalReflectorPrivate::StateF TreeTraversalReflectorPrivate::m_fStateMachine[4][7] = {
/*               POPULATE     DISABLE    ENABLE     RESET       FREE       MOVE   ,   TRIM */
/*NO_MODEL */ { A nothing , A nothing, A nothing, A nothing, A nothing, A nothing , A error },
/*PAUSED   */ { A populate, A nothing, A error  , A  reset , A  free  , A nothing , A trim  },
/*POPULATED*/ { A error   , A nothing, A track  , A  reset , A  free  , A fill    , A trim  },
/*TRACKING */ { A nothing , A untrack, A nothing, A  reset , A  free  , A fill    , A trim  },
};

// Keep track of the number of instances per state
const TreeTraversalReflectorPrivate::StateFS TreeTraversalReflectorPrivate::m_fStateLogging[7][2] = {
/*                ENTER           LEAVE    */
/*NEW      */ { A error     , A leaveState },
/*BUFFER   */ { A enterState, A leaveState },
/*REMOVED  */ { A enterState, A leaveState },
/*REACHABLE*/ { A enterState, A leaveState },
/*VISIBLE  */ { A enterState, A leaveState },
/*ERROR    */ { A enterState, A leaveState },
/*DANGLING */ { A enterState, A error      },
};
#undef A

#define S StateTracker::ModelItem::State::
const StateTracker::ModelItem::State StateTracker::ModelItem::m_fStateMap[7][7] = {
/*                 SHOW         HIDE        ATTACH     DETACH      UPDATE       MOVE         RESET    */
/*NEW      */ { S ERROR  , S ERROR    , S REACHABLE, S ERROR    , S ERROR  , S ERROR    , S ERROR     },
/*BUFFER   */ { S VISIBLE, S BUFFER   , S BUFFER   , S DANGLING , S BUFFER , S BUFFER   , S BUFFER    },
/*REMOVED  */ { S ERROR  , S ERROR    , S ERROR    , S REACHABLE, S ERROR  , S ERROR    , S ERROR     },
/*REACHABLE*/ { S VISIBLE, S REACHABLE, S ERROR    , S REACHABLE, S ERROR  , S REACHABLE, S REACHABLE },
/*VISIBLE  */ { S VISIBLE, S BUFFER   , S ERROR    , S ERROR    , S VISIBLE, S VISIBLE  , S VISIBLE   },
/*ERROR    */ { S ERROR  , S ERROR    , S ERROR    , S ERROR    , S ERROR  , S ERROR    , S ERROR     },
/*DANGLING */ { S ERROR  , S ERROR    , S ERROR    , S ERROR    , S ERROR  , S ERROR    , S ERROR     },
};
#undef S

#define A &StateTracker::ModelItem::
const StateTracker::ModelItem::StateF StateTracker::ModelItem::m_fStateMachine[7][7] = {
/*                 SHOW       HIDE      ATTACH     DETACH      UPDATE    MOVE     RESET  */
/*NEW      */ { A error  , A nothing, A nothing, A error   , A error  , A error, A error },
/*BUFFER   */ { A show   , A remove , A attach , A destroy , A refresh, A move , A reset },
/*REMOVED  */ { A error  , A error  , A error  , A detach  , A error  , A error, A reset },
/*REACHABLE*/ { A show   , A nothing, A error  , A detach  , A error  , A move , A reset },
/*VISIBLE  */ { A nothing, A hide   , A error  , A error   , A refresh, A move , A reset },
/*ERROR    */ { A error  , A error  , A error  , A error   , A error  , A error, A error },
/*DANGLING */ { A error  , A error  , A error  , A error   , A error  , A error, A error },
};
#undef A

StateTracker::ModelItem::ModelItem(TreeTraversalReflectorPrivate* d):
  TreeTraversalBase(), d_ptr(d), m_Geometry(this, d->m_pViewport)
{
}

StateTracker::ModelItem* StateTracker::ModelItem::loadUp() const
{
    Q_ASSERT(index().model() == d_ptr->m_pModel);

    if (auto tti = TTI(up()))
        return tti;

    //TODO load more than 1 at a time (probably std::max(remaining, buffer)
    if (auto row = effectiveRow()) {

        // It will only work if the parent is already loaded. In the worst
        // case scenario, "up" is the only children of an item whose own parent
        // only has one children and so on for an undefined number of depth
        // iterations.
        const auto pidx = effectiveParentIndex();
        auto cur = pidx;

        QVector<QModelIndex> rewind;

        while (cur.isValid()) {
            auto pitem = d_ptr->m_hMapper.value(cur);

            if (pitem)
                break;

            rewind << cur;
            cur = cur.parent();
        }

        // Load each parents
        for (auto parIdx : qAsConst(rewind)) {
            d_ptr->slotRowsInserted(parIdx.parent(), parIdx.row(), parIdx.row());
        }

        d_ptr->slotRowsInserted(effectiveParentIndex(), row-1, row-1);
        d_ptr->_test_validateViewport();

        auto tti = TTI(up());
        Q_ASSERT(tti);

        return tti;
    }

    Q_ASSERT(parent() == d_ptr->m_pRoot); //TODO recurse parent->rowCount..->last

    // In theory, the parent is always loaded, so there is no other case
    return TTI(parent() == d_ptr->m_pRoot ? nullptr : parent());
}

StateTracker::ModelItem* StateTracker::ModelItem::loadDown() const
{
    Q_ASSERT(index().model() == d_ptr->m_pModel);

    if (auto tti = down()) {
        if (tti->parent() == parent())
            Q_ASSERT(!d_ptr->m_pModel->rowCount(index()));
        return TTI(tti);
    }

    // Check if there is children
    if (int childrenCount = d_ptr->m_pModel->rowCount(index())) {
        Q_UNUSED(childrenCount) //TODO load more than 1
        d_ptr->slotRowsInserted(index(), 0, 0);
        d_ptr->_test_validateViewport();

        auto tti = TTI(down());
        Q_ASSERT(tti);

        return tti;
    }

    //TODO load more than 1 at a time (probably std::max(remaining, buffer)
    int rc = d_ptr->m_pModel->rowCount(effectiveParentIndex());
    if (effectiveRow() < rc-1) {
        d_ptr->slotRowsInserted(effectiveParentIndex(), effectiveRow()+1, effectiveRow()+1);
        d_ptr->_test_validateViewport();
        auto tti = TTI(down());
        Q_ASSERT(tti);

        return tti;
    }

    const TreeTraversalBase *i = this;

    // Rewind until a (grand)parent with more to load is found
    while ((i = i->parent()) != d_ptr->m_pRoot) {
        Q_ASSERT(i);

        if ((rc = d_ptr->m_pModel->rowCount(i->effectiveParentIndex())) && rc > i->effectiveRow() + 1) {
            d_ptr->slotRowsInserted(i->effectiveParentIndex(), i->effectiveRow() + 1, i->effectiveRow()+1);
            auto tti = TTI(down());
            Q_ASSERT(tti);

            return tti;
        }
    }

    d_ptr->_test_validateViewport();
    Q_ASSERT(i == d_ptr->m_pRoot);
    return nullptr;
}

bool IndexMetadata::performAction(IndexMetadata::TrackingAction a)
{
    Q_ASSERT(modelTracker()->m_State != StateTracker::ModelItem::State::ERROR);
    const int s     = (int)modelTracker()->m_State;
    auto nextState  = modelTracker()->m_State = modelTracker()->m_fStateMap[s][(int)a];
    Q_ASSERT(modelTracker()->m_State != StateTracker::ModelItem::State::ERROR);

    // This need to be done before calling the transition function because it
    // can trigger another round of state change.
    if (s != (int)modelTracker()->m_State) {
        modelTracker()->d_ptr->leaveState(modelTracker(), (StateTracker::ModelItem::State)s);
        modelTracker()->d_ptr->enterState(modelTracker(), modelTracker()->m_State );
    }

    bool ret = (this->modelTracker()->*StateTracker::ModelItem::m_fStateMachine[s][(int)a])();

    //WARNING, do not access modelTracker() form here, it might have been deleted

    Q_ASSERT(nextState == StateTracker::ModelItem::State::DANGLING ||
        ((!modelTracker()->m_Geometry.viewTracker())
        ||  modelTracker()->m_State == StateTracker::ModelItem::State::BUFFER
        ||  modelTracker()->m_State == StateTracker::ModelItem::State::VISIBLE));

//     modelTracker()->d_ptr->_test_validate_edges();

    return ret;
}

TreeTraversalReflector::TrackingState TreeTraversalReflector::performAction(TreeTraversalReflector::TrackingAction a)
{
    const int s = (int)d_ptr->m_TrackingState;
    d_ptr->m_TrackingState = d_ptr->m_fStateMap[s][(int)a];

    (d_ptr->*TreeTraversalReflectorPrivate::m_fStateMachine[s][(int)a])();

    return d_ptr->m_TrackingState;
}

bool StateTracker::ModelItem::nothing()
{ return true; }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
bool StateTracker::ModelItem::error()
{
    Q_ASSERT(false);
    return false;
}
#pragma GCC diagnostic pop

bool StateTracker::ModelItem::show()
{
    Q_ASSERT(m_State == State::VISIBLE);

    //FIXME do this less often
    if (down() && TTI(down())->m_Geometry.isValid())
        d_ptr->m_pViewport->s_ptr->notifyInsert(&TTI(down())->m_Geometry);

    if (auto item = m_Geometry.viewTracker()) {
        //item->setVisible(true);
        Q_ASSERT(false); //TODO
    }
    else {
        m_Geometry.setViewTracker(d_ptr->q_ptr->d_ptr->m_fFactory()->s_ptr);
        Q_ASSERT(m_Geometry.viewTracker());
        m_Geometry.viewTracker()->m_pTTI = this;
        Q_ASSERT((!down()) || !TTI(down())->m_Geometry.isValid());

        m_Geometry.viewTracker()->performAction(StateTracker::ViewItem::ViewAction::ATTACH);
        Q_ASSERT(m_Geometry.viewTracker()->m_State == StateTracker::ViewItem::State::POOLED);
        Q_ASSERT(m_State == State::VISIBLE);
//         Q_ASSERT((!m_Geometry.viewTracker()->item())
//             || m_Geometry.removeMe() == (int)StateTracker::Geometry::State::SIZE
//         );
        Q_ASSERT((!down()) || !TTI(down())->m_Geometry.isValid());

        if (auto item = m_Geometry.viewTracker()->item())
            qDebug() << "CREATE" << this << item->y() << item->height();
    }
    Q_ASSERT((!down()) || !TTI(down())->m_Geometry.isValid());

    //d_ptr->_test_validateViewport(true);
    m_Geometry.viewTracker()->performAction(StateTracker::ViewItem::ViewAction::ENTER_BUFFER);
    Q_ASSERT(m_State == State::VISIBLE);
    //d_ptr->_test_validateViewport(true);
    Q_ASSERT(m_Geometry.viewTracker()->m_State == StateTracker::ViewItem::State::BUFFER);
    Q_ASSERT((!down()) || !TTI(down())->m_Geometry.isValid());

    //d_ptr->_test_validateViewport(true);
    m_Geometry.viewTracker()->performAction(StateTracker::ViewItem::ViewAction::ENTER_VIEW);
    Q_ASSERT(m_State == State::VISIBLE);
    //d_ptr->_test_validateViewport();
//     Q_ASSERT((!down()) || TTI(down())->m_Geometry.m_State.state() != StateTracker::Geometry::State::VALID);

    d_ptr->m_pViewport->s_ptr->updateGeometry(&m_Geometry);

    // For some reason creating the visual element failed, this can and will
    // happen and need to be recovered from.
    if (m_Geometry.viewTracker()->hasFailed()) {
        m_Geometry.viewTracker()->performAction(StateTracker::ViewItem::ViewAction::LEAVE_BUFFER);
        Q_ASSERT(m_State == State::VISIBLE);
        m_Geometry.setViewTracker(nullptr);
    }
//     Q_ASSERT((!down()) || TTI(down())->m_Geometry.m_State.state() != StateTracker::Geometry::State::VALID);

    if (auto item = m_Geometry.viewTracker()->item())
        qDebug() << "IN SHOW" << this << item->y() << item->height();

    Q_ASSERT(m_State == State::VISIBLE);

//     Q_ASSERT((!down()) || TTI(down())->m_Geometry.m_State.state() != StateTracker::Geometry::State::VALID);

    // Update the edges
    if (m_State == StateTracker::ModelItem::State::VISIBLE) {
        auto first = d_ptr->edges(TreeTraversalReflector::EdgeType::VISIBLE)->getEdge(Qt::TopEdge);
        auto prev = up();
        auto next = down();
        auto last = d_ptr->edges(TreeTraversalReflector::EdgeType::VISIBLE)->getEdge(Qt::BottomEdge);
//         qDebug() << "VISIBLE!" << first << prev << tti << next << last;
//         qDebug() << first << tti;
//         qDebug() << last << tti;

        // Make sure the geometry is up to data
        m_Geometry.decoratedGeometry();
        Q_ASSERT(m_Geometry.isValid());

        if (first == next) {
            d_ptr->setEdge(TreeTraversalReflector::EdgeType::VISIBLE, this, Qt::TopEdge);
        }

        if (prev == last) {
            d_ptr->setEdge(TreeTraversalReflector::EdgeType::VISIBLE, this, Qt::BottomEdge);
        }

        Q_ASSERT(this != first);
        Q_ASSERT(this != last);
    }
    else
        Q_ASSERT(false); //TODO handle failed elements

    d_ptr->_test_validateViewport();

    return true;
}

bool StateTracker::ModelItem::hide()
{
    qDebug() << "\n\nHIDE";

    if (auto item = m_Geometry.viewTracker()) {
        //item->setVisible(false);
        m_Geometry.viewTracker()->performAction(StateTracker::ViewItem::ViewAction::LEAVE_BUFFER);
    }

    return true;
}

bool StateTracker::ModelItem::remove()
{

    if (m_Geometry.viewTracker()) {
        Q_ASSERT(m_Geometry.viewTracker()->m_State != StateTracker::ViewItem::State::POOLED);
        Q_ASSERT(m_Geometry.viewTracker()->m_State != StateTracker::ViewItem::State::POOLING);
        Q_ASSERT(m_Geometry.viewTracker()->m_State != StateTracker::ViewItem::State::DANGLING);

        // Move the item lifecycle forward
        while (m_Geometry.viewTracker()->m_State != StateTracker::ViewItem::State::POOLED
          && m_Geometry.viewTracker()->m_State != StateTracker::ViewItem::State::DANGLING)
            m_Geometry.viewTracker()->performAction(StateTracker::ViewItem::ViewAction::DETACH);

        // It should still exists, it may crash otherwise, so make sure early
        Q_ASSERT(m_Geometry.viewTracker()->m_State == StateTracker::ViewItem::State::POOLED
            || m_Geometry.viewTracker()->m_State == StateTracker::ViewItem::State::DANGLING
        );

        m_Geometry.setViewTracker(nullptr);
        qDebug() << "\n\nREMOVE!!!" << this;
    }

//     qDebug() << "HIDE";
    return true;
}

bool StateTracker::ModelItem::attach()
{
    Q_ASSERT(m_State != State::VISIBLE);
    // Attach can only be called when there is room in the viewport //FIXME and the buffer
//     Q_ASSERT(d_ptr->edges(EdgeType::FREE)->m_Edges & (Qt::TopEdge|Qt::BottomEdge));


    Q_ASSERT(!m_Geometry.viewTracker());

//     if (m_Geometry.viewTracker())
//         m_Geometry.viewTracker()->performAction(StateTracker::ViewItem::ViewAction::ATTACH);

    const auto upTTI   = TTI(up());
    const auto downTTI = TTI(down());

    //FIXME this ain't correct
    return m_Geometry.performAction(IndexMetadata::TrackingAction::SHOW);
}

bool StateTracker::ModelItem::detach()
{
//     Q_ASSERT(false);

    const auto children = allLoadedChildren();

    // First, detach any remaining children
    for (auto i : qAsConst(children)) {
        auto tti = TTI(i);
        tti->m_Geometry.performAction(IndexMetadata::TrackingAction::HIDE);
        tti->m_Geometry.performAction(IndexMetadata::TrackingAction::DETACH);
    }

    Q_ASSERT(!loadedChildrenCount());

    Q_ASSERT(m_State == State::REACHABLE || m_State == State::DANGLING);

    auto e = d_ptr->edges(EdgeType::FREE);

    Q_ASSERT(!((!e->getEdge(Qt::BottomEdge)) ^ (!e->getEdge(Qt::TopEdge  ))));
    Q_ASSERT(!((!e->getEdge(Qt::LeftEdge  )) ^ (!e->getEdge(Qt::RightEdge))));

    // Update the viewport
//     if (e->getEdge(Qt::TopEdge) == this)
//         e->setEdge(up() ? up() : down(), Qt::TopEdge);

//     if (e->getEdge(Qt::BottomEdge) == this)
//         e->setEdge(down() ? down() : up(), Qt::BottomEdge);

    const auto p = parent();

    d_ptr->m_pViewport->s_ptr->notifyRemoval(&m_Geometry);
    StateTracker::ModelItem::remove();
    TreeTraversalBase::remove();
    d_ptr->m_pViewport->s_ptr->refreshVisible();

    if (p)
        Q_ASSERT(!p->childrenLookup(index()));

    //d_ptr->_test_validateViewport(true);

//     d_ptr->_test_validateViewport();
//
    //FIXME set the parent firstChild() correctly and add an insert()/move() method
    // then drop bridgeGap

    Q_ASSERT(!m_Geometry.viewTracker());
    qDebug() << "\n\nDETACHED!";

    return true;
}

bool StateTracker::ModelItem::refresh()
{
    //
//     qDebug() << "REFRESH";

    d_ptr->m_pViewport->s_ptr->updateGeometry(&m_Geometry);

    for (auto i = TTI(firstChild()); i; i = TTI(i->nextSibling())) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->m_Geometry.performAction(IndexMetadata::TrackingAction::UPDATE);

        if (i == lastChild())
            break;
    }

    Q_ASSERT(m_Geometry.viewTracker()); //FIXME

    return true;
}

bool StateTracker::ModelItem::move()
{
    //TODO remove this implementation and use the one below

    // Propagate
    for (auto i = TTI(firstChild()); i; i = TTI(i->nextSibling())) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        qDebug() << "MOVE CHILD" << i;
        i->m_Geometry.performAction(IndexMetadata::TrackingAction::MOVE);

        if (i == lastChild())
            break;
    }

    qDebug() << "MOVE" << this;
    //FIXME this if should not exists, this should be handled by the state
    // machine.
    if (m_Geometry.viewTracker()) {
        m_Geometry.viewTracker()->performAction(StateTracker::ViewItem::ViewAction::MOVE); //FIXME don't
        Q_ASSERT(m_Geometry.isInSync());
    }
    //TODO update m_Geometry

    Q_ASSERT(m_Geometry.isValid());

    return true;

//     if (!m_Geometry.viewTracker())
//         return true;

//FIXME this is better, but require having createItem() called earlier
//     if (m_Geometry.viewTracker())
//         m_Geometry.viewTracker()->performAction(StateTracker::ViewItem::ViewAction::MOVE); //FIXME don't
//
//     if (oldGeo != m_Geometry.viewTracker()->geometry())
//         if (auto n = m_Geometry.viewTracker()->down())
//             n->parent()->performAction(IndexMetadata::TrackingAction::MOVE);

//     return true;
}

bool StateTracker::ModelItem::destroy()
{
    const auto p = parent();
    Q_ASSERT(p->hasChildren(this));

    detach();

    if (m_Geometry.viewTracker()) {
        m_Geometry.viewTracker()->performAction(StateTracker::ViewItem::ViewAction::DETACH);
    }

    m_Geometry.setViewTracker(nullptr);

    Q_ASSERT(!loadedChildrenCount());

    delete this;

    Q_ASSERT(!p->hasChildren(this));
    return true;
}

bool StateTracker::ModelItem::reset()
{
    for (auto i = TTI(firstChild()); i; i = TTI(i->nextSibling())) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->m_Geometry.performAction(IndexMetadata::TrackingAction::RESET);

        if (i == lastChild())
            break;
    }

    if (m_Geometry.viewTracker()) {
        Q_ASSERT(this != d_ptr->m_pRoot);
        m_Geometry.viewTracker()->performAction(StateTracker::ViewItem::ViewAction::LEAVE_BUFFER);
        m_Geometry.viewTracker()->performAction(StateTracker::ViewItem::ViewAction::DETACH);
        m_Geometry.setViewTracker(nullptr);
    }

    m_Geometry.performAction( IndexMetadata::GeometryAction::RESET );

    return true;
    /*return this == d_ptr->m_pRoot ?
        true : m_Geometry.performAction(IndexMetadata::TrackingAction::UPDATE);*/
}

TreeTraversalReflector::TreeTraversalReflector(Viewport* parent) : QObject(parent),
    d_ptr(new TreeTraversalReflectorPrivate())
{
    d_ptr->m_pViewport = parent;
    d_ptr->q_ptr = this;
}

TreeTraversalReflector::~TreeTraversalReflector()
{
    delete d_ptr->m_pRoot;
}

// Setters
void TreeTraversalReflector::setItemFactory(std::function<AbstractItemAdapter*()> factory)
{
    d_ptr->m_fFactory = factory;
}

void TreeTraversalReflectorPrivate::slotRowsInserted(const QModelIndex& parent, int first, int last)
{
    Q_ASSERT((!parent.isValid()) || parent.model() == q_ptr->model());

    qDebug() << "IN SOLT ROWS INSERTED!" << first << last << parent;
    if (!isInsertActive(parent, first, last)) {
        _test_validateLinkedList();
        qDebug() << "INACTIVE, BYE" << parent << first << last;
//         Q_ASSERT(false); //FIXME so I can't forget when time comes
        return;
    }

    auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;

    //FIXME it is possible if the anchor is at the bottom that the parent
    // needs to be loaded
    if (!pitem) {
        qDebug() << "NO PARENT, BYE";
        return;
    }

    Q_ASSERT((!pitem->firstChild()) || pitem->lastChild());

    StateTracker::ModelItem *prev(nullptr);

    //FIXME use up()
    if (first && pitem)
        prev = TTI(pitem->childrenLookup(q_ptr->model()->index(first-1, 0, parent)));

    auto oldFirst = pitem->firstChild();

    // There is no choice here but to load a larger subset to avoid holes, in
    // theory, edges(EdgeType::FREE)->m_Edges will prevent runaway loading
    if (pitem->firstChild())
        last = std::max(last, pitem->firstChild()->effectiveRow() - 1);

    if (prev && prev->down()) {
        m_pViewport->s_ptr->notifyInsert(&TTI(prev->down())->m_Geometry);
        Q_ASSERT(!TTI(prev->down())->m_Geometry.isValid());
    }
    else if ((!prev) && (!pitem) && m_pRoot->firstChild()) {
        Q_ASSERT(!first);
        Q_ASSERT(!parent.isValid());
        m_pViewport->s_ptr->notifyInsert(&TTI(m_pRoot->firstChild())->m_Geometry);
        Q_ASSERT(!TTI(m_pRoot->firstChild())->m_Geometry.isValid());
    }
    else if (pitem && pitem != m_pRoot && pitem->down()) {
        Q_ASSERT(!first);
        m_pViewport->s_ptr->notifyInsert(&TTI(pitem->down())->m_Geometry);
        Q_ASSERT(!TTI(pitem->down())->m_Geometry.isValid());
    }

    //FIXME support smaller ranges
    for (int i = first; i <= last; i++) {
        auto idx = q_ptr->model()->index(i, 0, parent);
        Q_ASSERT(idx.isValid() && idx.parent() != idx && idx.model() == q_ptr->model());

        // If the insertion is sandwiched between loaded items, not doing it
        // will corrupt the view, but if it's a "tail" insertion, then they
        // can be discarded.
        if (!(edges(EdgeType::FREE)->m_Edges & (Qt::BottomEdge | Qt::TopEdge))) {

            qDebug() << "\n\nNO SPACE LEFT" << prev
                << (prev && prev->down() ? "down" : "nodown")
                << (bool) (edges(EdgeType::FREE)->m_Edges & Qt::BottomEdge)
                << (bool) (edges(EdgeType::FREE)->m_Edges & Qt::TopEdge);

            m_pViewport->s_ptr->refreshVisible();
            return; //FIXME break
        }

        auto e = addChildren(pitem, idx);

//         e->parent()->_test_validate_chain();

        if (pitem->firstChild() && pitem->firstChild()->effectiveRow() == idx.row()+1) {
            Q_ASSERT(idx.parent() == pitem->firstChild()->effectiveParentIndex());

//             m_pViewport->s_ptr->notifyInsert(&TTI(pitem->firstChild())->m_Geometry);
            TreeTraversalBase::insertChildBefore(e, pitem->firstChild(), pitem);
        }
        else {
//             m_pViewport->s_ptr->notifyInsert(&TTI(prev)->m_Geometry);
            TreeTraversalBase::insertChildAfter(e, prev, pitem); //FIXME incorrect
        }

        //FIXME It can happen if the previous is out of the visible range
        Q_ASSERT( e->previousSibling() || e->nextSibling() || e->effectiveRow() == 0);

        //TODO merge with bridgeGap
        if (prev) {
            TreeTraversalBase::bridgeGap(prev, e);
            prev->parent()->_test_validate_chain();
        }

        // This is required before ::ATTACH because otherwise ::down() wont work

        Q_ASSERT((!pitem->firstChild()) || !pitem->firstChild()->hasTemporaryIndex()); //TODO

        const bool needDownMove = (!pitem->lastChild()) ||
            e->effectiveRow() > pitem->lastChild()->effectiveRow();

        const bool needUpMove = (!pitem->firstChild()) ||
            e->effectiveRow() < pitem->firstChild()->effectiveRow();

        e->parent()->_test_validate_chain();
        // The item is about the current parent first item
        if (needUpMove) {
//             Q_ASSERT(false); //TODO merge with the other bridgeGap above

            m_pViewport->s_ptr->notifyInsert(&TTI(m_pRoot->firstChild())->m_Geometry);
            Q_ASSERT((!m_pRoot->firstChild()) || !TTI(m_pRoot->firstChild())->m_Geometry.isValid());
            TreeTraversalBase::insertChildBefore(e, nullptr, pitem);
        }
        e->parent()->_test_validate_chain();

        e->parent()->_test_validate_chain();

        Q_ASSERT(e->m_Geometry.removeMe() == (int)StateTracker::Geometry::State::INIT);

//         if (auto ne = TTI(e->down()))
//             Q_ASSERT(ne->m_Geometry.m_State.state() != StateTracker::Geometry::State::VALID);

        Q_ASSERT(e->m_State != StateTracker::ModelItem::State::VISIBLE);

        // NEW -> REACHABLE, this should never fail
        if (!e->m_Geometry.performAction(IndexMetadata::TrackingAction::ATTACH)) {
            qDebug() << "\n\nATTACH FAILED";
            _test_validateLinkedList();
            break;
        }

        if (needUpMove) {
            Q_ASSERT(pitem != e);
            if (auto pe = TTI(e->up()))
                pe->m_Geometry.performAction(IndexMetadata::TrackingAction::MOVE);
        }

        Q_ASSERT((!pitem->lastChild()) || !pitem->lastChild()->hasTemporaryIndex()); //TODO

        if (needDownMove) {
            if (auto ne = TTI(e->down())) {
                Q_ASSERT(!ne->m_Geometry.isValid());

                ne->m_Geometry.performAction(IndexMetadata::TrackingAction::MOVE);
                Q_ASSERT(!ne->m_Geometry.isValid());
            }
        }

        int rc = q_ptr->model()->rowCount(idx);
        if (rc && edges(EdgeType::FREE)->m_Edges & Qt::BottomEdge) {
            slotRowsInserted(idx, 0, rc-1);
        }

        // Validate early to prevent propagating garbage that's nearly impossible
        // to debug.
        if (pitem && pitem != m_pRoot && !i) {
            Q_ASSERT(e->up() == pitem);
            Q_ASSERT(e == pitem->down());
        }

        prev = e;
    }

    m_pViewport->s_ptr->refreshVisible();

    _test_validateLinkedList();
    pitem->_test_validate_chain();

    Q_EMIT q_ptr->contentChanged();

    if (!parent.isValid())
        Q_EMIT q_ptr->countChanged();
}

void TreeTraversalReflectorPrivate::slotRowsRemoved(const QModelIndex& parent, int first, int last)
{
    Q_ASSERT((!parent.isValid()) || parent.model() == q_ptr->model());

    if (!q_ptr->isActive(parent, first, last)) {
        _test_validateLinkedList();
        Q_ASSERT(false);

        return;
    }

    auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;

    if (!pitem)
        return;

    //TODO make sure the state machine support them
    //StateTracker::ModelItem *prev(nullptr), *next(nullptr);

    //FIXME use up()
    //if (first && pitem)
    //    prev = pitem->m_hLookup.value(model()->index(first-1, 0, parent));

    //next = pitem->m_hLookup.value(model()->index(last+1, 0, parent));

    //FIXME support smaller ranges
    for (int i = first; i <= last; i++) {
        auto idx = q_ptr->model()->index(i, 0, parent);

        if (auto elem = TTI(pitem->childrenLookup(idx))) {
            elem->m_Geometry.performAction(IndexMetadata::TrackingAction::HIDE);
            elem->m_Geometry.performAction(IndexMetadata::TrackingAction::DETACH);
        }
    }

    if (!parent.isValid())
        Q_EMIT q_ptr->countChanged();

    Q_EMIT q_ptr->contentChanged();
}

void TreeTraversalReflectorPrivate::slotLayoutChanged()
{
    if (auto rc = q_ptr->model()->rowCount())
        slotRowsInserted({}, 0, rc - 1);

    Q_EMIT q_ptr->contentChanged();

    Q_EMIT q_ptr->countChanged();
}

QList<TreeTraversalBase*> TreeTraversalReflectorPrivate::setTemporaryIndices(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    QList<TreeTraversalBase*> ret;

    //FIXME list only
    // Before moving them, set a temporary now/col value because it wont be set
    // on the index until before slotRowsMoved2 is called (but after this
    // method returns //TODO do not use the hashmap, it is already known
    if (parent == destination) {
        const auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;
        for (int i = start; i <= end; i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);

            auto elem = pitem->childrenLookup(idx);
            Q_ASSERT(elem);

            elem->setTemporaryIndex(
                destination, row + (i - start), idx.column()
            );
            ret << elem;
        }

        for (int i = row; i <= row + (end - start); i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);

            auto elem = pitem->childrenLookup(idx);
            Q_ASSERT(elem);

            elem->setTemporaryIndex(
                destination, row + (end - start) + 1, idx.column()
            );
            ret << elem;
        }
    }

    return ret;
}

void TreeTraversalReflectorPrivate::resetTemporaryIndices(const QList<TreeTraversalBase*>& indices)
{
    for (auto i : qAsConst(indices))
        i->resetTemporaryIndex();
}

void TreeTraversalReflectorPrivate::slotRowsMoved(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    Q_ASSERT((!parent.isValid()) || parent.model() == q_ptr->model());
    Q_ASSERT((!destination.isValid()) || destination.model() == q_ptr->model());

    // There is literally nothing to do
    if (parent == destination && start == row)
        return;

    // Whatever has to be done only affect a part that's not currently tracked.
    if (!q_ptr->isActive(destination, row, row)) {
        Q_ASSERT(false); //TODO so I don't forget
        return;
    }

    auto tmp = setTemporaryIndices(parent, start, end, destination, row);

    // As the actual view is implemented as a daisy chained list, only moving
    // the edges is necessary for the StateTracker::ModelItem. Each StateTracker::ViewItem
    // need to be moved.

    const auto idxStart = q_ptr->model()->index(start, 0, parent);
    const auto idxEnd   = q_ptr->model()->index(end  , 0, parent);
    Q_ASSERT(idxStart.isValid() && idxEnd.isValid());

    //FIXME once partial ranges are supported, this is no longer always valid
    auto startTTI = ttiForIndex(idxStart);
    auto endTTI   = ttiForIndex(idxEnd);

    if (end - start == 1)
        Q_ASSERT(startTTI->nextSibling() == endTTI);

    //FIXME so I don't forget, it will mess things up if silently ignored
    Q_ASSERT(startTTI && endTTI);
    Q_ASSERT(startTTI->parent() == endTTI->parent());

    auto oldPreviousTTI = startTTI->up();
    auto oldNextTTI     = endTTI->down();

    Q_ASSERT((!oldPreviousTTI) || oldPreviousTTI->down() == startTTI);
    Q_ASSERT((!oldNextTTI) || oldNextTTI->up() == endTTI);

    auto newNextIdx = q_ptr->model()->index(row, 0, destination);

    // You cannot move things into an empty model
    Q_ASSERT((!row) || newNextIdx.isValid());

    StateTracker::ModelItem *newNextTTI(nullptr), *newPrevTTI(nullptr);

    // Rewind until a next element is found, this happens when destination is empty
    if (!newNextIdx.isValid() && destination.parent().isValid()) {
        Q_ASSERT(q_ptr->model()->rowCount(destination) == row);
        auto par = destination.parent();
        do {
            if (q_ptr->model()->rowCount(par.parent()) > par.row()) {
                newNextIdx = q_ptr->model()->index(par.row(), 0, par.parent());
                break;
            }

            par = par.parent();
        } while (par.isValid());

        newNextTTI = ttiForIndex(newNextIdx);
    }
    else {
        newNextTTI = ttiForIndex(newNextIdx);
        newPrevTTI = newNextTTI ? TTI(newNextTTI->up()) : nullptr;
    }

    if (!row) {
        auto otherI = ttiForIndex(destination);
        Q_ASSERT((!newPrevTTI) || otherI == newPrevTTI);
    }

    // When there is no next element, then the parent has to be extracted manually
    if (!(newNextTTI || newPrevTTI)) {
        if (!row)
            newPrevTTI = ttiForIndex(destination);
        else {
            newPrevTTI = ttiForIndex(
                destination.model()->index(row-1, 0, destination)
            );
        }
    }

    StateTracker::ModelItem* newParentTTI = ttiForIndex(destination);
    Q_ASSERT(newParentTTI || !destination.isValid()); //TODO not coded yet

    newParentTTI = newParentTTI ? newParentTTI : m_pRoot;

    // Remove everything //TODO add batching again
    TreeTraversalBase* tti = endTTI;
    TreeTraversalBase* dest = newNextTTI && newNextTTI->parent() == newParentTTI ?
        newNextTTI : nullptr;

    QList<TreeTraversalBase*> tmp2;
    do {
        tmp2 << tti;
    } while(endTTI != startTTI && (tti = tti->previousSibling()) != startTTI);

    if (endTTI != startTTI) //FIXME use a better condition
        tmp2 << startTTI;

    Q_ASSERT( (end - start) == tmp2.size() - 1);

    // Check if the point where
    //TODO this isn't good enough
    bool isRangeVisible = (newPrevTTI &&
        newPrevTTI->m_State == StateTracker::ModelItem::State::VISIBLE) || (newNextTTI &&
            newNextTTI->m_State == StateTracker::ModelItem::State::VISIBLE);

    const auto topEdge    = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge));
    const auto bottomEdge = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge));

    bool needRefreshVisibleTop    = false;
    bool needRefreshVisibleBottom = false;
    bool needRefreshBufferTop     = false; //TODO
    bool needRefreshBufferBottom  = false; //TODO

    for (auto item : qAsConst(tmp2)) {
        needRefreshVisibleTop    |= item != topEdge;
        needRefreshVisibleBottom |= item != bottomEdge;

        m_pViewport->s_ptr->notifyRemoval(&TTI(item)->m_Geometry);
        item->TreeTraversalBase::remove();

        TreeTraversalBase::insertChildBefore(item, dest, newParentTTI);

        if (dest)
            m_pViewport->s_ptr->notifyInsert(&TTI(dest)->m_Geometry);

        dest = item;
        if (isRangeVisible)
            TTI(item)->m_Geometry.performAction(IndexMetadata::TrackingAction::SHOW);
    }

    _test_validate_move(newParentTTI, startTTI, endTTI, newPrevTTI, newNextTTI, row);

    resetTemporaryIndices(tmp);
    m_pViewport->s_ptr->refreshVisible();

    // The visible edges are now probably incorectly placed, reload them
        if (needRefreshVisibleTop || needRefreshVisibleBottom) {
        reloadEdges();
        m_pViewport->s_ptr->refreshVisible();
    }
//     _test_validateLinkedList();
}

void TreeTraversalReflectorPrivate::resetEdges()
{
    for (int i = 0; i <= (int) TreeTraversalReflector::EdgeType::BUFFERED; i++) {
        setEdge((TreeTraversalReflector::EdgeType)i, nullptr, Qt::BottomEdge);
        setEdge((TreeTraversalReflector::EdgeType)i, nullptr, Qt::TopEdge   );
        setEdge((TreeTraversalReflector::EdgeType)i, nullptr, Qt::LeftEdge  );
        setEdge((TreeTraversalReflector::EdgeType)i, nullptr, Qt::RightEdge );
    }
}

// Go O(N) for now. Optimize when it becomes a problem (read: soon)
void TreeTraversalReflectorPrivate::reloadEdges()
{
    enum Range : uint16_t {
        TOP            = 0x0 << 0,
        BUFFER_TOP     = 0x1 << 0,
        VISIBLE        = 0x1 << 1,
        BUFFER_BOTTOM  = 0x1 << 2,
        BOTTOM         = 0x1 << 3,
        IS_VISIBLE     = 0x1 << 4,
        IS_NOT_VISIBLE = 0x1 << 5,
        IS_BUFFER      = 0x1 << 6,
        IS_NOT_BUFFER  = 0x1 << 7,
    };

    Range pos = Range::TOP;

    for (auto item = m_pRoot->firstChild(); item; item = item->down()) {
        const uint16_t isVisible = TTI(item)->m_State == StateTracker::ModelItem::State::VISIBLE ?
            IS_VISIBLE : IS_NOT_VISIBLE;
        const uint16_t isBuffer  = TTI(item)->m_State == StateTracker::ModelItem::State::BUFFER  ?
            IS_BUFFER : IS_NOT_BUFFER;

        switch(pos | isVisible | isBuffer) {
            case Range::TOP    | Range::IS_BUFFER:
                pos = Range::BUFFER_TOP;
                setEdge(TreeTraversalReflector::EdgeType::BUFFERED, item, Qt::TopEdge);
                break;
            case Range::TOP        | Range::IS_VISIBLE:
            case Range::BUFFER_TOP | Range::IS_VISIBLE:
                pos = Range::VISIBLE;
                setEdge(TreeTraversalReflector::EdgeType::VISIBLE, item, Qt::TopEdge);
                break;
            case Range::VISIBLE | Range::IS_NOT_VISIBLE:
            case Range::VISIBLE | Range::IS_NOT_BUFFER:
                setEdge(TreeTraversalReflector::EdgeType::VISIBLE, item->up(), Qt::BottomEdge);
                pos = Range::BOTTOM;
                break;
            case Range::VISIBLE | Range::IS_NOT_VISIBLE | Range::IS_BUFFER:
                pos = Range::BUFFER_BOTTOM;
                break;
            case Range::VISIBLE | Range::IS_NOT_BUFFER | Range::IS_NOT_VISIBLE:
                setEdge(TreeTraversalReflector::EdgeType::BUFFERED, item->up(), Qt::BottomEdge);
                pos = Range::BOTTOM;
                break;
        }
    }
}

QAbstractItemModel* TreeTraversalReflector::model() const
{
    return d_ptr->m_pModel;
}

void TreeTraversalReflectorPrivate::track()
{
    Q_ASSERT(!m_pTrackedModel);
    Q_ASSERT(m_pModel);

    connect(m_pModel, &QAbstractItemModel::rowsInserted, this,
        &TreeTraversalReflectorPrivate::slotRowsInserted );
    connect(m_pModel, &QAbstractItemModel::rowsAboutToBeRemoved, this,
        &TreeTraversalReflectorPrivate::slotRowsRemoved  );
    connect(m_pModel, &QAbstractItemModel::layoutAboutToBeChanged, this,
        &TreeTraversalReflectorPrivate::cleanup);
    connect(m_pModel, &QAbstractItemModel::layoutChanged, this,
        &TreeTraversalReflectorPrivate::slotLayoutChanged);
    connect(m_pModel, &QAbstractItemModel::modelAboutToBeReset, this,
        &TreeTraversalReflectorPrivate::cleanup);
    connect(m_pModel, &QAbstractItemModel::modelReset, this,
        &TreeTraversalReflectorPrivate::slotLayoutChanged);
    connect(m_pModel, &QAbstractItemModel::rowsAboutToBeMoved, this,
        &TreeTraversalReflectorPrivate::slotRowsMoved);

// #ifdef QT_NO_DEBUG_OUTPUT
    connect(m_pModel, &QAbstractItemModel::rowsMoved, this,
        [this](){_test_validateLinkedList();});
    connect(m_pModel, &QAbstractItemModel::rowsRemoved, this,
        [this](){_test_validateLinkedList();});
// #endif

    m_pTrackedModel = m_pModel;
}

void TreeTraversalReflectorPrivate::free()
{
    m_pRoot->m_Geometry.performAction(IndexMetadata::TrackingAction::RESET);

    for (int i = 0; i < 3; i++)
        m_lRects[i] = {};

    m_hMapper.clear();
    delete m_pRoot;
    m_pRoot = new StateTracker::ModelItem(this);
}

void TreeTraversalReflectorPrivate::reset()
{
    qDebug() << "RESET";
    m_pRoot->m_Geometry.performAction(IndexMetadata::TrackingAction::RESET);
    resetEdges();
}

void TreeTraversalReflectorPrivate::untrack()
{
    Q_ASSERT(m_pTrackedModel);
    if (!m_pTrackedModel)
        return;

    disconnect(m_pTrackedModel, &QAbstractItemModel::rowsInserted, this,
        &TreeTraversalReflectorPrivate::slotRowsInserted);
    disconnect(m_pTrackedModel, &QAbstractItemModel::rowsAboutToBeRemoved, this,
        &TreeTraversalReflectorPrivate::slotRowsRemoved);
    disconnect(m_pTrackedModel, &QAbstractItemModel::layoutAboutToBeChanged, this,
        &TreeTraversalReflectorPrivate::cleanup);
    disconnect(m_pTrackedModel, &QAbstractItemModel::layoutChanged, this,
        &TreeTraversalReflectorPrivate::slotLayoutChanged);
    disconnect(m_pTrackedModel, &QAbstractItemModel::modelAboutToBeReset, this,
        &TreeTraversalReflectorPrivate::cleanup);
    disconnect(m_pTrackedModel, &QAbstractItemModel::modelReset, this,
        &TreeTraversalReflectorPrivate::slotLayoutChanged);
    disconnect(m_pTrackedModel, &QAbstractItemModel::rowsAboutToBeMoved, this,
        &TreeTraversalReflectorPrivate::slotRowsMoved);

// #ifdef QT_NO_DEBUG_OUTPUT
//     disconnect(m_pTrackedModel, &QAbstractItemModel::rowsMoved, this,
//         [this](){_test_validateLinkedList();});
//     disconnect(m_pTrackedModel, &QAbstractItemModel::rowsRemoved, this,
//         [this](){_test_validateLinkedList();});
// #endif

    m_pTrackedModel = nullptr;
}

void TreeTraversalReflectorPrivate::populate()
{
    Q_ASSERT(m_pModel);

    qDebug() << "\n\nPOPULATE!" << edges(EdgeType::FREE)->m_Edges << (edges(EdgeType::FREE));

    if (!edges(EdgeType::FREE)->m_Edges)
        return;

    if (m_pRoot->firstChild() && (edges(EdgeType::FREE)->m_Edges & (Qt::TopEdge|Qt::BottomEdge))) {
        //Q_ASSERT(edges(EdgeType::FREE)->getEdge(Qt::TopEdge));

        while (edges(EdgeType::FREE)->m_Edges & Qt::TopEdge) {
            const auto was = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge));

            auto u = was->loadUp();

            Q_ASSERT(u || edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge)->effectiveRow() == 0);
            Q_ASSERT(u || !edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge)->effectiveParentIndex().isValid());

            if (!u)
                break;

            u->m_Geometry.performAction(IndexMetadata::TrackingAction::SHOW);
        }

        while (edges(EdgeType::FREE)->m_Edges & Qt::BottomEdge) {
            const auto was = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge));

            auto u = was->loadDown();

            //Q_ASSERT(u || edges(EdgeType::FREE)->getEdge(Qt::BottomEdge)->effectiveRow() == 0);
            //Q_ASSERT(u || !edges(EdgeType::FREE)->getEdge(Qt::BottomEdge)->effectiveParentIndex().isValid());

            if (!u)
                break;

            u->m_Geometry.performAction(IndexMetadata::TrackingAction::SHOW);
        }
    }
    else if (auto rc = m_pModel->rowCount()) {
        //FIX support anchors
        slotRowsInserted({}, 0, rc - 1);
    }
}

void TreeTraversalReflectorPrivate::trim()
{
    return;
//     QTimer::singleShot(0, [this]() {
//         if (m_TrackingState != TreeTraversalReflector::TrackingState::TRACKING)
//             return;
//
//         _test_validateViewport();
//
//         qDebug() << "TRIM" << edges(EdgeType::VISIBLE)->m_Edges;
//     //     if (!edges(EdgeType::FREE)->getEdge(Qt::TopEdge)) {
//     //         Q_ASSERT(!edges(EdgeType::FREE)->getEdge(Qt::BottomEdge));
//     //         return;
//     //     }
//
//         auto elem = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge)); //FIXME have a visual and reacheable rect
//
//         while (!(edges(EdgeType::FREE)->m_Edges & Qt::TopEdge)) {
//             Q_ASSERT(elem);
//             Q_ASSERT(edges(EdgeType::FREE)->getEdge(Qt::TopEdge));
//             Q_ASSERT(!m_pViewport->currentRect().intersects(elem->m_Geometry.geometry()));
//
//             elem->m_Geometry.performAction(IndexMetadata::TrackingAction::HIDE);
//             elem = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge));
//         }
//
//         elem = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge)); //FIXME have a visual and reacheable rect
//         while (!(edges(EdgeType::FREE)->m_Edges & Qt::BottomEdge)) {
//             Q_ASSERT(elem);
//             Q_ASSERT(!m_pViewport->currentRect().intersects(elem->m_Geometry.geometry()));
//             elem->m_Geometry.performAction(IndexMetadata::TrackingAction::HIDE);
//             elem = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge));
//         }
//     });


    // Unload everything below the cache area to get rid of the insertion/moving
    // overhead. Not doing so would also require to handle "holes" in the tree
    // which is very complex and unnecessary.
    TreeTraversalBase *be = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge)); //TODO use BUFFERED
    qDebug() << "TRIM" << be;

    if (!be)
        return;

    TreeTraversalBase *item = lastItem();

    if (be == item)
        return;

    do {
        qDebug() << "DEWTAC" << be << item;
        TTI(item)->m_Geometry.performAction(IndexMetadata::TrackingAction::DETACH);
    } while((item = item->up()) != be);
}

void TreeTraversalReflectorPrivate::fill()
{
    trim();
    populate();
}

void TreeTraversalReflectorPrivate::nothing()
{}

void TreeTraversalReflectorPrivate::error()
{
    Q_ASSERT(false);
    q_ptr->performAction(TreeTraversalReflector::TrackingAction::DISABLE);
    q_ptr->performAction(TreeTraversalReflector::TrackingAction::RESET);
}

void TreeTraversalReflectorPrivate::enterState(StateTracker::ModelItem* tti, StateTracker::ModelItem::State s)
{
    qDebug() << "ENTER STATE!!";


//     _test_validate_edges();
}

void TreeTraversalReflectorPrivate::leaveState(StateTracker::ModelItem *tti, StateTracker::ModelItem::State s)
{
    qDebug() << "LEAVE STATE!!";

    if (s == StateTracker::ModelItem::State::VISIBLE) {
        auto first = edges(TreeTraversalReflector::EdgeType::VISIBLE)->getEdge(Qt::TopEdge);
        auto last = edges(TreeTraversalReflector::EdgeType::VISIBLE)->getEdge(Qt::BottomEdge);

        // The item was somewhere in between, not an edge case
        if ((tti != first) && (tti != last))
            return;

        auto prev = TTI(tti->up());
        auto next = TTI(tti->down());

        if (tti == first)
            setEdge(TreeTraversalReflector::EdgeType::VISIBLE,
                (next && next->m_State == StateTracker::ModelItem::State::VISIBLE) ?
                    next : nullptr,
                Qt::TopEdge
            );

        if (tti == last)
            setEdge(TreeTraversalReflector::EdgeType::VISIBLE,
                (prev && prev->m_State == StateTracker::ModelItem::State::VISIBLE) ?
                    prev : nullptr,
                Qt::BottomEdge
            );
    }
}

void TreeTraversalReflectorPrivate::error(StateTracker::ModelItem*, StateTracker::ModelItem::State)
{
    Q_ASSERT(false);
}

void TreeTraversalReflector::setModel(QAbstractItemModel* m)
{
    if (m == model())
        return;

    performAction(TrackingAction::DISABLE);
    performAction(TrackingAction::RESET);
    performAction(TrackingAction::FREE);

    //TODO don't set the state directly
    if (d_ptr->m_TrackingState == TrackingState::NO_MODEL)
        d_ptr->m_TrackingState = TrackingState::PAUSED;

    Q_ASSERT(!d_ptr->m_pTrackedModel);

    d_ptr->m_pModel = m;

    if (!m)
        return;
}

StateTracker::ModelItem *TreeTraversalReflectorPrivate::lastItem() const
{
    auto candidate = m_pRoot->lastChild();

    while (candidate && candidate->lastChild())
        candidate = candidate->lastChild();

    return TTI(candidate);
}

bool TreeTraversalReflectorPrivate::isInsertActive(const QModelIndex& p, int first, int last) const
{
    return true; //TODO solve all the problems later

    if (edges(EdgeType::FREE)->m_Edges & (Qt::TopEdge|Qt::BottomEdge))
        return true;

    const auto parent = p.isValid() ? ttiForIndex(p) : m_pRoot;

    // It's controversial as it means if the items would otherwise be
    // visible because their parent "virtual" position + insertion height
    // could happen to be in the view but in this case the scrollbar would
    // move. Mobile views tend to not shuffle the current content around so
    // from that point of view it is correct, but if the item is in the
    // buffer, then it will move so doing this is a bit buggy.
    if (!parent)
        return false;

    return parent->withinRange(m_pModel, first - 1, last + 1);
}

/// Return true if the indices affect the current view
bool TreeTraversalReflector::isActive(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)

    if (!parent.isValid()) {
        // There is nothing loaded, so everything is active (so far)
        if (!d_ptr->m_pRoot->firstChild())
            return true;

        // There is room for more. Assuming the code that makes sure all items
        // that can be loaded are, then it only happens when there is room for
        // more.
        if (d_ptr->edges(EdgeType::FREE)->m_Edges & (Qt::BottomEdge | Qt::TopEdge))
            return true;

        const QRect loadedRect(
            d_ptr->m_pRoot->firstChild()->index().column(),
            d_ptr->m_pRoot->firstChild()->index().row(),
            d_ptr->m_pRoot->lastChild()->index().column() + 1,
            d_ptr->m_pRoot->lastChild()->index().row()
        );

        const QRect changedRect(
            0,
            first,
            1, //FIXME use columnCount+!?
            last
        );

        return loadedRect.intersects(changedRect);
    }

    Q_ASSERT(false); //TODO

    return true; //FIXME
}

/// Add new entries to the mapping
StateTracker::ModelItem* TreeTraversalReflectorPrivate::addChildren(StateTracker::ModelItem* parent, const QModelIndex& index)
{
    Q_ASSERT(index.isValid());
    Q_ASSERT(index.parent() != index);

    auto e = new StateTracker::ModelItem(this);
    e->setModelIndex(index);

    const int oldSize(m_hMapper.size()), oldSize2(parent->loadedChildrenCount());

    Q_ASSERT(!m_hMapper.contains(index));

    m_hMapper[index] = e;

    // If the size did not grow, something leaked
    Q_ASSERT(m_hMapper.size() == oldSize+1);

    return e;
}

void TreeTraversalReflectorPrivate::cleanup()
{
    // The whole cleanup cycle isn't necessary, it wont find anything.
    if (!m_pRoot->firstChild())
        return;

    m_pRoot->m_Geometry.performAction(IndexMetadata::TrackingAction::HIDE);
    m_pRoot->m_Geometry.performAction(IndexMetadata::TrackingAction::DETACH);

    m_hMapper.clear();
    m_pRoot = new StateTracker::ModelItem(this);
    //m_FailedCount = 0;
}

StateTracker::ModelItem* TreeTraversalReflectorPrivate::ttiForIndex(const QModelIndex& idx) const
{
    if (!idx.isValid())
        return nullptr;

    if (!idx.parent().isValid())
        return TTI(m_pRoot->childrenLookup(idx));

    if (auto parent = m_hMapper.value(idx.parent()))
        return TTI(parent->childrenLookup(idx));

    return nullptr;
}

AbstractItemAdapter* TreeTraversalReflector::itemForIndex(const QModelIndex& idx) const
{
    const auto tti = d_ptr->ttiForIndex(idx);
    return tti && tti->m_Geometry.viewTracker() ? tti->m_Geometry.viewTracker()->d_ptr : nullptr;
}

IndexMetadata *TreeTraversalReflector::geometryForIndex(const QModelIndex& idx) const
{
    if (const auto tti = d_ptr->ttiForIndex(idx))
        return &tti->m_Geometry;

    return nullptr;
}

void TreeTraversalReflector::setAvailableEdges(Qt::Edges edges, TreeTraversalReflector::EdgeType t)
{
    d_ptr->edges(t)->m_Edges = edges;
}

Qt::Edges TreeTraversalReflector::availableEdges(TreeTraversalReflector::EdgeType  t) const
{
    return d_ptr->edges(t)->m_Edges;
}

QModelIndex IndexMetadata::index() const
{
    return QModelIndex(modelTracker()->index());
}

QPersistentModelIndex StateTracker::ViewItem::index() const
{
    return QModelIndex(m_pTTI->index());
}

/**
 * Flatten the tree as a linked list.
 *
 * Returns the previous non-failed item.
 */
StateTracker::ViewItem* StateTracker::ViewItem::up(AbstractItemAdapter::StateFlags flags) const
{
    Q_UNUSED(flags)
    Q_ASSERT(m_State == State::ACTIVE
        || m_State == State::BUFFER
        || m_State == State::FAILED
        || m_State == State::POOLING
        || m_State == State::DANGLING //FIXME add a new state for
        // "deletion in progress" or swap the f call and set state
    );

    auto ret = TTI(m_pTTI->up());
    //TODO support collapsed nodes

    // Linearly look for a valid element. Doing this here allows the views
    // that implement this (abstract) class to work without having to always
    // check if some of their item failed to load. This is non-fatal in the
    // other Qt views, so it isn't fatal here either.
    while (ret && !ret->m_Geometry.viewTracker())
        ret = TTI(ret->up());

    if (ret && ret->m_Geometry.viewTracker())
        Q_ASSERT(ret->m_Geometry.viewTracker()->m_State != State::POOLING && ret->m_Geometry.viewTracker()->m_State != State::DANGLING);

    auto vi = ret ? ret->m_Geometry.viewTracker() : nullptr;

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
StateTracker::ViewItem* StateTracker::ViewItem::down(AbstractItemAdapter::StateFlags flags) const
{
    Q_UNUSED(flags)

    Q_ASSERT(m_State == State::ACTIVE
        || m_State == State::BUFFER
        || m_State == State::FAILED
        || m_State == State::POOLING
        || m_State == State::DANGLING //FIXME add a new state for
        // "deletion in progress" or swap the f call and set state
    );

    auto ret = TTI(m_pTTI->down());
    //TODO support collapsed entries

    // Recursively look for a valid element. Doing this here allows the views
    // that implement this (abstract) class to work without having to always
    // check if some of their item failed to load. This is non-fatal in the
    // other Qt views, so it isn't fatal here either.
    while (ret && !ret->m_Geometry.viewTracker())
        ret = TTI(ret->down());

    auto vi = ret ? ret->m_Geometry.viewTracker() : nullptr;

    // Do not allow navigating past the loaded edges
    if (vi && (vi->m_State == State::POOLING || vi->m_State == State::POOLED))
        return nullptr;

    return vi;
}

int StateTracker::ViewItem::row() const
{
    return m_pTTI->effectiveRow();
}

int StateTracker::ViewItem::column() const
{
    return m_pTTI->effectiveColumn();
}

IndexMetadata *IndexMetadata::up() const
{
    auto i = TTI(modelTracker()->up());
    return i ? &i->m_Geometry : nullptr;
}

IndexMetadata *IndexMetadata::down() const
{
    auto i = TTI(modelTracker()->down());
    return i ? &i->m_Geometry : nullptr;
}

IndexMetadata *IndexMetadata::left() const
{
    auto i = TTI(modelTracker()->left());
    return i ? &i->m_Geometry : nullptr;
}

IndexMetadata *IndexMetadata::right() const
{
    auto i = TTI(modelTracker()->right());
    return i ? &i->m_Geometry : nullptr;
}

bool IndexMetadata::isTopItem() const
{
    return modelTracker() == modelTracker()->d_ptr->m_pRoot->firstChild();
}

IndexMetadata *TreeTraversalReflector::getEdge(EdgeType t, Qt::Edge e) const
{
    auto ret = TTI(d_ptr->edges(t)->getEdge(e));
    return ret ? &ret->m_Geometry : nullptr;
}

void StateTracker::ViewItem::setCollapsed(bool v)
{
    m_pTTI->m_IsCollapsed = v;
}

bool StateTracker::ViewItem::isCollapsed() const
{
    return m_pTTI->m_IsCollapsed;
}

ModelRect* TreeTraversalReflectorPrivate::edges(TreeTraversalReflector::EdgeType e) const
{
    Q_ASSERT((int) e >= 0 && (int)e <= 2);
    return (ModelRect*) &m_lRects[(int)e]; //FIXME use reference, not ptr
}

// Add more validation to detect possible invalid edges being set
void TreeTraversalReflectorPrivate::
setEdge(TreeTraversalReflector::EdgeType et, TreeTraversalBase* tti, Qt::Edge e)
{
    if (tti && et == TreeTraversalReflector::EdgeType::VISIBLE)
        Q_ASSERT(TTI(tti)->m_Geometry.isValid());

    edges(et)->setEdge(tti, e);
}

#include <treetraversalreflector_p.moc>
