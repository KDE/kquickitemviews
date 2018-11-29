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
#include "statetracker/proximity_p.h"
#include "statetracker/model_p.h"
#include "statetracker/modelitem_p.h"
#include "adapters/abstractitemadapter.h"
#include "viewport.h"
#include "viewport_p.h"
#include "indexmetadata_p.h"

// Qt
#include <QQmlComponent>
#include <QtCore/QDebug>
#include <QtCore/QTimer>

using EdgeType = IndexMetadata::EdgeType;

#include <treetraversalreflector_debug.h>
#include <private/treetraversalreflector_p2.h>

#define A &TreeTraversalReflectorPrivate::
// Keep track of the number of instances per state
const TreeTraversalReflectorPrivate::StateFS TreeTraversalReflectorPrivate::m_fStateLogging[8][2] = {
/*                ENTER           LEAVE    */
/*NEW      */ { A error     , A leaveState },
/*BUFFER   */ { A enterState, A leaveState },
/*REMOVED  */ { A enterState, A leaveState },
/*REACHABLE*/ { A enterState, A leaveState },
/*VISIBLE  */ { A insertEdge, A removeEdge },
/*ERROR    */ { A enterState, A leaveState },
/*DANGLING */ { A enterState, A error      },
/*MOVING   */ { A enterState, A resetState },
};
#undef A

void TreeTraversalReflectorPrivate::insertEdge(StateTracker::ModelItem* tti, StateTracker::ModelItem::State s)
{
    Q_UNUSED(s)
    const auto first = edges(EdgeType::VISIBLE)->getEdge(  Qt::TopEdge   );
    const auto last  = edges(EdgeType::VISIBLE)->getEdge( Qt::BottomEdge );

    const auto prev = tti->up();
    const auto next = tti->down();

    if (first == next)
        setEdge(EdgeType::VISIBLE, tti, Qt::TopEdge);

    if (last == prev)
        setEdge(EdgeType::VISIBLE, tti, Qt::BottomEdge);

    _test_validate_edges_simple();
}

void TreeTraversalReflectorPrivate::removeEdge(StateTracker::ModelItem* tti, StateTracker::ModelItem::State s)
{
    Q_UNUSED(s)
    const auto first = edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge);
    const auto last  = edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge);

    // The item was somewhere in between, not an edge case
    if ((tti != first) && (tti != last))
        return;

    auto prev = tti->up();
    auto next = tti->down();

    if (tti == first)
        setEdge(EdgeType::VISIBLE,
            (next && next->metadata()->modelTracker()->state() == StateTracker::ModelItem::State::VISIBLE) ?
                next : nullptr,
            Qt::TopEdge
        );

    if (tti == last)
        setEdge(EdgeType::VISIBLE,
            (prev && prev->metadata()->modelTracker()->state() == StateTracker::ModelItem::State::VISIBLE) ?
                prev : nullptr,
            Qt::BottomEdge
        );

    _test_validate_edges_simple();
}

/**
 * This method is called when an item moves into a new area and the "real"
 * new state needs to be determined.
 */
void TreeTraversalReflectorPrivate::resetState(StateTracker::ModelItem* i, StateTracker::ModelItem::State)
{
    i->rebuildState();
}

TreeTraversalReflectorPrivate::TreeTraversalReflectorPrivate() :
m_pModelTracker(new StateTracker::Model(this))
{}

TreeTraversalReflector::TreeTraversalReflector(Viewport* parent) : QObject(parent),
    d_ptr(new TreeTraversalReflectorPrivate())
{
    d_ptr->m_pViewport = parent;
    d_ptr->q_ptr       = this;
}

TreeTraversalReflector::~TreeTraversalReflector()
{
    delete d_ptr->m_pRoot;
}

void TreeTraversalReflector::setItemFactory(std::function<AbstractItemAdapter*()> factory)
{
    d_ptr->m_fFactory = factory;
}

