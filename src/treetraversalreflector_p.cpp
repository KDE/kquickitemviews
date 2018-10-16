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
#include "adapters/abstractitemadapter_p.h"
#include "adapters/abstractitemadapter.h"
#include "viewport.h"
#include "viewport_p.h"

// Qt
#include <QQmlComponent>
#include <QtCore/QDebug>

using EdgeType = TreeTraversalReflector::EdgeType;

#include <treestructure_p.h>

/**
 * Hold the metadata associated with the QModelIndex.
 */
struct TreeTraversalItem : public TreeTraversalBase
{
    explicit TreeTraversalItem(TreeTraversalReflectorPrivate* d);

    enum class State {
        NEW       = 0, /*!< During creation, not part of the tree yet         */
        BUFFER    = 1, /*!< Not in the viewport, but close                    */
        REMOVED   = 2, /*!< Currently in a removal transaction                */
        REACHABLE = 3, /*!< The [grand]parent of visible indexes              */
        VISIBLE   = 4, /*!< The element is visible on screen                  */
        ERROR     = 5, /*!< Something went wrong                              */
        DANGLING  = 6, /*!< Being destroyed                                   */
    };

    typedef bool(TreeTraversalItem::*StateF)();

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

    TreeTraversalItem* loadUp  () const;
    TreeTraversalItem* loadDown() const;

    uint m_Depth {0};
    State m_State {State::BUFFER};

    // Managed by the Viewport
    bool m_IsCollapsed {false}; //TODO change the default to true

    BlockMetadata   m_Geometry;

    //TODO keep the absolute index of the GC circular buffer

    TreeTraversalReflectorPrivate* d_ptr;
};
#define TTI(i) static_cast<TreeTraversalItem*>(i)

/// Allows more efficient batching of actions related to consecutive items
class TreeTraversalItemRange //TODO
{
public:
    //TODO
    explicit TreeTraversalItemRange(TreeTraversalItem* topLeft, TreeTraversalItem* bottomRight);
    explicit TreeTraversalItemRange(
        Qt::Edge e, TreeTraversalReflector::EdgeType t, TreeTraversalItem* u
    );

    enum class Action {
        //TODO
    };

    bool apply();
};

/// Allow recycling of VisualTreeItem
class VisualTreeItemPool
{
    //TODO
};

class TreeTraversalReflectorPrivate : public QObject
{
    Q_OBJECT
public:

    // Helpers
    TreeTraversalItem* addChildren(TreeTraversalItem* parent, const QModelIndex& index);
    TreeTraversalItem* ttiForIndex(const QModelIndex& idx) const;

    bool isInsertActive(const QModelIndex& p, int first, int last) const;

    QList<TreeTraversalBase*> setTemporaryIndices(const QModelIndex &parent, int start, int end,
                             const QModelIndex &destination, int row);
    void resetTemporaryIndices(const QList<TreeTraversalBase*>&);

    TreeTraversalItem* m_pRoot {new TreeTraversalItem(this)};

    //TODO add a circular buffer to GC the items
    // * relative index: when to trigger GC
    // * absolute array index: kept in the TreeTraversalItem so it can
    //   remove itself

    /// All elements with loaded children
    QHash<QPersistentModelIndex, TreeTraversalItem*> m_hMapper;
    QAbstractItemModel* m_pModel {nullptr};
    QAbstractItemModel* m_pTrackedModel {nullptr};
    std::function<AbstractItemAdapter*()> m_fFactory;
    Viewport *m_pViewport;

    TreeTraversalReflector::TrackingState m_TrackingState {TreeTraversalReflector::TrackingState::NO_MODEL};

    ModelRect m_lRects[3];
    ModelRect* edges(TreeTraversalReflector::EdgeType e) const;

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
    void enterState(TreeTraversalItem*, TreeTraversalItem::State);
    void leaveState(TreeTraversalItem*, TreeTraversalItem::State);
    void error     (TreeTraversalItem*, TreeTraversalItem::State);

    typedef void(TreeTraversalReflectorPrivate::*StateF)();
    typedef void(TreeTraversalReflectorPrivate::*StateFS)(TreeTraversalItem*, TreeTraversalItem::State);
    static const TreeTraversalReflector::TrackingState m_fStateMap[4][7];
    static const StateF m_fStateMachine[4][7];
    static const StateFS m_fStateLogging[7][2];

