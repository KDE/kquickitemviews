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

// Use some constant for readability
#define PREVIOUS 0
#define NEXT 1
#define FIRST 0
#define LAST 1
/**
 * Hold the QPersistentModelIndex and the metadata associated with them.
 */
struct TreeTraversalItems
{
    explicit TreeTraversalItems(TreeTraversalItems* parent, TreeTraversalReflectorPrivate* d):
        m_pParent(parent), d_ptr(d) {
            m_Geometry.m_pTTI = this;
        }

    enum class State {
        BUFFER    = 0, /*!< Not in use by any visible indexes, but pre-loaded */
        REMOVED   = 1, /*!< Currently in a removal transaction                */
        REACHABLE = 2, /*!< The [grand]parent of visible indexes              */
        VISIBLE   = 3, /*!< The element is visible on screen                  */
        ERROR     = 4, /*!< Something went wrong                              */
        DANGLING  = 5, /*!< Being destroyed                                   */
    };

    typedef bool(TreeTraversalItems::*StateF)();

    static const State  m_fStateMap    [6][7];
    static const StateF m_fStateMachine[6][7];

    bool nothing();
    bool error  ();
    bool show   ();
    bool hide   ();
    bool attach ();
    bool detach ();
    bool refresh();
    bool index  ();
    bool destroy();
    bool reset  ();

    // Geometric navigation
    TreeTraversalItems* up   () const;
    TreeTraversalItems* down () const;
    TreeTraversalItems* left () const;
    TreeTraversalItems* right() const;

    TreeTraversalItems* loadUp  () const;
    TreeTraversalItems* loadDown() const;

    //TODO use a btree, not an hash
    QHash<QPersistentModelIndex, TreeTraversalItems*> m_hLookup;

    uint m_Depth {0};
    State m_State {State::BUFFER};

    // Keep the parent to be able to get back to the root
    TreeTraversalItems* m_pParent;

    TreeTraversalItems* m_tSiblings[2] = {nullptr, nullptr};
    TreeTraversalItems* m_tChildren[2] = {nullptr, nullptr};

    // Because slotRowsMoved is called before the change take effect, cache
    // the "new real row and column" since the current index()->row() is now
    // garbage.
    int m_MoveToRow    {-1};
    int m_MoveToColumn {-1};

    // Managed by the Viewport
    bool m_IsCollapsed {false}; //TODO change the default to true

    QPersistentModelIndex m_Index;
    VisualTreeItem* m_pTreeItem {nullptr};
    BlockMetadata   m_Geometry;

    //TODO keep the absolute index of the GC circular buffer

    TreeTraversalReflectorPrivate* d_ptr;
};

class TreeTraversalReflectorPrivate : public QObject
{
    Q_OBJECT
public:

    // Helpers
    TreeTraversalItems* addChildren(TreeTraversalItems* parent, const QModelIndex& index);
    void bridgeGap(TreeTraversalItems* first, TreeTraversalItems* second, bool insert = false);
    void createGap(TreeTraversalItems* first, TreeTraversalItems* last  );
    TreeTraversalItems* ttiForIndex(const QModelIndex& idx) const;

    bool isInsertActive(const QModelIndex& p, int first, int last) const;

    void setTemporaryIndices(const QModelIndex &parent, int start, int end,
                             const QModelIndex &destination, int row);
    void resetTemporaryIndices(const QModelIndex &parent, int start, int end,
                               const QModelIndex &destination, int row);

    TreeTraversalItems* m_pRoot {new TreeTraversalItems(nullptr, this)};

    //TODO add a circular buffer to GC the items
    // * relative index: when to trigger GC
    // * absolute array index: kept in the TreeTraversalItems so it can
    //   remove itself

    /// All elements with loaded children
    QHash<QPersistentModelIndex, TreeTraversalItems*> m_hMapper;
    QAbstractItemModel* m_pModel {nullptr};
    QAbstractItemModel* m_pTrackedModel {nullptr};
    std::function<AbstractItemAdapter*()> m_fFactory;
    Viewport *m_pViewport;
    Qt::Edges m_Edges {Qt::TopEdge|Qt::LeftEdge|Qt::RightEdge|Qt::BottomEdge};
    TreeTraversalReflector::TrackingState m_TrackingState {TreeTraversalReflector::TrackingState::NO_MODEL};

    enum Pos {Top, Left, Right, Bottom};

    TreeTraversalItems *m_lpEdges[Pos::Bottom+1];
    int m_lEffectiveBuffer[Pos::Bottom+1] {0,0,0,0};

    TreeTraversalReflector* q_ptr;

    // Actions, do not call directly
    void track();
    void untrack();
    void nothing();
    void reset();
    void free();
    void error();
    void populate();

    typedef void(TreeTraversalReflectorPrivate::*StateF)();
    static const TreeTraversalReflector::TrackingState m_fStateMap[4][6];
    static const StateF m_fStateMachine[4][6];

    // Tests
    void _test_validateTree(TreeTraversalItems* p);
    void _test_validateViewport(bool skipVItemState = false);

public Q_SLOTS:
    void cleanup();
    void slotRowsInserted  (const QModelIndex& parent, int first, int last);
    void slotRowsRemoved   (const QModelIndex& parent, int first, int last);
    void slotLayoutChanged (                                              );
    void slotRowsMoved     (const QModelIndex &p, int start, int end,
                            const QModelIndex &dest, int row);
    void slotRowsMoved2    (const QModelIndex &p, int start, int end,
                            const QModelIndex &dest, int row);
};

#include <treetraversalreflector_debug.h>

#define S TreeTraversalReflector::TrackingState::
const TreeTraversalReflector::TrackingState TreeTraversalReflectorPrivate::m_fStateMap[4][6] = {
/*                POPULATE     DISABLE      ENABLE       RESET        FREE         MOVE   */
/*NO_MODEL */ { S NO_MODEL , S NO_MODEL , S NO_MODEL, S NO_MODEL, S NO_MODEL , S NO_MODEL },
/*PAUSED   */ { S POPULATED, S PAUSED   , S TRACKING, S PAUSED  , S PAUSED   , S PAUSED   },
/*POPULATED*/ { S TRACKING , S PAUSED   , S TRACKING, S PAUSED  , S POPULATED, S POPULATED},
/*TRACKING */ { S TRACKING , S POPULATED, S TRACKING, S TRACKING, S TRACKING , S TRACKING },
};
#undef S