void TreeTraversalReflectorPrivate::slotRowsInserted(const QModelIndex& parent, int first, int last)
{
    Q_ASSERT(((!parent.isValid()) || parent.model() == q_ptr->model()) && first <= last);

    // It is the job of isInsertActive to decide what's correct
    if (!isInsertActive(parent, first, last)) {
        _test_validateLinkedList();
        return;
    }

    const auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;

    //FIXME it is possible if the anchor is at the bottom that the parent
    // needs to be loaded. But this is currently too not supported.
    if (!pitem) {
        _test_validateLinkedList();
        return;
    }

    StateTracker::Index *prev = nullptr;

    //FIXME use up()
    if (first && pitem)
        prev = pitem->childrenLookup(q_ptr->model()->index(first-1, 0, parent));

    // There is no choice here but to load a larger subset to avoid holes, in
    // theory, edges(EdgeType::FREE)->m_Edges will prevent runaway loading
    if (pitem->firstChild())
        last = std::max(last, pitem->firstChild()->effectiveRow() - 1);

    if (prev && prev->down()) {
        m_pViewport->s_ptr->notifyInsert(prev->down()->metadata());
        //Q_ASSERT(!TTI(prev->down())->metadata()->isValid());
    }
    else if ((!prev) && (!pitem) && m_pRoot->firstChild()) {
        Q_ASSERT((!first) && !parent.isValid());
        m_pViewport->s_ptr->notifyInsert(m_pRoot->firstChild()->metadata());
        //Q_ASSERT(!TTI(m_pRoot->firstChild())->metadata()->isValid());
    }
    else if (pitem && pitem != m_pRoot && pitem->down()) {
        //Q_ASSERT(!first);
        m_pViewport->s_ptr->notifyInsert(pitem->down()->metadata());

        //TODO check if this one is still correct, right now notifyInsert update the geo
        //Q_ASSERT(!TTI(pitem->down())->metadata()->isValid());
    }

    //FIXME support smaller ranges
    for (int i = first; i <= last; i++) {
        auto idx = q_ptr->model()->index(i, 0, parent);
        Q_ASSERT(idx.isValid() && idx.parent() != idx && idx.model() == q_ptr->model());

        // If the insertion is sandwiched between loaded items, not doing it
        // will corrupt the view, but if it's a "tail" insertion, then they
        // can be discarded.
        if (!(edges(EdgeType::FREE)->m_Edges & (Qt::BottomEdge | Qt::TopEdge))) {

            const auto nextIdx = getNextIndex(idx);
            const auto nextTTI = nextIdx.isValid() ? ttiForIndex(nextIdx) : nullptr;

            if (!nextTTI) {
                m_pViewport->s_ptr->refreshVisible();
                _test_validateLinkedList();
                return; //FIXME break
            }
        }

        auto e = addChildren(idx);

//         e->parent()->_test_validate_chain();

        if (pitem->firstChild() && pitem->firstChild()->effectiveRow() == idx.row()+1) {
            Q_ASSERT(idx.parent() == pitem->firstChild()->effectiveParentIndex());

//             m_pViewport->s_ptr->notifyInsert(&TTI(pitem->firstChild())->m_Geometry);
            StateTracker::Index::insertChildBefore(e, pitem->firstChild(), pitem);
        }
        else {
            StateTracker::Index::insertChildAfter(e, prev, pitem); //FIXME incorrect
//             m_pViewport->s_ptr->notifyInsert(&TTI(prev)->m_Geometry);
        }

        //FIXME It can happen if the previous is out of the visible range
        Q_ASSERT( e->previousSibling() || e->nextSibling() || e->effectiveRow() == 0);

        //TODO merge with bridgeGap
        if (prev) {
            StateTracker::Index::bridgeGap(prev, e);
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

            m_pViewport->s_ptr->notifyInsert(m_pRoot->firstChild()->metadata());
            Q_ASSERT((!m_pRoot->firstChild()) || !m_pRoot->firstChild()->metadata()->isValid());
            StateTracker::Index::insertChildBefore(e, nullptr, pitem);
        }
        e->parent()->_test_validate_chain();

        e->parent()->_test_validate_chain();

        Q_ASSERT(e->metadata()->removeMe() == (int)StateTracker::Geometry::State::INIT);

//         if (auto ne = TTI(e->down()))
//             Q_ASSERT(ne->metadata()->m_State.state() != StateTracker::Geometry::State::VALID);

        Q_ASSERT(e->state() != StateTracker::ModelItem::State::VISIBLE);

        // NEW -> REACHABLE, this should never fail
        if (!e->metadata()->performAction(IndexMetadata::LoadAction::ATTACH)) {
            qDebug() << "\n\nATTACH FAILED";
            _test_validateLinkedList();
            break;
        }

        if (needUpMove) {
            Q_ASSERT(pitem != e);
            if (auto pe = e->up())
                pe->metadata() << IndexMetadata::LoadAction::MOVE;
        }

        Q_ASSERT((!pitem->lastChild()) || !pitem->lastChild()->hasTemporaryIndex()); //TODO

        if (needDownMove) {
            if (auto ne = e->down()) {
                Q_ASSERT(!ne->metadata()->isValid());

                ne->metadata() << IndexMetadata::LoadAction::MOVE;
                Q_ASSERT(!ne->metadata()->isValid());
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
        _test_validateUnloaded(parent, first, last);

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

        if (auto elem = pitem->childrenLookup(idx)) {
            elem->metadata()
                << IndexMetadata::LoadAction::HIDE
                << IndexMetadata::LoadAction::DETACH;
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
    Q_ASSERT(startTTI->parent() == endTTI->parent()); //TODO not implemented
    Q_ASSERT(startTTI && endTTI); //TODO partially loaded move are not implemented

    auto oldPreviousTTI = startTTI->up();
    auto oldNextTTI     = endTTI->down();

    Q_ASSERT((!oldPreviousTTI) || oldPreviousTTI->down() == startTTI);
    Q_ASSERT((!oldNextTTI) || oldNextTTI->up() == endTTI);

    auto newNextIdx = q_ptr->model()->index(row, 0, destination);

    // You cannot move things into an empty model
    Q_ASSERT((!row) || newNextIdx.isValid());

    StateTracker::Index *newNextTTI(nullptr), *newPrevTTI(nullptr);

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

    StateTracker::ModelItem* newParentTTI = ttiForIndex(destination);
    Q_ASSERT(newParentTTI || !destination.isValid()); //TODO not coded yet

    newParentTTI = newParentTTI ? newParentTTI : m_pRoot;

    // Remove everything //TODO add batching again
    StateTracker::Index* tti = endTTI;
    StateTracker::Index* dest = newNextTTI && newNextTTI->parent() == newParentTTI ?
        newNextTTI : nullptr;

    QList<StateTracker::Index*> tmp2;
    do {
        tmp2 << tti;
    } while(endTTI != startTTI && (tti = tti->previousSibling()) != startTTI);

    if (endTTI != startTTI) //FIXME use a better condition
        tmp2 << startTTI;

    Q_ASSERT( (end - start) == tmp2.size() - 1);

    // Check if the point where
    //TODO this isn't good enough
    bool isRangeVisible = (newPrevTTI &&
        newPrevTTI->metadata()->modelTracker()->state() == StateTracker::ModelItem::State::VISIBLE) || (newNextTTI &&
            newNextTTI->metadata()->modelTracker()->state() == StateTracker::ModelItem::State::VISIBLE);

    const auto topEdge    = edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge);
    const auto bottomEdge = edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge);

    bool needRefreshVisibleTop    = false;
    bool needRefreshVisibleBottom = false;
    /*bool needRefreshBufferTop     = false; //TODO lateral move is not implemented
    bool needRefreshBufferBottom  = false;*/

    for (auto item : qAsConst(tmp2)) {
        needRefreshVisibleTop    |= item != topEdge;
        needRefreshVisibleBottom |= item != bottomEdge;

        item->metadata() << IndexMetadata::GeometryAction::MOVE;

        item->remove();

        StateTracker::Index::insertChildBefore(item, dest, newParentTTI);

        Q_ASSERT((!dest) || item->down() == dest);

        if (dest)
            dest->metadata() << IndexMetadata::GeometryAction::MOVE;

        _test_validate_edges_simple();
        m_pViewport->s_ptr->notifyInsert(item->metadata());

        dest = item;
        if (isRangeVisible) {
            if (item->metadata()->modelTracker()->state() == StateTracker::ModelItem::State::BUFFER)
                item->metadata() << IndexMetadata::LoadAction::ATTACH;

            item->metadata() << IndexMetadata::LoadAction::SHOW;
        }
    }

    _test_validate_move(newParentTTI, startTTI, endTTI, newPrevTTI, newNextTTI, row);

    resetTemporaryIndices(tmp);

    // The visible edges are now probably incorectly placed, reload them
    if (needRefreshVisibleTop || needRefreshVisibleBottom) {
        reloadEdges();
        m_pViewport->s_ptr->refreshVisible();
    }

    //WARNING The indices still are in transition mode, do not use their value
}

QList<StateTracker::Index*> TreeTraversalReflectorPrivate::setTemporaryIndices(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    //FIXME This exists but for now ignoring it hasn't caused too many problems
    if (parent != destination) {
        qWarning() << "setTemporaryIndices called with corrupted arguments";
        return {};
    }

    QList<StateTracker::Index*> ret;

    // Before moving them, set a temporary now/col value because it wont be set
    // on the index until before endMoveRows is called (but after this
    // method returns).

    const auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;

    for (int i = start; i <= end; i++) {
        auto idx = q_ptr->model()->index(i, 0, parent);

        //TODO do not use the hashmap, it is already known
        auto elem = pitem->childrenLookup(idx);
        Q_ASSERT(elem);

        elem->setTemporaryIndex(
            destination, row + (i - start), idx.column()
        );
        ret << elem;
    }

    for (int i = row; i <= row + (end - start); i++) {
        auto idx = q_ptr->model()->index(i, 0, parent);

        //TODO do not use the hashmap, it is already known
        auto elem = pitem->childrenLookup(idx);
        Q_ASSERT(elem);

        elem->setTemporaryIndex(
            destination, row + (end - start) + 1, idx.column()
        );
        ret << elem;
    }

    return ret;
}

void TreeTraversalReflectorPrivate::resetTemporaryIndices(const QList<StateTracker::Index*>& indices)
{
    for (auto i : qAsConst(indices))
        i->resetTemporaryIndex();
}

void TreeTraversalReflectorPrivate::resetEdges()
{
    for (int i = 0; i < (int) EdgeType::NONE; i++)
        for (const auto e : {Qt::BottomEdge, Qt::TopEdge, Qt::LeftEdge, Qt::RightEdge})
            setEdge((EdgeType)i, nullptr, e);
}

// Go O(N) for now. Optimize when it becomes a problem (read: soon)
void TreeTraversalReflectorPrivate::reloadEdges()
{
    //FIXME optimize this

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
        const uint16_t isVisible = item->metadata()->modelTracker()->state() == StateTracker::ModelItem::State::VISIBLE ?
            IS_VISIBLE : IS_NOT_VISIBLE;
        const uint16_t isBuffer  = item->metadata()->modelTracker()->state() == StateTracker::ModelItem::State::BUFFER  ?
            IS_BUFFER : IS_NOT_BUFFER;

        switch(pos | isVisible | isBuffer) {
            case Range::TOP    | Range::IS_BUFFER:
                pos = Range::BUFFER_TOP;
                setEdge(EdgeType::BUFFERED, item, Qt::TopEdge);
                break;
            case Range::TOP        | Range::IS_VISIBLE:
            case Range::BUFFER_TOP | Range::IS_VISIBLE:
                pos = Range::VISIBLE;
                setEdge(EdgeType::VISIBLE, item, Qt::TopEdge);
                break;
            case Range::VISIBLE | Range::IS_NOT_VISIBLE:
            case Range::VISIBLE | Range::IS_NOT_BUFFER:
                setEdge(EdgeType::VISIBLE, item->up(), Qt::BottomEdge);
                pos = Range::BOTTOM;
                break;
            case Range::VISIBLE | Range::IS_NOT_VISIBLE | Range::IS_BUFFER:
                pos = Range::BUFFER_BOTTOM;
                break;
            case Range::VISIBLE | Range::IS_NOT_BUFFER | Range::IS_NOT_VISIBLE:
                setEdge(EdgeType::BUFFERED, item->up(), Qt::BottomEdge);
                pos = Range::BOTTOM;
                break;
        }
    }
}

QAbstractItemModel* TreeTraversalReflector::model() const
{
    return d_ptr->m_pModel;
}

void TreeTraversalReflectorPrivate::enterState(StateTracker::ModelItem* tti, StateTracker::ModelItem::State s)
{
    Q_UNUSED(tti);
    Q_UNUSED(s);
    //TODO count the number of active objects in each states for the "frame" load balancing
}

void TreeTraversalReflectorPrivate::leaveState(StateTracker::ModelItem *tti, StateTracker::ModelItem::State s)
{
    Q_UNUSED(tti);
    Q_UNUSED(s);
    //TODO count the number of active objects in each states for the "frame" load balancing
}

void TreeTraversalReflectorPrivate::error(StateTracker::ModelItem*, StateTracker::ModelItem::State)
{
    Q_ASSERT(false);
}

void TreeTraversalReflector::setModel(QAbstractItemModel* m)
{
    if (m == model())
        return;

    d_ptr->m_pModelTracker
        << StateTracker::Model::Action::DISABLE
        << StateTracker::Model::Action::RESET
        << StateTracker::Model::Action::FREE;

    d_ptr->_test_validateModelAboutToReplace();

    d_ptr->m_pModel = m;

    if (m)
        d_ptr->m_pModelTracker->forcePaused(); //HACK

    if (!m)
        return;
}

StateTracker::Index *TreeTraversalReflectorPrivate::lastItem() const
{
    auto candidate = m_pRoot->lastChild();

    while (candidate && candidate->lastChild())
        candidate = candidate->lastChild();

    return candidate;
}

bool TreeTraversalReflectorPrivate::isInsertActive(const QModelIndex& p, int first, int last) const
{
    Q_UNUSED(last) //TODO
    auto pitem = p.isValid() ? m_hMapper.value(p) : m_pRoot;

    StateTracker::Index *prev(nullptr);

    //FIXME use up()
    if (first && pitem)
        prev = pitem->childrenLookup(q_ptr->model()->index(first-1, 0, p));

    if (first && !prev)
        return false;

    if (edges(EdgeType::FREE)->m_Edges & (Qt::TopEdge|Qt::BottomEdge))
        return true;

    const auto parent = p.isValid() ? ttiForIndex(p) : m_pRoot;

    // It's controversial as it means if the items would otherwise be
    // visible because their parent "virtual" position + insertion height
    // could happen to be in the view but in this case the scrollbar would
    // move. Mobile views tend to not shuffle the current content around so
    // from that point of view it is correct, but if the item is in the
    // buffer, then it will move so doing this is a bit buggy.
    return parent != nullptr;
}

/// Return true if the indices affect the current view
bool TreeTraversalReflector::isActive(const QModelIndex& parent, int first, int last)
{
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
            d_ptr->m_pRoot->lastChild ()->index().column() + 1,
            d_ptr->m_pRoot->lastChild ()->index().row() + 1
        );

        const QRect changedRect(
            0,
            first,
            1, //FIXME use columnCount+!?
            last
        );

        return loadedRect.intersects(changedRect);
    }