    // Tests
    void _test_validateTree(TreeTraversalItem* p);
    void _test_validateViewport(bool skipVItemState = false);
    void _test_validateLinkedList(bool skipVItemState = false);
    void _test_validate_edges();

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

#define S TreeTraversalItem::State::
const TreeTraversalItem::State TreeTraversalItem::m_fStateMap[7][7] = {
/*                 SHOW         HIDE        ATTACH     DETACH      UPDATE       MOVE         RESET    */
/*NEW      */ { S ERROR  , S ERROR    , S REACHABLE, S ERROR    , S ERROR  , S ERROR    , S ERROR     },
/*BUFFER   */ { S VISIBLE, S BUFFER   , S BUFFER   , S DANGLING , S BUFFER , S BUFFER   , S BUFFER    },
/*REMOVED  */ { S ERROR  , S ERROR    , S ERROR    , S REACHABLE, S ERROR  , S ERROR    , S ERROR     },
/*REACHABLE*/ { S VISIBLE, S REACHABLE, S ERROR    , S REACHABLE, S ERROR  , S REACHABLE, S REACHABLE },
/*VISIBLE  */ { S VISIBLE, S BUFFER   , S ERROR    , S REACHABLE, S VISIBLE, S VISIBLE  , S VISIBLE   },
/*ERROR    */ { S ERROR  , S ERROR    , S ERROR    , S ERROR    , S ERROR  , S ERROR    , S ERROR     },
/*DANGLING */ { S ERROR  , S ERROR    , S ERROR    , S ERROR    , S ERROR  , S ERROR    , S ERROR     },
};
#undef S

#define A &TreeTraversalItem::
const TreeTraversalItem::StateF TreeTraversalItem::m_fStateMachine[7][7] = {
/*                 SHOW       HIDE      ATTACH     DETACH      UPDATE    MOVE     RESET  */
/*NEW      */ { A error  , A nothing, A nothing, A error   , A error  , A error, A error },
/*BUFFER   */ { A show   , A remove , A attach , A destroy , A refresh, A move , A reset },
/*REMOVED  */ { A error  , A error  , A error  , A detach  , A error  , A error, A reset },
/*REACHABLE*/ { A show   , A nothing, A error  , A detach  , A error  , A move , A reset },
/*VISIBLE  */ { A nothing, A hide   , A error  , A detach  , A refresh, A move , A reset },
/*ERROR    */ { A error  , A error  , A error  , A error   , A error  , A error, A error },
/*DANGLING */ { A error  , A error  , A error  , A error   , A error  , A error, A error },
};
#undef A

TreeTraversalItem::TreeTraversalItem(TreeTraversalReflectorPrivate* d):
  TreeTraversalBase(), d_ptr(d), m_Geometry(d->m_pViewport ? d->m_pViewport->d_ptr : nullptr)
{
    m_Geometry.m_pTTI = this;
}

TreeTraversalItem* TreeTraversalItem::loadUp() const
{
    Q_ASSERT(index().model() == d_ptr->m_pModel);

    if (auto tti = TTI(up()))
        return tti;

    //TODO load more than 1 at a time (probably std::max(remaining, buffer)
    if (auto row = effectiveRow()) {
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

TreeTraversalItem* TreeTraversalItem::loadDown() const
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

bool BlockMetadata::performAction(BlockMetadata::Action a)
{
    const int s     = (int)m_pTTI->m_State;
    m_pTTI->m_State = m_pTTI->m_fStateMap[s][(int)a];
    Q_ASSERT(m_pTTI->m_State != TreeTraversalItem::State::ERROR);

    if (s != (int)m_pTTI->m_State) {
        m_pTTI->d_ptr->leaveState(m_pTTI, (TreeTraversalItem::State)s);
        m_pTTI->d_ptr->enterState(m_pTTI, m_pTTI->m_State );
    }

    m_pTTI->m_Geometry.removeMe = (int) m_pTTI->m_State; //FIXME remove
    bool ret        = (this->m_pTTI->*TreeTraversalItem::m_fStateMachine[s][(int)a])();
    Q_ASSERT((!m_pTTI->m_Geometry.visualItem()) ||  m_pTTI->m_State == TreeTraversalItem::State::VISIBLE);

//     m_pTTI->d_ptr->_test_validate_edges();

    return ret;
}

TreeTraversalReflector::TrackingState TreeTraversalReflector::performAction(TreeTraversalReflector::TrackingAction a)
{
    const int s = (int)d_ptr->m_TrackingState;
    d_ptr->m_TrackingState = d_ptr->m_fStateMap[s][(int)a];

    (d_ptr->*TreeTraversalReflectorPrivate::m_fStateMachine[s][(int)a])();

    return d_ptr->m_TrackingState;
}

bool TreeTraversalItem::nothing()
{ return true; }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
bool TreeTraversalItem::error()
{
    Q_ASSERT(false);
    return false;
}
#pragma GCC diagnostic pop

bool TreeTraversalItem::show()
{
//     qDebug() << "SHOW";
    Q_ASSERT(m_State == State::VISIBLE);

    // Update the viewport first since the state is already to visible
//     auto upTTI = up();
//     if ((!upTTI) || upTTI->m_State != State::VISIBLE)
//         d_ptr->edges(EdgeType::VISIBLE)->setEdge(this, Qt::TopEdge);

//     auto downTTI = down();
//     if ((!downTTI) || downTTI->m_State != State::VISIBLE)
//         d_ptr->edges(EdgeType::VISIBLE)->setEdge(this, Qt::BottomEdge);

    if (auto item = m_Geometry.visualItem()) {
        //item->setVisible(true);
    }
    else {
        qDebug() << "CREATE" << this;
        m_Geometry.setVisualItem(d_ptr->q_ptr->d_ptr->m_fFactory()->s_ptr);
        Q_ASSERT(m_Geometry.visualItem());
        m_Geometry.visualItem()->m_pTTI = this;

        m_Geometry.visualItem()->performAction(VisualTreeItem::ViewAction::ATTACH);
        Q_ASSERT(m_Geometry.visualItem()->m_State == VisualTreeItem::State::POOLED);
        Q_ASSERT(m_State == State::VISIBLE);
    }

    d_ptr->_test_validateViewport(true);
    m_Geometry.visualItem()->performAction(VisualTreeItem::ViewAction::ENTER_BUFFER);
    Q_ASSERT(m_State == State::VISIBLE);
    d_ptr->_test_validateViewport(true);
    Q_ASSERT(m_Geometry.visualItem()->m_State == VisualTreeItem::State::BUFFER);

    d_ptr->_test_validateViewport(true);
    m_Geometry.visualItem()->performAction(VisualTreeItem::ViewAction::ENTER_VIEW);
    Q_ASSERT(m_State == State::VISIBLE);
    d_ptr->_test_validateViewport();

    // For some reason creating the visual element failed, this can and will
    // happen and need to be recovered from.
    if (m_Geometry.visualItem()->hasFailed()) {
        m_Geometry.visualItem()->performAction(VisualTreeItem::ViewAction::LEAVE_BUFFER);
        Q_ASSERT(m_State == State::VISIBLE);
        m_Geometry.setVisualItem(nullptr);
    }

    Q_ASSERT(m_State == State::VISIBLE);
    d_ptr->_test_validateViewport();

    return true;
}

bool TreeTraversalItem::hide()
{
    qDebug() << "HIDE";
    if (auto item = m_Geometry.visualItem()) {
        //item->setVisible(false);
        m_Geometry.visualItem()->performAction(VisualTreeItem::ViewAction::LEAVE_BUFFER);
    }

    return true;
}

bool TreeTraversalItem::remove()
{

    if (m_Geometry.visualItem()) {

        // If the item was active (due, for example, of a full reset), then
        // it has to be removed from view then deleted.
        if (m_Geometry.visualItem()->m_State == VisualTreeItem::State::ACTIVE) {
            m_Geometry.visualItem()->performAction(VisualTreeItem::ViewAction::DETACH);

            // It should still exists, it may crash otherwise, so make sure early
            Q_ASSERT(m_Geometry.visualItem()->m_State == VisualTreeItem::State::POOLED);

            m_Geometry.visualItem()->m_State = VisualTreeItem::State::POOLED;
            //FIXME ^^ add a new action for finish pooling or call
            // VisualTreeItem::detach from a new action method (instead of directly)
        }

        m_Geometry.visualItem()->performAction(VisualTreeItem::ViewAction::DETACH);
        m_Geometry.setVisualItem(nullptr);
        qDebug() << "\n\nREMOVE!!!" << this;
    }

//     qDebug() << "HIDE";
    return true;
}

bool TreeTraversalItem::attach()
{
    Q_ASSERT(m_State != State::VISIBLE);
    // Attach can only be called when there is room in the viewport //FIXME and the buffer
//     Q_ASSERT(d_ptr->edges(EdgeType::FREE)->m_Edges & (Qt::TopEdge|Qt::BottomEdge));

    d_ptr->m_pViewport->s_ptr->updateGeometry(&m_Geometry);

    Q_ASSERT(!m_Geometry.visualItem());

//     if (m_Geometry.visualItem())
//         m_Geometry.visualItem()->performAction(VisualTreeItem::ViewAction::ATTACH);

    const auto upTTI   = TTI(up());
    const auto downTTI = TTI(down());

    //TODO this ain't correct
    if ((!upTTI) || (upTTI->m_State == State::BUFFER && upTTI->m_Geometry.m_BufferEdge == Qt::TopEdge)) {
        upTTI->m_Geometry.m_BufferEdge == Qt::TopEdge;
        return m_Geometry.performAction(BlockMetadata::Action::SHOW);
    }
    else if ((!downTTI) || (downTTI->m_State == State::BUFFER && downTTI->m_Geometry.m_BufferEdge == Qt::BottomEdge)) {
        upTTI->m_Geometry.m_BufferEdge == Qt::BottomEdge;
        return m_Geometry.performAction(BlockMetadata::Action::SHOW);
    }
    else {
        return m_Geometry.performAction(BlockMetadata::Action::SHOW);
    }

    return true;
}

bool TreeTraversalItem::detach()
{
//     Q_ASSERT(false);

    const auto children = allLoadedChildren();

    // First, detach any remaining children
    for (auto i : qAsConst(children)) {
        auto tti = TTI(i);
        tti->m_Geometry.performAction(BlockMetadata::Action::DETACH);
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

    TreeTraversalItem::remove();
    TreeTraversalBase::remove();


    //d_ptr->_test_validateViewport(true);

//     d_ptr->_test_validateViewport();
//
    //FIXME set the parent firstChild() correctly and add an insert()/move() method
    // then drop bridgeGap

    Q_ASSERT(!m_Geometry.visualItem());
    qDebug() << "\n\nDETACHED!";

    return true;
}

bool TreeTraversalItem::refresh()
{
    //
//     qDebug() << "REFRESH";

    d_ptr->m_pViewport->s_ptr->updateGeometry(&m_Geometry);

    for (auto i = TTI(firstChild()); i; i = TTI(i->nextSibling())) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->m_Geometry.performAction(BlockMetadata::Action::UPDATE);

        if (i == lastChild())
            break;
    }

    Q_ASSERT(m_Geometry.visualItem()); //FIXME

    return true;
}

bool TreeTraversalItem::move()
{
    //TODO remove this implementation and use the one below

    // Propagate
    for (auto i = TTI(firstChild()); i; i = TTI(i->nextSibling())) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->m_Geometry.performAction(BlockMetadata::Action::MOVE);

        if (i == lastChild())
            break;
    }

    //FIXME this if should not exists, this should be handled by the state
    // machine.
    if (m_Geometry.visualItem()) {
        m_Geometry.visualItem()->performAction(VisualTreeItem::ViewAction::MOVE); //FIXME don't
    }
    //TODO update m_Geometry

    return true;

//     if (!m_Geometry.visualItem())
//         return true;

//FIXME this is better, but require having createItem() called earlier
//     if (m_Geometry.visualItem())
//         m_Geometry.visualItem()->performAction(VisualTreeItem::ViewAction::MOVE); //FIXME don't
//
//     if (oldGeo != m_Geometry.visualItem()->geometry())
//         if (auto n = m_Geometry.visualItem()->down())
//             n->parent()->performAction(BlockMetadata::Action::MOVE);

//     return true;
}

bool TreeTraversalItem::destroy()
{
    detach();

    m_Geometry.setVisualItem(nullptr);

    Q_ASSERT(!loadedChildrenCount());

    delete this;
    return true;
}

bool TreeTraversalItem::reset()
{
    for (auto i = TTI(firstChild()); i; i = TTI(i->nextSibling())) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->m_Geometry.performAction(BlockMetadata::Action::RESET);

        if (i == lastChild())
            break;
    }

    if (m_Geometry.visualItem()) {
        Q_ASSERT(this != d_ptr->m_pRoot);
        m_Geometry.visualItem()->performAction(VisualTreeItem::ViewAction::LEAVE_BUFFER);
        m_Geometry.visualItem()->performAction(VisualTreeItem::ViewAction::DETACH);
        m_Geometry.setVisualItem(nullptr);
    }

    m_Geometry.setSize({});
    m_Geometry.setPosition({});

    return true;
    /*return this == d_ptr->m_pRoot ?
        true : m_Geometry.performAction(BlockMetadata::Action::UPDATE);*/
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

    if (!isInsertActive(parent, first, last)) {
        _test_validateLinkedList();
        Q_ASSERT(false); //FIXME so I can't forget when time comes
        return;
    }

    auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;
    Q_ASSERT((!pitem->firstChild()) || pitem->lastChild());

    TreeTraversalItem *prev(nullptr);

    //FIXME use up()
    if (first && pitem)
        prev = TTI(pitem->childrenLookup(q_ptr->model()->index(first-1, 0, parent)));

    auto oldFirst = pitem->firstChild();

    // There is no choice here but to load a larger subset to avoid holes, in
    // theory, edges(EdgeType::FREE)->m_Edges will prevent runaway loading
    if (pitem->firstChild())
        last = std::max(last, pitem->firstChild()->effectiveRow() - 1);

    //FIXME support smaller ranges
    for (int i = first; i <= last; i++) {
        auto idx = q_ptr->model()->index(i, 0, parent);
        Q_ASSERT(idx.isValid());
        Q_ASSERT(idx.parent() != idx);
        Q_ASSERT(idx.model() == q_ptr->model());

        // If the insertion is sandwitched between loaded items, not doing it
        // will corrupt the view, but if it's a "tail" insertion, then they
        // can be discarded.
        if (((!prev) || !prev->down()) && !(edges(EdgeType::FREE)->m_Edges & Qt::BottomEdge)) {
            Q_ASSERT((!prev) || !prev->down());
            qDebug() << "\n\nNO SPACE LEFT";
            return; //FIXME break
        }

        auto e = addChildren(pitem, idx);

//         e->parent()->_test_validate_chain();

        if (pitem->firstChild() && pitem->firstChild()->effectiveRow() == idx.row()+1) {
            Q_ASSERT(idx.parent() == pitem->firstChild()->effectiveParentIndex());

            TreeTraversalBase::insertChildBefore(e, pitem->firstChild(), pitem);
        }
        else
            TreeTraversalBase::insertChildAfter(e, prev, pitem); //FIXME incorrect

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
            TreeTraversalBase::insertChildBefore(e, nullptr, pitem);
        }
        e->parent()->_test_validate_chain();

        // Do this before ATTACH to make sure the daisy-chaining is valid in
        // case it is used.
//         if (needDownMove) {
//             Q_ASSERT(pitem != e);
//             pitem->m_tChildren[LAST] = e;
//             Q_ASSERT(!e->nextSibling());
//         }

        e->parent()->_test_validate_chain();

        // NEW -> REACHABLE, this should never fail
        if (!e->m_Geometry.performAction(BlockMetadata::Action::ATTACH)) {
            _test_validateLinkedList();
            qDebug() << "\n\nATTACH FAILED";
            break;
        }

        if (needUpMove) {
            Q_ASSERT(pitem != e);
            if (auto pe = TTI(e->up()))
                pe->m_Geometry.performAction(BlockMetadata::Action::MOVE);
        }

        Q_ASSERT((!pitem->lastChild()) || !pitem->lastChild()->hasTemporaryIndex()); //TODO

        if (needDownMove) {
            if (auto ne = TTI(e->down()))
                ne->m_Geometry.performAction(BlockMetadata::Action::MOVE);
        }

        int rc = q_ptr->model()->rowCount(idx);
        if (rc && edges(EdgeType::FREE)->m_Edges & Qt::BottomEdge) {
            qDebug() << "\n\nRECURSE!" << rc;
            slotRowsInserted(idx, 0, rc-1);
            qDebug() << "\n\nDONE!" << rc;
        }

        // Validate early to prevent propagating garbage that's nearly impossible
        // to debug.
        if (pitem && pitem != m_pRoot && !i) {
            Q_ASSERT(e->up() == pitem);
            Q_ASSERT(e == pitem->down());
        }

        if (first == last)
            _test_validateLinkedList();

        prev = e;

    }