// This state machine is self healing, error can be called in release mode
// and it will only disable the view without further drama.
#define A &TreeTraversalReflectorPrivate::
const TreeTraversalReflectorPrivate::StateF TreeTraversalReflectorPrivate::m_fStateMachine[4][6] = {
/*               POPULATE     DISABLE    ENABLE     RESET       FREE       MOVE   */
/*NO_MODEL */ { A nothing , A nothing, A nothing, A nothing, A nothing, A nothing },
/*PAUSED   */ { A populate, A nothing, A error  , A  reset , A  free  , A nothing },
/*POPULATED*/ { A error   , A nothing, A track  , A  reset , A  free  , A populate},
/*TRACKING */ { A nothing , A untrack, A nothing, A  reset , A  free  , A populate},
};
#undef A

#define S TreeTraversalItems::State::
const TreeTraversalItems::State TreeTraversalItems::m_fStateMap[6][7] = {
/*                 SHOW         HIDE        ATTACH     DETACH      UPDATE       MOVE         RESET    */
/*BUFFER   */ { S ERROR  , S BUFFER   , S REACHABLE, S DANGLING , S BUFFER , S BUFFER   , S BUFFER    },
/*REMOVED  */ { S ERROR  , S ERROR    , S ERROR    , S BUFFER   , S ERROR  , S ERROR    , S ERROR     },
/*REACHABLE*/ { S VISIBLE, S REACHABLE, S ERROR    , S BUFFER   , S ERROR  , S REACHABLE, S REACHABLE },
/*VISIBLE  */ { S VISIBLE, S REACHABLE, S ERROR    , S BUFFER   , S VISIBLE, S VISIBLE  , S VISIBLE   },
/*ERROR    */ { S ERROR  , S ERROR    , S ERROR    , S ERROR    , S ERROR  , S ERROR    , S ERROR     },
/*DANGLING */ { S ERROR  , S ERROR    , S ERROR    , S ERROR    , S ERROR  , S ERROR    , S ERROR     },
};
#undef S

#define A &TreeTraversalItems::
const TreeTraversalItems::StateF TreeTraversalItems::m_fStateMachine[6][7] = {
/*                 SHOW       HIDE      ATTACH    DETACH      UPDATE    MOVE     RESET  */
/*BUFFER   */ { A error  , A nothing, A attach, A destroy , A refresh, A index, A reset },
/*REMOVED  */ { A error  , A error  , A error , A detach  , A error  , A error, A reset },
/*REACHABLE*/ { A show   , A nothing, A error , A detach  , A error  , A index, A reset },
/*VISIBLE  */ { A nothing, A hide   , A error , A detach  , A refresh, A index, A reset },
/*ERROR    */ { A error  , A error  , A error , A error   , A error  , A error, A error },
/*DANGLING */ { A error  , A error  , A error , A error   , A error  , A error, A error },
};
#undef A

TreeTraversalItems* TreeTraversalItems::up() const
{
    TreeTraversalItems* ret = nullptr;

    // Another simple case, there is no parent
    if (!m_pParent) {
        Q_ASSERT(!m_Index.parent().isValid()); //TODO remove, no longer true when partial loading is implemented

        return nullptr;
    }

    // The parent is the element above in a Cartesian plan
    if (d_ptr->m_pRoot != m_pParent && !m_tSiblings[PREVIOUS])
        return m_pParent;

    ret = m_tSiblings[PREVIOUS];

    while (ret && ret->m_tChildren[LAST])
        ret = ret->m_tChildren[LAST];

    return ret;
}

TreeTraversalItems* TreeTraversalItems::down() const
{
    TreeTraversalItems* ret = nullptr;
    auto i = this;

    if (m_tChildren[FIRST]) {
        //Q_ASSERT(m_pParent->m_tChildren[FIRST]->m_Index.row() == 0); //racy
        ret = m_tChildren[FIRST];
        return ret;
    }


    // Recursively unwrap the tree until an element is found
    while(i) {
        if (i->m_tSiblings[NEXT] && (ret = i->m_tSiblings[NEXT]))
            return ret;

        i = i->m_pParent;
    }

    // Can't happen, exists to detect corrupted code
    if (m_Index.parent().isValid()) {
        Q_ASSERT(m_pParent);
//         Q_ASSERT(m_pParent->m_pParent->m_pParent->m_hLookup.size()
//             == m_pParent->m_Index.parent().row()+1);
    }

    return ret;
}

TreeTraversalItems* TreeTraversalItems::left() const
{
    return nullptr; //TODO
}

TreeTraversalItems* TreeTraversalItems::right() const
{
    return nullptr; //TODO
}


TreeTraversalItems* TreeTraversalItems::loadUp() const
{
    Q_ASSERT(m_Index.model() == d_ptr->m_pModel);

    if (auto tti = up())
        return tti;

    //TODO load more than 1 at a time (probably std::max(remaining, buffer)
    if (auto row = m_Index.row()) {
        d_ptr->slotRowsInserted(m_Index.parent(), row-1, row-1);
        d_ptr->_test_validateViewport();

        auto tti = up();
        Q_ASSERT(tti);

        return tti;
    }

    // In theory, the parent is always loaded, so there is no other case
    return m_pParent == d_ptr->m_pRoot ? nullptr : m_pParent;
}

TreeTraversalItems* TreeTraversalItems::loadDown() const
{
    Q_ASSERT(m_Index.model() == d_ptr->m_pModel);

    if (auto tti = down())
        return tti;

    // Check if there is children
    if (int childrenCount = d_ptr->m_pModel->rowCount(m_Index)) {
        d_ptr->slotRowsInserted(m_Index, 0, 0);
        d_ptr->_test_validateViewport();

        auto tti = down();
        Q_ASSERT(tti);

        return tti;
    }

    //TODO load more than 1 at a time (probably std::max(remaining, buffer)
    volatile const int rc = d_ptr->m_pModel->rowCount(m_Index.parent());
    if (m_Index.row() < rc-1) {
        d_ptr->slotRowsInserted(m_Index.parent(), m_Index.row()+1, m_Index.row()+1);
        d_ptr->_test_validateViewport();
        auto tti = down();
        Q_ASSERT(tti);

        return tti;
    }

    // Rewind until a (grand)parent with more to load is found
//     while(i) {
//         if (i->m_tSiblings[NEXT] && (ret = i->m_tSiblings[NEXT]))
//             return ret;
//
//         i = i->m_pParent;
//     }

    d_ptr->_test_validateViewport();
    Q_ASSERT(false); //TODO
    return nullptr;
}