//FIXME This method is incomplete
//     Q_ASSERT(false); //TODO THIS_COMMIT

    return true;
}

/// Add new entries to the mapping
StateTracker::ModelItem* TreeTraversalReflectorPrivate::addChildren(const QModelIndex& index)
{
    Q_ASSERT(index.isValid() && !m_hMapper.contains(index));

    auto e = new StateTracker::ModelItem(this);
    e->setModelIndex(index);

    m_hMapper[index] = e;

    return e;
}

void TreeTraversalReflectorPrivate::cleanup()
{
    // The whole cleanup cycle isn't necessary, it wont find anything.
    if (!m_pRoot->firstChild())
        return;

    m_pModelTracker << StateTracker::Model::Action::RESET;

    m_pRoot->metadata()
        << IndexMetadata::LoadAction::HIDE
        << IndexMetadata::LoadAction::DETACH;

    m_hMapper.clear();
    m_pRoot = new StateTracker::ModelItem(this);
}

StateTracker::ModelItem* TreeTraversalReflectorPrivate::ttiForIndex(const QModelIndex& idx) const
{
    if (!idx.isValid())
        return nullptr;

    const auto parent = (!idx.parent().isValid()) ?
        m_pRoot : m_hMapper.value(idx.parent());

    return parent ?
        static_cast<StateTracker::ModelItem*>(parent->childrenLookup(idx)) : nullptr;
}