    _test_validateLinkedList();

    // Fix the linked list
//     if (oldFirst && prev && prev->effectiveRow() < oldFirst->effectiveRow()) {
//         Q_ASSERT(!oldFirst->previousSibling());
//         Q_ASSERT(oldFirst->effectiveRow() == prev->effectiveRow() + 1);
//         Q_ASSERT(prev->nextSibling() ==oldFirst);
//
//         oldFirst->previousSibling() = prev;
//     }

//     if ((!pitem->lastChild()) || last > pitem->lastChild()->effectiveRow()) {
//         pitem->m_tChildren[LAST] = prev;
//         Q_ASSERT((!prev) || !prev->nextSibling());
//     }

    Q_ASSERT(pitem->lastChild());

    pitem->_test_validate_chain();
    _test_validateLinkedList();


    //FIXME use down()
//     if (q_ptr->model()->rowCount(parent) > last) {
//         //detachUntil;
//         if (!prev) {
//
//         }
//         else if (auto i = pitem->childrenLookup(q_ptr->model()->index(last+1, 0, parent))) {
//             i->m_tSiblings[PREVIOUS] = prev;
//             prev->m_tSiblings[NEXT] = i;
//         }
// //         else //FIXME it happens
// //             Q_ASSERT(false);
//     }

    _test_validateLinkedList();

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