bool BlockMetadata::performAction(BlockMetadata::Action a)
{
    const int s     = (int)m_pTTI->m_State;
    m_pTTI->m_State = m_pTTI->m_fStateMap[s][(int)a];
    bool ret        = (this->m_pTTI->*TreeTraversalItems::m_fStateMachine[s][(int)a])();

    return ret;
}

TreeTraversalReflector::TrackingState TreeTraversalReflector::performAction(TreeTraversalReflector::TrackingAction a)
{
    const int s = (int)d_ptr->m_TrackingState;
    d_ptr->m_TrackingState = d_ptr->m_fStateMap[s][(int)a];

    (d_ptr->*TreeTraversalReflectorPrivate::m_fStateMachine[s][(int)a])();

    return d_ptr->m_TrackingState;
}

bool TreeTraversalItems::nothing()
{ return true; }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
bool TreeTraversalItems::error()
{
    Q_ASSERT(false);
    return false;
}
#pragma GCC diagnostic pop

bool TreeTraversalItems::show()
{
//     qDebug() << "SHOW";
    Q_ASSERT(m_State == State::VISIBLE);

    // Update the viewport first since the state is already to visible
    auto upTTI = up();
    if ((!upTTI) || upTTI->m_State != State::VISIBLE)
        d_ptr->m_lpEdges[TreeTraversalReflectorPrivate::Pos::Top] = this;

    auto downTTI = down();
    if ((!downTTI) || downTTI->m_State != State::VISIBLE)
        d_ptr->m_lpEdges[TreeTraversalReflectorPrivate::Pos::Bottom] = this;

    if (!m_pTreeItem) {
        m_pTreeItem = d_ptr->q_ptr->d_ptr->m_fFactory()->s_ptr;
        m_pTreeItem->m_pGeometry = &m_Geometry;
        m_Geometry.m_pItem = m_pTreeItem;
        Q_ASSERT(m_pTreeItem);
        m_pTreeItem->m_pTTI = this;

        m_pTreeItem->performAction(VisualTreeItem::ViewAction::ATTACH);
        Q_ASSERT(m_pTreeItem->m_State == VisualTreeItem::State::POOLED);
        Q_ASSERT(m_State == State::VISIBLE);
    }

    d_ptr->_test_validateViewport(true);
    m_pTreeItem->performAction(VisualTreeItem::ViewAction::ENTER_BUFFER);
    Q_ASSERT(m_State == State::VISIBLE);
    d_ptr->_test_validateViewport(true);
    Q_ASSERT(m_pTreeItem->m_State == VisualTreeItem::State::BUFFER);

    d_ptr->_test_validateViewport(true);
    m_pTreeItem->performAction(VisualTreeItem::ViewAction::ENTER_VIEW);
    Q_ASSERT(m_State == State::VISIBLE);
    d_ptr->_test_validateViewport();

    // For some reason creating the visual element failed, this can and will
    // happen and need to be recovered from.
    if (m_pTreeItem->hasFailed()) {
        m_pTreeItem->performAction(VisualTreeItem::ViewAction::LEAVE_BUFFER);
        Q_ASSERT(m_State == State::VISIBLE);
        m_pTreeItem = nullptr;
    }

    Q_ASSERT(m_State == State::VISIBLE);
    d_ptr->_test_validateViewport();

    return true;
}

bool TreeTraversalItems::hide()
{
//     qDebug() << "HIDE";
    return true;
}

bool TreeTraversalItems::attach()
{
    Q_ASSERT(m_State != State::VISIBLE);
    // Attach can only be called when there is room in the viewport //FIXME and the buffer
    Q_ASSERT(d_ptr->m_Edges & (Qt::TopEdge|Qt::BottomEdge));

    d_ptr->m_pViewport->s_ptr->updateGeometry(&m_Geometry);

    Q_ASSERT(!m_pTreeItem);

//     if (m_pTreeItem)
//         m_pTreeItem->performAction(VisualTreeItem::ViewAction::ATTACH);

    const auto upTTI   = up();
    const auto downTTI = down();

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

bool TreeTraversalItems::detach()
{
    using Pos = TreeTraversalReflectorPrivate::Pos;

    // First, detach any remaining children
    auto i = m_hLookup.begin();
    while ((i = m_hLookup.begin()) != m_hLookup.end())
        i.value()->m_Geometry.performAction(BlockMetadata::Action::DETACH);
    Q_ASSERT(m_hLookup.isEmpty());

    // Update the viewport
    if (d_ptr->m_lpEdges[Pos::Top] == this)
        d_ptr->m_lpEdges[Pos::Top] = up();

    if (d_ptr->m_lpEdges[Pos::Bottom] == this)
        d_ptr->m_lpEdges[Pos::Bottom] = down();

    d_ptr->_test_validateViewport();

    if (m_pTreeItem) {

        // If the item was active (due, for example, of a full reset), then
        // it has to be removed from view then deleted.
        if (m_pTreeItem->m_State == VisualTreeItem::State::ACTIVE) {
            m_pTreeItem->performAction(VisualTreeItem::ViewAction::DETACH);

            // It should still exists, it may crash otherwise, so make sure early
            Q_ASSERT(m_pTreeItem->m_State == VisualTreeItem::State::POOLED);

            m_pTreeItem->m_State = VisualTreeItem::State::POOLED;
            //FIXME ^^ add a new action for finish pooling or call
            // VisualTreeItem::detach from a new action method (instead of directly)
        }

        m_pTreeItem->performAction(VisualTreeItem::ViewAction::DETACH);
        m_pTreeItem = nullptr;
    }


    d_ptr->_test_validateViewport();

    if (m_pParent) {
        const int size = m_pParent->m_hLookup.size();
        m_pParent->m_hLookup.remove(m_Index);
        Q_ASSERT(size == m_pParent->m_hLookup.size()+1);
    }

    if (m_tSiblings[PREVIOUS] || m_tSiblings[NEXT]) {
        if (m_tSiblings[PREVIOUS])
            m_tSiblings[PREVIOUS]->m_tSiblings[NEXT] = m_tSiblings[NEXT];
        if (m_tSiblings[NEXT])
            m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] = m_tSiblings[PREVIOUS];
    }
    else if (m_pParent) { //FIXME very wrong
        Q_ASSERT(m_pParent->m_hLookup.isEmpty());
        m_pParent->m_tChildren[FIRST] = nullptr;
        m_pParent->m_tChildren[LAST] = nullptr;
    }

    d_ptr->_test_validateViewport();

    //FIXME set the parent m_tChildren[FIRST] correctly and add an insert()/move() method
    // then drop bridgeGap

    return true;
}