AbstractItemAdapter* TreeTraversalReflector::itemForIndex(const QModelIndex& idx) const
{
    const auto tti = d_ptr->ttiForIndex(idx);

    return (tti && tti->metadata()->viewTracker()) ?
        tti->metadata()->viewTracker()->d_ptr : nullptr;
}

IndexMetadata *TreeTraversalReflector::geometryForIndex(const QModelIndex& idx) const
{
    const auto tti = d_ptr->ttiForIndex(idx);

    return tti ? tti->metadata() : nullptr;
}

void TreeTraversalReflector::setAvailableEdges(Qt::Edges edges, EdgeType t)
{
    d_ptr->edges(t)->m_Edges = edges;
}

Qt::Edges TreeTraversalReflector::availableEdges(EdgeType  t) const
{
    return d_ptr->edges(t)->m_Edges;
}

IndexMetadata *TreeTraversalReflector::getEdge(EdgeType t, Qt::Edge e) const
{
    const auto ret = d_ptr->edges(t)->getEdge(e);

    return ret ? ret->metadata() : nullptr;
}

ModelRect* TreeTraversalReflectorPrivate::edges(EdgeType e) const
{
    Q_ASSERT((int) e >= 0 && (int)e <= 2);
    return (ModelRect*) &m_lRects[(int)e]; //FIXME use reference, not ptr
}