    //TODO make sure the state machine support them
    //TreeTraversalItem *prev(nullptr), *next(nullptr);

    //FIXME use up()
    //if (first && pitem)
    //    prev = pitem->m_hLookup.value(model()->index(first-1, 0, parent));

    //next = pitem->m_hLookup.value(model()->index(last+1, 0, parent));

    //FIXME support smaller ranges
    for (int i = first; i <= last; i++) {
        auto idx = q_ptr->model()->index(i, 0, parent);

        if (auto elem = TTI(pitem->childrenLookup(idx))) {
            elem->m_Geometry.performAction(BlockMetadata::Action::DETACH);
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
    // the edges is necessary for the TreeTraversalItem. Each VisualTreeItem
    // need to be moved.

    const auto idxStart = q_ptr->model()->index(start, 0, parent);
    const auto idxEnd   = q_ptr->model()->index(end  , 0, parent);
    Q_ASSERT(idxStart.isValid() && idxEnd.isValid());

    //FIXME once partial ranges are supported, this is no longer always valid
    auto startTTI = ttiForIndex(idxStart);
    auto endTTI   = ttiForIndex(idxEnd);

    if (end - start == 1)
        Q_ASSERT(startTTI->nextSibling() ==endTTI);

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

    TreeTraversalItem *newNextTTI(nullptr), *newPrevTTI(nullptr);

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

    Q_ASSERT((newPrevTTI || startTTI) && newPrevTTI != startTTI);
    Q_ASSERT((newNextTTI || endTTI  ) && newNextTTI != endTTI  );

    TreeTraversalItem* newParentTTI = ttiForIndex(destination);
    Q_ASSERT(newParentTTI || !destination.isValid()); //TODO not coded yet

    newParentTTI = newParentTTI ? newParentTTI : m_pRoot;
    auto oldParentTTI = startTTI->parent();

    // Make sure not to leave invalid pointers while the steps below are being performed
//     TreeTraversalBase::createGap(startTTI, endTTI);

    // Remove everything //TODO add batching again
    TreeTraversalBase* tti = endTTI;
    TreeTraversalBase* cur = nullptr;
    TreeTraversalBase* dest = newNextTTI && newNextTTI->parent() == newParentTTI ?
        newNextTTI : nullptr;

    int i = -1; //DEBUG

    do {
        TreeTraversalBase* cur = tti;
        tti = tti->previousSibling();
        Q_ASSERT(cur != tti);
        Q_ASSERT(cur->parent());
        cur->TreeTraversalBase::remove();
        Q_ASSERT(cur->m_LifeCycleState == TreeTraversalBase::LifeCycleState::NEW);

        TreeTraversalBase::insertChildBefore(cur, dest, newParentTTI);
        dest = cur;

        i++; //DEBUG

        if (cur == startTTI) {
            qDebug() << "\n\nBREAK!" << tti << startTTI << cur;
            break; //TODO remove, debug only
        }
    } while(tti || cur != startTTI);

    Q_ASSERT( (end - start) == i);

    // Update the tree parent (if necessary)
    Q_ASSERT(startTTI->parent() == newParentTTI);
    Q_ASSERT(endTTI->parent()   == newParentTTI);

    //BEGIN debug
    if (newPrevTTI && newPrevTTI->parent())
        newPrevTTI->parent()->_test_validate_chain();
    if (startTTI && startTTI->parent())
        startTTI->parent()->_test_validate_chain();
    if (endTTI && endTTI->parent())
        endTTI->parent()->_test_validate_chain();
    if (newNextTTI && newNextTTI->parent())
        newNextTTI->parent()->_test_validate_chain();
    //END debug

    if (endTTI->nextSibling()) {
        Q_ASSERT(endTTI->nextSibling()->previousSibling() ==endTTI);
    }

    if (startTTI->previousSibling()) {
        Q_ASSERT(startTTI->previousSibling()->parent() == startTTI->parent());
        Q_ASSERT(startTTI->previousSibling()->nextSibling() ==startTTI);
    }

    Q_ASSERT(newParentTTI->firstChild());
    /*Q_ASSERT(startTTI == newParentTTI->firstChild() ||
        newParentTTI->firstChild()->effectiveRow() <= startTTI->m_MoveToRow);*/
    Q_ASSERT(row || newParentTTI->firstChild() ==startTTI);

    // Move everything
    //TODO move it more efficient
    m_pRoot->m_Geometry.performAction(BlockMetadata::Action::MOVE);

    resetTemporaryIndices(tmp);
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
    m_pRoot->m_Geometry.performAction(BlockMetadata::Action::RESET);

    for (int i = 0; i < 3; i++)
        m_lRects[i] = {};

    m_hMapper.clear();
    delete m_pRoot;
    m_pRoot = new TreeTraversalItem(this);
}

void TreeTraversalReflectorPrivate::reset()
{
    m_pRoot->m_Geometry.performAction(BlockMetadata::Action::RESET);
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


    qDebug() << "\n\nPOPULATE!" << edges(EdgeType::FREE)->m_Edges;
    if (m_pRoot->firstChild() && (edges(EdgeType::FREE)->m_Edges & (Qt::TopEdge|Qt::BottomEdge))) {
        //Q_ASSERT(edges(EdgeType::FREE)->getEdge(Qt::TopEdge));

        while (edges(EdgeType::FREE)->m_Edges & Qt::TopEdge) {
            const auto was = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge));

            auto u = was->loadUp();

            Q_ASSERT(u || edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge)->effectiveRow() == 0);
            Q_ASSERT(u || !edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge)->effectiveParentIndex().isValid());