bool TreeTraversalItems::refresh()
{
    //
//     qDebug() << "REFRESH";

    d_ptr->m_pViewport->s_ptr->updateGeometry(&m_Geometry);

    for (auto i = m_tChildren[FIRST]; i; i = i->m_tSiblings[NEXT]) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->m_Geometry.performAction(BlockMetadata::Action::UPDATE);

        if (i == m_tChildren[LAST])
            break;
    }

    Q_ASSERT(m_pTreeItem); //FIXME

    return true;
}

bool TreeTraversalItems::index()
{
    //TODO remove this implementation and use the one below

    // Propagate
    for (auto i = m_tChildren[FIRST]; i; i = i->m_tSiblings[NEXT]) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->m_Geometry.performAction(BlockMetadata::Action::MOVE);

        if (i == m_tChildren[LAST])
            break;
    }

    //FIXME this if should not exists, this should be handled by the state
    // machine.
    if (m_pTreeItem) {
        m_pTreeItem->performAction(VisualTreeItem::ViewAction::MOVE); //FIXME don't
    }
    //TODO update m_Geometry

    return true;

//     if (!m_pTreeItem)
//         return true;

//FIXME this is better, but require having createItem() called earlier
//     if (m_pTreeItem)
//         m_pTreeItem->performAction(VisualTreeItem::ViewAction::MOVE); //FIXME don't
//
//     if (oldGeo != m_pTreeItem->geometry())
//         if (auto n = m_pTreeItem->down())
//             n->m_pParent->performAction(BlockMetadata::Action::MOVE);

//     return true;
}

bool TreeTraversalItems::destroy()
{
    detach();

    m_pTreeItem = nullptr;

    Q_ASSERT(m_hLookup.isEmpty());

    delete this;
    return true;
}

bool TreeTraversalItems::reset()
{
    for (auto i = m_tChildren[FIRST]; i; i = i->m_tSiblings[NEXT]) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->m_Geometry.performAction(BlockMetadata::Action::RESET);

        if (i == m_tChildren[LAST])
            break;
    }

    if (m_pTreeItem) {
        Q_ASSERT(this != d_ptr->m_pRoot);
        m_pTreeItem->performAction(VisualTreeItem::ViewAction::LEAVE_BUFFER);
        m_pTreeItem->performAction(VisualTreeItem::ViewAction::DETACH);
        m_pTreeItem = nullptr;
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
//         Q_ASSERT(false); //FIXME so I can't forget when time comes
        return;
    }

    auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;

    TreeTraversalItems *prev(nullptr);

    //FIXME use up()
    if (first && pitem)
        prev = pitem->m_hLookup.value(q_ptr->model()->index(first-1, 0, parent));

    auto oldFirst = pitem->m_tChildren[FIRST];

    // There is no choice here but to load a larger subset to avoid holes, in
    // theory, m_Edges will prevent runaway loading
    if (pitem->m_tChildren[FIRST])
        last = std::max(last, pitem->m_tChildren[FIRST]->m_Index.row() - 1);

    //FIXME support smaller ranges
    for (int i = first; i <= last; i++) {
        auto idx = q_ptr->model()->index(i, 0, parent);
        Q_ASSERT(idx.isValid());
        Q_ASSERT(idx.parent() != idx);
        Q_ASSERT(idx.model() == q_ptr->model());

        auto e = addChildren(pitem, idx);

        // Keep a dual chained linked list between the visual elements
        e->m_tSiblings[PREVIOUS] = prev ? prev : nullptr; //FIXME incorrect

        //FIXME It can happen if the previous is out of the visible range
        Q_ASSERT( e->m_tSiblings[PREVIOUS] || e->m_Index.row() == 0);

        //TODO merge with bridgeGap
        if (prev)
            bridgeGap(prev, e, true);

        // This is required before ::ATTACH because otherwise ::down() wont work

        Q_ASSERT((!pitem->m_tChildren[FIRST]) || pitem->m_tChildren[FIRST]->m_MoveToRow == -1); //TODO

        // The item is about the current parent first item
        if ((!pitem->m_tChildren[FIRST]) || e->m_Index.row() <= pitem->m_tChildren[FIRST]->m_Index.row()) {
            e->m_tSiblings[NEXT] = pitem->m_tChildren[FIRST];
            pitem->m_tChildren[FIRST] = e;
        }

        if (!e->m_Geometry.performAction(BlockMetadata::Action::ATTACH))
            return;

        if ((!pitem->m_tChildren[FIRST]) || e->m_Index.row() <= pitem->m_tChildren[FIRST]->m_Index.row()) {
            Q_ASSERT(pitem != e);
            if (auto pe = e->up())
                pe->m_Geometry.performAction(BlockMetadata::Action::MOVE);
        }

        Q_ASSERT((!pitem->m_tChildren[LAST]) || pitem->m_tChildren[LAST]->m_MoveToRow == -1); //TODO

        if ((!pitem->m_tChildren[LAST]) || e->m_Index.row() > pitem->m_tChildren[LAST]->m_Index.row()) {
            Q_ASSERT(pitem != e);
            pitem->m_tChildren[LAST] = e;
            if (auto ne = e->down())
                ne->m_Geometry.performAction(BlockMetadata::Action::MOVE);
        }

        if (auto rc = q_ptr->model()->rowCount(idx) && m_Edges & Qt::BottomEdge)
            slotRowsInserted(idx, 0, rc-1);

        // Validate early to prevent propagating garbage that's nearly impossible
        // to debug.
        if (pitem && pitem != m_pRoot && !i) {
            Q_ASSERT(e->up() == pitem);
            Q_ASSERT(e == pitem->down());
        }

        prev = e;

    }

    // Fix the linked list
    if (oldFirst && prev && prev->m_Index.row() < oldFirst->m_Index.row()) {
        Q_ASSERT(!oldFirst->m_tSiblings[PREVIOUS]);
        Q_ASSERT(oldFirst->m_Index.row() == prev->m_Index.row() + 1);
        Q_ASSERT(prev->m_tSiblings[NEXT] == oldFirst);

        oldFirst->m_tSiblings[PREVIOUS] = prev;
    }

    if ((!pitem->m_tChildren[LAST]) || last > pitem->m_tChildren[LAST]->m_Index.row())
        pitem->m_tChildren[LAST] = prev;

    Q_ASSERT(pitem->m_tChildren[LAST]);

    //FIXME use down()
    if (q_ptr->model()->rowCount(parent) > last) {
        //detachUntil;
        if (!prev) {

        }
        else if (auto i = pitem->m_hLookup.value(q_ptr->model()->index(last+1, 0, parent))) {
            i->m_tSiblings[PREVIOUS] = prev;
            prev->m_tSiblings[NEXT] = i;
        }
//         else //FIXME it happens
//             Q_ASSERT(false);
    }

    _test_validateTree(m_pRoot);

    Q_EMIT q_ptr->contentChanged();

    if (!parent.isValid())
        Q_EMIT q_ptr->countChanged();
}