// Add more validation to detect possible invalid edges being set
void TreeTraversalReflectorPrivate::
setEdge(EdgeType et, StateTracker::Index* tti, Qt::Edge e)
{
//DEBUG this only works with proxy models, not JIT/AOT strategies
//     if (tti && et == EdgeType::VISIBLE)
//         Q_ASSERT(TTI(tti)->metadata()->isValid());

    edges(et)->setEdge(tti, e);

    _test_validate_edges_simple();
}

StateTracker::Index *TreeTraversalReflectorPrivate::
find(StateTracker::Index *from, Qt::Edge direction, std::function<bool(StateTracker::Index *i)> cond) const
{
    auto f = from;
    while(f && cond(f) && f->next(direction) && (f = f->next(direction)));
    return f;
}

StateTracker::Model *TreeTraversalReflector::modelTracker() const
{
    return d_ptr->m_pModelTracker;
}

QModelIndex TreeTraversalReflectorPrivate::getNextIndex(const QModelIndex& idx) const
{
    // There is 2 possibilities, a sibling or a [[great]grand]uncle

    if (m_pModel->rowCount(idx))
        return m_pModel->index(0,0, idx);

    auto sib = idx.siblingAtRow(idx.row()+1);

    if (sib.isValid())
        return sib;

    if (!idx.parent().isValid())
        return {};

    auto p = idx.parent();

    while (p.isValid()) {
        sib = p.siblingAtRow(p.row()+1);
        if (sib.isValid())
            return sib;

        p = p.parent();
    }

    return {};
}

#include <treetraversalreflector_p.moc>