            if (!u)
                break;

            u->m_Geometry.performAction(BlockMetadata::Action::SHOW);
        }

        while (edges(EdgeType::FREE)->m_Edges & Qt::BottomEdge) {
            const auto was = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge));

            auto u = was->loadDown();

            //Q_ASSERT(u || edges(EdgeType::FREE)->getEdge(Qt::BottomEdge)->effectiveRow() == 0);
            //Q_ASSERT(u || !edges(EdgeType::FREE)->getEdge(Qt::BottomEdge)->effectiveParentIndex().isValid());

            if (!u)
                break;

            u->m_Geometry.performAction(BlockMetadata::Action::SHOW);
        }
    }
    else if (auto rc = m_pModel->rowCount()) {
        //FIX support anchors
        slotRowsInserted({}, 0, rc - 1);
    }
}

void TreeTraversalReflectorPrivate::trim()
{
    qDebug() << "TRIM" << edges(EdgeType::VISIBLE)->m_Edges;
    if (!edges(EdgeType::FREE)->getEdge(Qt::TopEdge)) {
        Q_ASSERT(!edges(EdgeType::FREE)->getEdge(Qt::BottomEdge));
        return;
    }

    while (!(edges(EdgeType::VISIBLE)->m_Edges & Qt::TopEdge)) {
        Q_ASSERT(edges(EdgeType::FREE)->getEdge(Qt::TopEdge));
        Q_ASSERT(!m_pViewport->currentRect().intersects(TTI(edges(EdgeType::FREE)->getEdge(Qt::TopEdge))->m_Geometry.geometry()));
//         Q_ASSERT(false);
        TTI(edges(EdgeType::FREE)->getEdge(Qt::TopEdge))->m_Geometry.performAction(BlockMetadata::Action::HIDE);
    }

    auto elem = TTI(edges(EdgeType::FREE)->getEdge(Qt::BottomEdge)); //FIXME have a visual and reacheable rect
    while (!(edges(EdgeType::VISIBLE)->m_Edges & Qt::BottomEdge)) {
        Q_ASSERT(elem);
        Q_ASSERT(!m_pViewport->currentRect().intersects(elem->m_Geometry.geometry()));
//         Q_ASSERT(false);
        elem->m_Geometry.performAction(BlockMetadata::Action::HIDE);
        elem = TTI(elem->up());
    }
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

void TreeTraversalReflectorPrivate::enterState(TreeTraversalItem* tti, TreeTraversalItem::State s)
{
    qDebug() << "ENTER STATE!!";
    if (s == TreeTraversalItem::State::VISIBLE) {
        auto first = edges(TreeTraversalReflector::EdgeType::VISIBLE)->getEdge(Qt::TopEdge);
        auto prev = tti->up();
        auto next = tti->down();
        auto last = edges(TreeTraversalReflector::EdgeType::VISIBLE)->getEdge(Qt::BottomEdge);
        qDebug() << "VISIBLE!" << first << prev << tti << next << last;
        qDebug() << first << tti;
        qDebug() << last << tti;

        if (first == next) {
            edges(TreeTraversalReflector::EdgeType::VISIBLE)->setEdge(tti, Qt::TopEdge);
        }

        if (prev == last) {
            edges(TreeTraversalReflector::EdgeType::VISIBLE)->setEdge(tti, Qt::BottomEdge);
        }

        Q_ASSERT(tti != first);
        Q_ASSERT(tti != last);
    }

//     _test_validate_edges();
}

void TreeTraversalReflectorPrivate::leaveState(TreeTraversalItem *tti, TreeTraversalItem::State s)
{
    qDebug() << "LEAVE STATE!!";

    if (s == TreeTraversalItem::State::VISIBLE) {
        auto first = edges(TreeTraversalReflector::EdgeType::VISIBLE)->getEdge(Qt::TopEdge);
        auto last = edges(TreeTraversalReflector::EdgeType::VISIBLE)->getEdge(Qt::BottomEdge);

        // The item was somewhere in between, not an edge case
        if ((tti != first) && (tti != last))
            return;

        auto prev = TTI(tti->up());
        auto next = TTI(tti->down());

        if (tti == first)
            edges(TreeTraversalReflector::EdgeType::VISIBLE)->setEdge(
                (next && next->m_State == TreeTraversalItem::State::VISIBLE) ?
                    next : nullptr,
                Qt::TopEdge
            );

        if (tti == last)
            edges(TreeTraversalReflector::EdgeType::VISIBLE)->setEdge(
                (prev && prev->m_State == TreeTraversalItem::State::VISIBLE) ?
                    prev : nullptr,
                Qt::BottomEdge
            );
    }
}

void TreeTraversalReflectorPrivate::error(TreeTraversalItem*, TreeTraversalItem::State)
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

bool TreeTraversalReflectorPrivate::isInsertActive(const QModelIndex& p, int first, int last) const
{
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
    return true; //FIXME
}

/// Add new entries to the mapping
TreeTraversalItem* TreeTraversalReflectorPrivate::addChildren(TreeTraversalItem* parent, const QModelIndex& index)
{
    Q_ASSERT(index.isValid());
    Q_ASSERT(index.parent() != index);

    auto e = new TreeTraversalItem(this);
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

    m_pRoot->m_Geometry.performAction(BlockMetadata::Action::DETACH);

    m_hMapper.clear();
    m_pRoot = new TreeTraversalItem(this);
    //m_FailedCount = 0;
}

TreeTraversalItem* TreeTraversalReflectorPrivate::ttiForIndex(const QModelIndex& idx) const
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
    return tti && tti->m_Geometry.visualItem() ? tti->m_Geometry.visualItem()->d_ptr : nullptr;
}