void TreeTraversalReflectorPrivate::slotRowsRemoved(const QModelIndex& parent, int first, int last)
{
    Q_ASSERT((!parent.isValid()) || parent.model() == q_ptr->model());
    Q_EMIT q_ptr->contentChanged();

    if (!q_ptr->isActive(parent, first, last))
        return;

    auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;

    //TODO make sure the state machine support them
    //TreeTraversalItems *prev(nullptr), *next(nullptr);

    //FIXME use up()
    //if (first && pitem)
    //    prev = pitem->m_hLookup.value(model()->index(first-1, 0, parent));

    //next = pitem->m_hLookup.value(model()->index(last+1, 0, parent));

    //FIXME support smaller ranges
    for (int i = first; i <= last; i++) {
        auto idx = q_ptr->model()->index(i, 0, parent);

        if (auto elem = pitem->m_hLookup.value(idx)) {
            elem->m_Geometry.performAction(BlockMetadata::Action::DETACH);
        }

    }

    if (!parent.isValid())
        Q_EMIT q_ptr->countChanged();
}

void TreeTraversalReflectorPrivate::slotLayoutChanged()
{
    if (auto rc = q_ptr->model()->rowCount())
        slotRowsInserted({}, 0, rc - 1);

    Q_EMIT q_ptr->contentChanged();

    Q_EMIT q_ptr->countChanged();
}

void TreeTraversalReflectorPrivate::createGap(TreeTraversalItems* first, TreeTraversalItems* last)
{
    Q_ASSERT(first->m_pParent == last->m_pParent);

    if (first->m_tSiblings[PREVIOUS]) {
        first->m_tSiblings[PREVIOUS]->m_tSiblings[NEXT] = last->m_tSiblings[NEXT];
    }

    if (last->m_tSiblings[NEXT]) {
        last->m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] = first->m_tSiblings[PREVIOUS];
    }

    if (first->m_pParent->m_tChildren[FIRST] == first)
        first->m_pParent->m_tChildren[FIRST] = last->m_tSiblings[NEXT];

    if (last->m_pParent->m_tChildren[LAST] == last)
        last->m_pParent->m_tChildren[LAST] = first->m_tSiblings[PREVIOUS];

    Q_ASSERT((!first->m_tSiblings[PREVIOUS]) ||
        first->m_tSiblings[PREVIOUS]->down() != first);
    Q_ASSERT((!last->m_tSiblings[NEXT]) ||
        last->m_tSiblings[NEXT]->up() != last);

    Q_ASSERT((!first) || first->m_tChildren[FIRST] || first->m_hLookup.isEmpty());
    Q_ASSERT((!last) || last->m_tChildren[FIRST] || last->m_hLookup.isEmpty());

    // Do not leave invalid pointers for easier debugging
    last->m_tSiblings[NEXT]      = nullptr;
    first->m_tSiblings[PREVIOUS] = nullptr;
}

/// Fix the issues introduced by createGap (does not update m_pParent and m_hLookup)
void TreeTraversalReflectorPrivate::bridgeGap(TreeTraversalItems* first, TreeTraversalItems* second, bool insert)
{
    // 3 possible case: siblings, first child or last child

    if (first && second && first->m_pParent == second->m_pParent) {
        // first and second are siblings

        // Assume the second item is new
        if (insert && first->m_tSiblings[NEXT]) {
            second->m_tSiblings[NEXT] = first->m_tSiblings[NEXT];
            first->m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] = second;
        }

        first->m_tSiblings[NEXT] = second;
        second->m_tSiblings[PREVIOUS] = first;
    }
    else if (second && ((!first) || first == second->m_pParent)) {
        // The `second` is `first` first child or it's the new root
        second->m_tSiblings[PREVIOUS] = nullptr;

        if (!second->m_pParent->m_tChildren[LAST])
            second->m_pParent->m_tChildren[LAST] = second;

        second->m_tSiblings[NEXT] = second->m_pParent->m_tChildren[FIRST];

        if (second->m_pParent->m_tChildren[FIRST]) {
            second->m_pParent->m_tChildren[FIRST]->m_tSiblings[PREVIOUS] = second;
        }

        second->m_pParent->m_tChildren[FIRST] = second;

        //BEGIN test
        /*int count =0;
        for (auto c = second->m_pParent->m_tChildren[FIRST]; c; c = c->m_tSiblings[NEXT])
            count++;
        Q_ASSERT(count == second->m_pParent->m_hLookup.size());*/
        //END test
    }
    else if (first) {
        // It's the last element or the second is a last leaf and first is unrelated
        first->m_tSiblings[NEXT] = nullptr;

        if (!first->m_pParent->m_tChildren[FIRST])
            first->m_pParent->m_tChildren[FIRST] = first;

        if (first->m_pParent->m_tChildren[LAST] && first->m_pParent->m_tChildren[LAST] != first) {
            first->m_pParent->m_tChildren[LAST]->m_tSiblings[NEXT] = first;
            first->m_tSiblings[PREVIOUS] = first->m_pParent->m_tChildren[LAST];
        }

        first->m_pParent->m_tChildren[LAST] = first;

        //BEGIN test
        int count =0;
        for (auto c = first->m_pParent->m_tChildren[LAST]; c; c = c->m_tSiblings[PREVIOUS])
            count++;

        Q_ASSERT(first->m_pParent->m_tChildren[FIRST]);

        Q_ASSERT(count == first->m_pParent->m_hLookup.size());
        //END test
    }
    else {
        Q_ASSERT(false); //Something went really wrong elsewhere
    }

    if (second && first && second->m_pParent && second->m_pParent->m_tChildren[FIRST] == second && second->m_pParent == first->m_pParent) {
        second->m_pParent->m_tChildren[FIRST] = first;
    }

    if ((!first) && second->m_MoveToRow != -1) {
        Q_ASSERT(second->m_pParent->m_tChildren[FIRST]);
        Q_ASSERT(second->m_pParent->m_tChildren[FIRST] == second ||
            second->m_pParent->m_tChildren[FIRST]->m_Index.row() < second->m_MoveToRow);
    }

    if (first)
        Q_ASSERT(first->m_pParent->m_tChildren[FIRST]);
    if (second)
        Q_ASSERT(second->m_pParent->m_tChildren[FIRST]);

    if (first)
        Q_ASSERT(first->m_pParent->m_tChildren[LAST]);
    if (second)
        Q_ASSERT(second->m_pParent->m_tChildren[LAST]);

//     if (first && second) { //Need to disable other asserts in down()
//         Q_ASSERT(first->down() == second);
//         Q_ASSERT(second->up() == first);
//     }
}

void TreeTraversalReflectorPrivate::setTemporaryIndices(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    //FIXME list only
    // Before moving them, set a temporary now/col value because it wont be set
    // on the index until before slotRowsMoved2 is called (but after this
    // method returns //TODO do not use the hashmap, it is already known
    if (parent == destination) {
        const auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;
        for (int i = start; i <= end; i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);

            auto elem = pitem->m_hLookup.value(idx);
            Q_ASSERT(elem);

            elem->m_MoveToRow = row + (i - start);
        }

        for (int i = row; i <= row + (end - start); i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);

            auto elem = pitem->m_hLookup.value(idx);
            Q_ASSERT(elem);

            elem->m_MoveToRow = row + (end - start) + 1;
        }
    }
}

void TreeTraversalReflectorPrivate::resetTemporaryIndices(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    //FIXME list only
    // Before moving them, set a temporary now/col value because it wont be set
    // on the index until before slotRowsMoved2 is called (but after this
    // method returns //TODO do not use the hashmap, it is already known
    if (parent == destination) {
        const auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;
        for (int i = start; i <= end; i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);
            auto elem = pitem->m_hLookup.value(idx);
            Q_ASSERT(elem);
            elem->m_MoveToRow = -1;
        }

        for (int i = row; i <= row + (end - start); i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);
            auto elem = pitem->m_hLookup.value(idx);
            Q_ASSERT(elem);
            elem->m_MoveToRow = -1;
        }
    }
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

    setTemporaryIndices(parent, start, end, destination, row);

    // As the actual view is implemented as a daisy chained list, only moving
    // the edges is necessary for the TreeTraversalItems. Each VisualTreeItem
    // need to be moved.

    const auto idxStart = q_ptr->model()->index(start, 0, parent);
    const auto idxEnd   = q_ptr->model()->index(end  , 0, parent);
    Q_ASSERT(idxStart.isValid() && idxEnd.isValid());

    //FIXME once partial ranges are supported, this is no longer always valid
    auto startTTI = ttiForIndex(idxStart);
    auto endTTI   = ttiForIndex(idxEnd);

    if (end - start == 1)
        Q_ASSERT(startTTI->m_tSiblings[NEXT] == endTTI);

    //FIXME so I don't forget, it will mess things up if silently ignored
    Q_ASSERT(startTTI && endTTI);
    Q_ASSERT(startTTI->m_pParent == endTTI->m_pParent);

    auto oldPreviousTTI = startTTI->up();
    auto oldNextTTI     = endTTI->down();

    Q_ASSERT((!oldPreviousTTI) || oldPreviousTTI->down() == startTTI);
    Q_ASSERT((!oldNextTTI) || oldNextTTI->up() == endTTI);

    auto newNextIdx = q_ptr->model()->index(row, 0, destination);

    // You cannot move things into an empty model
    Q_ASSERT((!row) || newNextIdx.isValid());

    TreeTraversalItems *newNextTTI(nullptr), *newPrevTTI(nullptr);

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
        newPrevTTI = newNextTTI ? newNextTTI->up() : nullptr;
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

    TreeTraversalItems* newParentTTI = ttiForIndex(destination);
    newParentTTI = newParentTTI ? newParentTTI : m_pRoot;
    auto oldParentTTI = startTTI->m_pParent;

    // Make sure not to leave invalid pointers while the steps below are being performed
    createGap(startTTI, endTTI);

    // Update the tree parent (if necessary)
    if (oldParentTTI != newParentTTI) {
        for (auto i = startTTI; i; i = i->m_tSiblings[NEXT]) {
            auto idx = i->m_Index;

            const int size = oldParentTTI->m_hLookup.size();
            oldParentTTI->m_hLookup.remove(idx);
            Q_ASSERT(oldParentTTI->m_hLookup.size() == size-1);

            newParentTTI->m_hLookup[idx] = i;
            i->m_pParent = newParentTTI;
            if (i == endTTI)
                break;
        }
    }

    Q_ASSERT(startTTI->m_pParent == newParentTTI);
    Q_ASSERT(endTTI->m_pParent   == newParentTTI);

    bridgeGap(newPrevTTI, startTTI );
    bridgeGap(endTTI   , newNextTTI);

    // Close the gap between the old previous and next elements
    Q_ASSERT(startTTI->m_tSiblings[NEXT]     != startTTI);
    Q_ASSERT(startTTI->m_tSiblings[PREVIOUS] != startTTI);
    Q_ASSERT(endTTI->m_tSiblings[NEXT]       != endTTI  );
    Q_ASSERT(endTTI->m_tSiblings[PREVIOUS]   != endTTI  );

    //BEGIN debug
    if (newPrevTTI) {
        int count = 0;
        for (auto c = newPrevTTI->m_pParent->m_tChildren[FIRST]; c; c = c->m_tSiblings[NEXT])
            count++;
        Q_ASSERT(count == newPrevTTI->m_pParent->m_hLookup.size());

        count = 0;
        for (auto c = newPrevTTI->m_pParent->m_tChildren[LAST]; c; c = c->m_tSiblings[PREVIOUS])
            count++;
        Q_ASSERT(count == newPrevTTI->m_pParent->m_hLookup.size());
    }
    //END

    bridgeGap(oldPreviousTTI, oldNextTTI);


    if (endTTI->m_tSiblings[NEXT]) {
        Q_ASSERT(endTTI->m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] == endTTI);
    }

    if (startTTI->m_tSiblings[PREVIOUS]) {
        Q_ASSERT(startTTI->m_tSiblings[PREVIOUS]->m_pParent == startTTI->m_pParent);
        Q_ASSERT(startTTI->m_tSiblings[PREVIOUS]->m_tSiblings[NEXT] == startTTI);
    }

    Q_ASSERT(newParentTTI->m_tChildren[FIRST]);
    Q_ASSERT(startTTI == newParentTTI->m_tChildren[FIRST] ||
        newParentTTI->m_tChildren[FIRST]->m_Index.row() <= startTTI->m_MoveToRow);
    Q_ASSERT(row || newParentTTI->m_tChildren[FIRST] == startTTI);