void TreeTraversalReflector::setAvailableEdges(Qt::Edges edges, TreeTraversalReflector::EdgeType t)
{
    d_ptr->edges(t)->m_Edges = edges;
}

Qt::Edges TreeTraversalReflector::availableEdges(TreeTraversalReflector::EdgeType  t) const
{
    return d_ptr->edges(t)->m_Edges;
}

QModelIndex BlockMetadata::index() const
{
    return QModelIndex(m_pTTI->index());
}

QPersistentModelIndex VisualTreeItem::index() const
{
    return QModelIndex(m_pTTI->index());
}

/**
 * Flatten the tree as a linked list.
 *
 * Returns the previous non-failed item.
 */
VisualTreeItem* VisualTreeItem::up(AbstractItemAdapter::StateFlags flags) const
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
    while (ret && !ret->m_Geometry.visualItem())
        ret = TTI(ret->up());

    if (ret && ret->m_Geometry.visualItem())
        Q_ASSERT(ret->m_Geometry.visualItem()->m_State != State::POOLING && ret->m_Geometry.visualItem()->m_State != State::DANGLING);

    auto vi = ret ? ret->m_Geometry.visualItem() : nullptr;

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
VisualTreeItem* VisualTreeItem::down(AbstractItemAdapter::StateFlags flags) const
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
    while (ret && !ret->m_Geometry.visualItem())
        ret = TTI(ret->down());

    auto vi = ret ? ret->m_Geometry.visualItem() : nullptr;

    // Do not allow navigating past the loaded edges
    if (vi && (vi->m_State == State::POOLING || vi->m_State == State::POOLED))
        return nullptr;

    return vi;
}