//     Q_ASSERT((!newNextVI) || newNextVI->m_pParent->m_tSiblings[PREVIOUS] == endVI->m_pParent);
//     Q_ASSERT((!newPrevVI) ||
// //         newPrevVI->m_pParent->m_tSiblings[NEXT] == startVI->m_pParent ||
//         (newPrevVI->m_pParent->m_tChildren[FIRST] == startVI->m_pParent && !row)
//     );

//     Q_ASSERT((!oldPreviousVI) || (!oldPreviousVI->m_pParent->m_tSiblings[NEXT]) ||
//         oldPreviousVI->m_pParent->m_tSiblings[NEXT] == (oldNextVI ? oldNextVI->m_pParent : nullptr));


    // Move everything
    //TODO move it more efficient
    m_pRoot->m_Geometry.performAction(BlockMetadata::Action::MOVE);

    resetTemporaryIndices(parent, start, end, destination, row);
}

void TreeTraversalReflectorPrivate::slotRowsMoved2(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    Q_UNUSED(parent)
    Q_UNUSED(start)
    Q_UNUSED(end)
    Q_UNUSED(destination)
    Q_UNUSED(row)

    // The test would fail if it was in aboutToBeMoved
    _test_validateTree(m_pRoot);
}

// void TreeTraversalReflector::resetEverything()
// {
//     // There is nothing to reset when there is no model
//     if (!model())
//         return;
//
//     d_ptr->m_pRoot->m_Geometry.performAction(BlockMetadata::Action::RESET);
// }

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
    connect(m_pModel, &QAbstractItemModel::rowsMoved, this,
        &TreeTraversalReflectorPrivate::slotRowsMoved2);

    m_pTrackedModel = m_pModel;
}

void TreeTraversalReflectorPrivate::free()
{
    m_pRoot->m_Geometry.performAction(BlockMetadata::Action::RESET);

    m_Edges = {Qt::TopEdge|Qt::LeftEdge|Qt::RightEdge|Qt::BottomEdge};

    for (int i = Pos::Top; i <= Pos::Bottom; i++)
        m_lpEdges[i] = nullptr;

    m_hMapper.clear();
    delete m_pRoot;
    m_pRoot = new TreeTraversalItems(nullptr, this);
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
    disconnect(m_pTrackedModel, &QAbstractItemModel::rowsMoved, this,
        &TreeTraversalReflectorPrivate::slotRowsMoved2);

    m_pTrackedModel = nullptr;
}

void TreeTraversalReflectorPrivate::populate()
{
    Q_ASSERT(m_pModel);


    if (m_pRoot->m_tChildren[FIRST] && (m_Edges & (Qt::TopEdge|Qt::BottomEdge))) {
        Q_ASSERT(m_lpEdges[Pos::Top]);
        while (m_Edges & Qt::TopEdge) {
            const auto was = m_lpEdges[Pos::Top];

            auto u = m_lpEdges[Pos::Top]->loadUp();

            Q_ASSERT(u || m_lpEdges[Pos::Top]->m_Index.row() == 0);
            Q_ASSERT(u || !m_lpEdges[Pos::Top]->m_Index.parent().isValid());

            if (!u)
                break;

            u->m_Geometry.performAction(BlockMetadata::Action::SHOW);
        }

        while (m_Edges & Qt::BottomEdge) {
            const auto was = m_lpEdges[Pos::Bottom];

            auto u = m_lpEdges[Pos::Bottom]->loadDown();

            Q_ASSERT(u || m_lpEdges[Pos::Bottom]->m_Index.row() == 0);
            Q_ASSERT(u || !m_lpEdges[Pos::Bottom]->m_Index.parent().isValid());

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

void TreeTraversalReflectorPrivate::nothing()
{}

void TreeTraversalReflectorPrivate::error()
{
    Q_ASSERT(false);
    q_ptr->performAction(TreeTraversalReflector::TrackingAction::DISABLE);
    q_ptr->performAction(TreeTraversalReflector::TrackingAction::RESET);
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


bool TreeTraversalReflector::detachUntil(Qt::Edge from, VisualTreeItem *to)
{
    return detachUntil(from, to ? to->m_pTTI : nullptr);
}

bool TreeTraversalReflector::detachUntil(Qt::Edge from, TreeTraversalItems *to)
{
    Q_UNUSED(from)
    if (!to)
        return false;

    if (to->m_pTreeItem)
        to->m_pTreeItem->performAction(VisualTreeItem::ViewAction::LEAVE_BUFFER);

    return false;
}

bool TreeTraversalReflectorPrivate::isInsertActive(const QModelIndex& p, int first, int last) const
{
    if (m_Edges & (Qt::TopEdge|Qt::BottomEdge))
        return true;

    auto parent = p.isValid() ? ttiForIndex(p) : m_pRoot;

    // It's controversial as it means if the items would otherwise be
    // visible because their parent "virtual" position + insertion height
    // could happen to be in the view but in this case the scrollbar would
    // move. Mobile views tend to not shuffle the current content around so
    // from that point of view it is correct, but if the item is in the
    // buffer, then it will move so doing this is a bit buggy.
    if (!parent)
        return false;

    // Return true if the previous element or next element are loaded
    const QModelIndex prev = first ? m_pModel->index(first - 1, 0, p) : QModelIndex();
    const QModelIndex next = first ? m_pModel->index(last  + 1, 0, p) : QModelIndex();

    return (prev.isValid() && parent->m_hLookup.contains(prev))
        || (next.isValid() && parent->m_hLookup.contains(next));
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
TreeTraversalItems* TreeTraversalReflectorPrivate::addChildren(TreeTraversalItems* parent, const QModelIndex& index)
{
    Q_ASSERT(index.isValid());
    Q_ASSERT(index.parent() != index);

    auto e = new TreeTraversalItems(parent, this);
    e->m_Index = index;

    const int oldSize(m_hMapper.size()), oldSize2(parent->m_hLookup.size());

    Q_ASSERT(!m_hMapper.contains(index));

    m_hMapper [index] = e;
    parent->m_hLookup[index] = e;

    // If the size did not grow, something leaked
    Q_ASSERT(m_hMapper.size() == oldSize+1);
    Q_ASSERT(parent->m_hLookup.size() == oldSize2+1);

    return e;
}

void TreeTraversalReflectorPrivate::cleanup()
{
    m_pRoot->m_Geometry.performAction(BlockMetadata::Action::DETACH);

    m_hMapper.clear();
    m_pRoot = new TreeTraversalItems(nullptr, this);
    //m_FailedCount = 0;
}

TreeTraversalItems* TreeTraversalReflectorPrivate::ttiForIndex(const QModelIndex& idx) const
{
    if (!idx.isValid())
        return nullptr;

    if (!idx.parent().isValid())
        return m_pRoot->m_hLookup.value(idx);

    if (auto parent = m_hMapper.value(idx.parent()))
        return parent->m_hLookup.value(idx);

    return nullptr;
}

AbstractItemAdapter* TreeTraversalReflector::itemForIndex(const QModelIndex& idx) const
{
    const auto tti = d_ptr->ttiForIndex(idx);
    return tti && tti->m_pTreeItem ? tti->m_pTreeItem->d_ptr : nullptr;
}

void TreeTraversalReflector::setAvailableEdges(Qt::Edges edges)
{
    d_ptr->m_Edges = edges;
}

Qt::Edges TreeTraversalReflector::availableEdges() const
{
    return d_ptr->m_Edges;
}

QModelIndex BlockMetadata::index() const
{
    return m_pTTI->m_Index;
}

QPersistentModelIndex VisualTreeItem::index() const
{
    return m_pTTI->m_Index;
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

    auto ret = m_pTTI->up();
    //TODO support collapsed nodes

    // Linearly look for a valid element. Doing this here allows the views
    // that implement this (abstract) class to work without having to always
    // check if some of their item failed to load. This is non-fatal in the
    // other Qt views, so it isn't fatal here either.
    while (ret && !ret->m_pTreeItem)
        ret = ret->up();

    if (ret && ret->m_pTreeItem)
        Q_ASSERT(ret->m_pTreeItem->m_State != State::POOLING && ret->m_pTreeItem->m_State != State::DANGLING);

    return ret ? ret->m_pTreeItem : nullptr;
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

    auto ret = m_pTTI->down();
    //TODO support collapsed entries

    // Recursively look for a valid element. Doing this here allows the views
    // that implement this (abstract) class to work without having to always
    // check if some of their item failed to load. This is non-fatal in the
    // other Qt views, so it isn't fatal here either.
    while (ret && !ret->m_pTreeItem)
        ret = ret->down();

    return ret ? ret->m_pTreeItem : nullptr;
}

int VisualTreeItem::row() const
{
    return m_pTTI->m_MoveToRow == -1 ?
        index().row() : m_pTTI->m_MoveToRow;
}

int VisualTreeItem::column() const
{
    return m_pTTI->m_MoveToColumn == -1 ?
        index().column() : m_pTTI->m_MoveToColumn;
}

BlockMetadata *BlockMetadata::up() const
{
    auto i = m_pTTI->up();
    return i ? &i->m_Geometry : nullptr;
}

BlockMetadata *BlockMetadata::down() const
{
    auto i = m_pTTI->down();
    return i ? &i->m_Geometry : nullptr;
}

BlockMetadata *BlockMetadata::left() const
{
    auto i = m_pTTI->left();
    return i ? &i->m_Geometry : nullptr;
}

BlockMetadata *BlockMetadata::right() const
{
    auto i = m_pTTI->right();
    return i ? &i->m_Geometry : nullptr;
}

void VisualTreeItem::setCollapsed(bool v)
{
    m_pTTI->m_IsCollapsed = v;
}

bool VisualTreeItem::isCollapsed() const
{
    return m_pTTI->m_IsCollapsed;
}

#undef PREVIOUS
#undef NEXT
#undef FIRST
#undef LAST

#include <treetraversalreflector_p.moc>