int VisualTreeItem::row() const
{
    return m_pTTI->effectiveRow();
}

int VisualTreeItem::column() const
{
    return m_pTTI->effectiveColumn();
}

BlockMetadata *BlockMetadata::up() const
{
    auto i = TTI(m_pTTI->up());
    return i ? &i->m_Geometry : nullptr;
}

BlockMetadata *BlockMetadata::down() const
{
    auto i = TTI(m_pTTI->down());
    return i ? &i->m_Geometry : nullptr;
}

BlockMetadata *BlockMetadata::left() const
{
    auto i = TTI(m_pTTI->left());
    return i ? &i->m_Geometry : nullptr;
}

BlockMetadata *BlockMetadata::right() const
{
    auto i = TTI(m_pTTI->right());
    return i ? &i->m_Geometry : nullptr;
}

BlockMetadata *TreeTraversalReflector::getEdge(EdgeType t, Qt::Edge e) const
{
    auto ret = TTI(d_ptr->edges(t)->getEdge(e));
    return ret ? &ret->m_Geometry : nullptr;
}

void VisualTreeItem::setCollapsed(bool v)
{
    m_pTTI->m_IsCollapsed = v;
}

bool VisualTreeItem::isCollapsed() const
{
    return m_pTTI->m_IsCollapsed;
}

ModelRect* TreeTraversalReflectorPrivate::edges(TreeTraversalReflector::EdgeType e) const
{
    Q_ASSERT((int) e >= 0 && (int)e <= 2);
    return (ModelRect*) &m_lRects[(int)e]; //FIXME use reference, not ptr
}

#include <treetraversalreflector_p.moc>
