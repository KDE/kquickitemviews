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
#include "content_p.h"

// KQuickItemViews
#include "viewitem_p.h"
#include "proximity_p.h"
#include "model_p.h"
#include "modelitem_p.h"
#include <viewport.h>
#include <private/viewport_p.h>
#include <private/indexmetadata_p.h>
#include <private/statetracker/continuity_p.h>
#include <adapters/contextadapter.h>

// Qt
#include <QtCore/QDebug>

using EdgeType = IndexMetadata::EdgeType;

namespace StateTracker {
class Index;
class Model;
class Content;
}

class AbstractItemAdapter;
class ModelRect;
#include <private/indexmetadata_p.h>
#include <private/statetracker/modelitem_p.h>

class ContentPrivate : public QObject
{
    Q_OBJECT
public:
    explicit ContentPrivate();

    // Helpers
    StateTracker::ModelItem* addChildren(const QModelIndex& index);
    StateTracker::ModelItem* ttiForIndex(const QModelIndex& idx) const;

    bool isInsertActive(const QModelIndex& p, int first, int last) const;

    QList<StateTracker::Index*> setTemporaryIndices(const QModelIndex &parent, int start, int end,
                             const QModelIndex &destination, int row);
    void resetTemporaryIndices(const QList<StateTracker::Index*>&);

    void reloadEdges();

    StateTracker::ModelItem *m_pRoot {nullptr};

    //TODO add a circular buffer to GC the items
    // * relative index: when to trigger GC
    // * absolute array index: kept in the StateTracker::ModelItem so it can
    //   remove itself

    /// All elements with loaded children
    QHash<QPersistentModelIndex, StateTracker::ModelItem*> m_hMapper;

    QModelIndex getNextIndex(const QModelIndex& idx) const;

    ModelRect m_lRects[3];
    StateTracker::Model *m_pModelTracker;
    Viewport            *m_pViewport;

    StateTracker::Content* q_ptr;

    // Update the ModelRect
    void insertEdge(IndexMetadata *, StateTracker::ModelItem::State);
    void removeEdge(IndexMetadata *, StateTracker::ModelItem::State);
    void enterState(IndexMetadata *, StateTracker::ModelItem::State);
    void leaveState(IndexMetadata *, StateTracker::ModelItem::State);
    void error     (IndexMetadata *, StateTracker::ModelItem::State);
    void resetState(IndexMetadata *, StateTracker::ModelItem::State);

    typedef void(ContentPrivate::*StateFS)(IndexMetadata*, StateTracker::ModelItem::State);
    static const StateFS m_fStateLogging[8][2];

public Q_SLOTS:
    void slotCleanup();
    void slotRowsInserted  (const QModelIndex& parent, int first, int last);
    void slotRowsRemoved   (const QModelIndex& parent, int first, int last);
    void slotLayoutChanged (                                              );
    void slotDataChanged   (const QModelIndex& tl, const QModelIndex& br,
                            const QVector<int> &roles  );
    void slotRowsMoved     (const QModelIndex &p, int start, int end,
                            const QModelIndex &dest, int row);
};

#define A &ContentPrivate::
// Keep track of the number of instances per state
const ContentPrivate::StateFS ContentPrivate::m_fStateLogging[8][2] = {
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

void ContentPrivate::insertEdge(IndexMetadata *md, StateTracker::ModelItem::State s)
{
    auto tti = md->modelTracker();
    Q_UNUSED(s)
    const auto first = q_ptr->edges(EdgeType::VISIBLE)->getEdge(  Qt::TopEdge   );
    const auto last  = q_ptr->edges(EdgeType::VISIBLE)->getEdge( Qt::BottomEdge );

    const auto prev = tti->up();
    const auto next = tti->down();

    if (first == next)
        q_ptr->setEdge(EdgeType::VISIBLE, tti, Qt::TopEdge);

    if (last == prev)
        q_ptr->setEdge(EdgeType::VISIBLE, tti, Qt::BottomEdge);

    _DO_TEST(_test_validate_edges_simple, q_ptr)
}

void ContentPrivate::removeEdge(IndexMetadata *md, StateTracker::ModelItem::State s)
{
    Q_UNUSED(s)
    auto tti = md->modelTracker();
    const auto first = q_ptr->edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge);
    const auto last  = q_ptr->edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge);

    // The item was somewhere in between, not an edge case
    if ((tti != first) && (tti != last))
        return;

    auto prev = tti->up();
    auto next = tti->down();

    if (tti == first)
        q_ptr->setEdge(EdgeType::VISIBLE,
            (next && next->metadata()->modelTracker()->state() == StateTracker::ModelItem::State::VISIBLE) ?
                next : nullptr,
            Qt::TopEdge
        );

    if (tti == last)
        q_ptr->setEdge(EdgeType::VISIBLE,
            (prev && prev->metadata()->modelTracker()->state() == StateTracker::ModelItem::State::VISIBLE) ?
                prev : nullptr,
            Qt::BottomEdge
        );

    _DO_TEST(_test_validate_edges_simple, q_ptr)
}

/**
 * This method is called when an item moves into a new area and the "real"
 * new state needs to be determined.
 */
void ContentPrivate::resetState(IndexMetadata *i, StateTracker::ModelItem::State)
{
    i->modelTracker()->rebuildState();
}

ContentPrivate::ContentPrivate()
{}

StateTracker::Content::Content(Viewport* parent) : QObject(parent),
    d_ptr(new ContentPrivate())
{
    d_ptr->m_pViewport     = parent;
    d_ptr->m_pRoot         = new StateTracker::ModelItem(parent);
    d_ptr->m_pModelTracker = new StateTracker::Model(this);
    d_ptr->q_ptr           = this;
}

StateTracker::Content::~Content()
{
    delete d_ptr->m_pModelTracker;
    d_ptr->m_pModelTracker = nullptr;
    delete d_ptr->m_pRoot;
}

void ContentPrivate::slotRowsInserted(const QModelIndex& parent, int first, int last)
{
    // This method uses modelCandidate() because it needs to be called during
    // initilization (thus not always directly by QAbstractItemModel::rowsInserted

    Q_ASSERT(((!parent.isValid()) || parent.model() == m_pModelTracker->modelCandidate()) && first <= last);

    // It is the job of isInsertActive to decide what's correct
    if (!isInsertActive(parent, first, last)) {
        _DO_TEST(_test_validateLinkedList, q_ptr)
        return;
    }

    const auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;

    //FIXME it is possible if the anchor is at the bottom that the parent
    // needs to be loaded. But this is currently too not supported.
    if (!pitem) {
        _DO_TEST(_test_validateLinkedList, q_ptr)
        return;
    }

    StateTracker::Index *prev = nullptr;

    //FIXME use up()
    if (first && pitem)
        prev = pitem->childrenLookup(m_pModelTracker->modelCandidate()->index(first-1, 0, parent));

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
        auto idx = m_pModelTracker->modelCandidate()->index(i, 0, parent);
        Q_ASSERT(idx.isValid() && idx.parent() != idx && idx.model() == m_pModelTracker->modelCandidate());

        // If the insertion is sandwiched between loaded items, not doing it
        // will corrupt the view, but if it's a "tail" insertion, then they
        // can be discarded.
        if (!(q_ptr->edges(EdgeType::FREE)->m_Edges & (Qt::BottomEdge | Qt::TopEdge))) {
            const auto nextIdx = getNextIndex(idx);
            const auto nextTTI = nextIdx.isValid() ? ttiForIndex(nextIdx) : nullptr;

            if (!nextTTI) {
                m_pViewport->s_ptr->refreshVisible();
                _DO_TEST(_test_validateLinkedList, q_ptr)
                return; //FIXME break
            }
        }

        auto e = addChildren(idx);

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
            _DO_TEST_IDX(_test_validate_chain, prev->parent())
        }

        // This is required before ::ATTACH because otherwise ::down() wont work

        Q_ASSERT((!pitem->firstChild()) || !pitem->firstChild()->hasTemporaryIndex()); //TODO

        const bool needDownMove = (!pitem->lastChild()) ||
            e->effectiveRow() > pitem->lastChild()->effectiveRow();

        const bool needUpMove = (!pitem->firstChild()) ||
            e->effectiveRow() < pitem->firstChild()->effectiveRow();

        _DO_TEST_IDX(_test_validate_chain, e->parent())

        // The item is about the current parent first item
        if (needUpMove) {
//             Q_ASSERT(false); //TODO merge with the other bridgeGap above

            m_pViewport->s_ptr->notifyInsert(m_pRoot->firstChild()->metadata());
            Q_ASSERT((!m_pRoot->firstChild()) || !m_pRoot->firstChild()->metadata()->isValid());
            StateTracker::Index::insertChildBefore(e, nullptr, pitem);
        }

        _DO_TEST_IDX(_test_validate_chain, e->parent())

        Q_ASSERT(e->metadata()->geometryTracker()->state() == StateTracker::Geometry::State::INIT);
        Q_ASSERT(e->state() != StateTracker::ModelItem::State::VISIBLE);

        // NEW -> REACHABLE, this should never fail
        if (!e->metadata()->performAction(IndexMetadata::LoadAction::ATTACH)) {
            qDebug() << "\n\nATTACH FAILED";
            _DO_TEST(_test_validateLinkedList, q_ptr)
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

        int rc = m_pModelTracker->modelCandidate()->rowCount(idx);
        if (rc && q_ptr->edges(EdgeType::FREE)->m_Edges & Qt::BottomEdge) {
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

    _DO_TEST(_test_validateLinkedList, q_ptr)
    _DO_TEST_IDX(_test_validate_chain, pitem)

    Q_EMIT q_ptr->contentChanged();
}

void ContentPrivate::slotRowsRemoved(const QModelIndex& parent, int first, int last)
{
    Q_ASSERT((!parent.isValid()) || parent.model() == m_pModelTracker->modelCandidate());

    if (!q_ptr->isActive(parent, first, last)) {
        _DO_TEST(_test_validateLinkedList, q_ptr)
        _DO_TEST(_test_validateUnloaded, q_ptr, parent, first, last)
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
        auto idx = m_pModelTracker->modelCandidate()->index(i, 0, parent);

        if (auto elem = pitem->childrenLookup(idx)) {
            elem->metadata()
                << IndexMetadata::LoadAction::HIDE
                << IndexMetadata::LoadAction::DETACH;
        }
    }

    Q_EMIT q_ptr->contentChanged();
}

//TODO optimize this
void ContentPrivate::slotDataChanged(const QModelIndex& tl, const QModelIndex& br, const QVector<int> &roles)
{
    Q_ASSERT(((!tl.isValid()) || tl.model() == m_pModelTracker->modelCandidate()));
    Q_ASSERT(((!br.isValid()) || br.model() == m_pModelTracker->modelCandidate()));

    if (!q_ptr->isActive(tl.parent(), tl.row(), br.row()))
        return;

    for (int i = tl.row(); i <= br.row(); i++) {
        const auto idx = m_pModelTracker->modelCandidate()->index(i, tl.column(), tl.parent());
        if (auto tti = ttiForIndex(idx)) {
            if (tti->metadata()->viewTracker()) {
                tti->metadata()->contextAdapter()->updateRoles(roles);
                tti->metadata() << IndexMetadata::ViewAction::UPDATE;
            }
        }
    }
}

void ContentPrivate::slotLayoutChanged()
{
    if (auto rc = m_pModelTracker->modelCandidate()->rowCount())
        slotRowsInserted({}, 0, rc - 1);

    Q_EMIT q_ptr->contentChanged();
}

void ContentPrivate::slotRowsMoved(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    Q_ASSERT((!parent.isValid()) || parent.model() == m_pModelTracker->modelCandidate());
    Q_ASSERT((!destination.isValid()) || destination.model() == m_pModelTracker->modelCandidate());

    // There is literally nothing to do
    if (parent == destination && start == row)
        return;

    // Whatever has to be done only affect a part that's not currently tracked.
    if (!q_ptr->isActive(destination, row, row)) {
        //Q_ASSERT(false); //TODO so I don't forget
        return;
    }

    auto tmp = setTemporaryIndices(parent, start, end, destination, row);

    // As the actual view is implemented as a daisy chained list, only moving
    // the edges is necessary for the StateTracker::ModelItem. Each StateTracker::ViewItem
    // need to be moved.

    const auto idxStart = m_pModelTracker->modelCandidate()->index(start, 0, parent);
    const auto idxEnd   = m_pModelTracker->modelCandidate()->index(end  , 0, parent);
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

    auto newNextIdx = m_pModelTracker->modelCandidate()->index(row, 0, destination);

    // You cannot move things into an empty model
    Q_ASSERT((!row) || newNextIdx.isValid());

    StateTracker::Index *newNextTTI(nullptr), *newPrevTTI(nullptr);

    // Rewind until a next element is found, this happens when destination is empty
    if (!newNextIdx.isValid() && destination.parent().isValid()) {
        Q_ASSERT(m_pModelTracker->modelCandidate()->rowCount(destination) == row);
        auto par = destination.parent();
        do {
            if (m_pModelTracker->modelCandidate()->rowCount(par.parent()) > par.row()) {
                newNextIdx = m_pModelTracker->modelCandidate()->index(par.row(), 0, par.parent());
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

    const auto topEdge    = q_ptr->edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge);
    const auto bottomEdge = q_ptr->edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge);

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

        _DO_TEST(_test_validate_edges_simple, q_ptr)
        m_pViewport->s_ptr->notifyInsert(item->metadata());

        dest = item;
        if (isRangeVisible) {
            if (item->metadata()->modelTracker()->state() == StateTracker::ModelItem::State::BUFFER)
                item->metadata() << IndexMetadata::LoadAction::ATTACH;

            item->metadata() << IndexMetadata::LoadAction::SHOW;
        }
    }

    _DO_TEST(_test_validate_move, q_ptr, newParentTTI, startTTI, endTTI, newPrevTTI, newNextTTI, row)

    resetTemporaryIndices(tmp);

    // The visible edges are now probably incorectly placed, reload them
    if (needRefreshVisibleTop || needRefreshVisibleBottom) {
        reloadEdges();
        m_pViewport->s_ptr->refreshVisible();
    }

    //WARNING The indices still are in transition mode, do not use their value
}

QList<StateTracker::Index*> ContentPrivate::setTemporaryIndices(const QModelIndex &parent, int start, int end,
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
        auto idx = m_pModelTracker->modelCandidate()->index(i, 0, parent);

        //TODO do not use the hashmap, it is already known
        auto elem = pitem->childrenLookup(idx);
        Q_ASSERT(elem);

        elem->setTemporaryIndex(
            destination, row + (i - start), idx.column()
        );
        ret << elem;
    }

    for (int i = row; i <= row + (end - start); i++) {
        auto idx = m_pModelTracker->modelCandidate()->index(i, 0, parent);

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

void ContentPrivate::resetTemporaryIndices(const QList<StateTracker::Index*>& indices)
{
    for (auto i : qAsConst(indices))
        i->resetTemporaryIndex();
}

void StateTracker::Content::resetEdges()
{
    for (int i = 0; i < (int) EdgeType::NONE; i++)
        for (const auto e : {Qt::BottomEdge, Qt::TopEdge, Qt::LeftEdge, Qt::RightEdge})
            setEdge((EdgeType)i, nullptr, e);
}

// Go O(N) for now. Optimize when it becomes a problem (read: soon)
void ContentPrivate::reloadEdges()
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
                q_ptr->setEdge(EdgeType::BUFFERED, item, Qt::TopEdge);
                break;
            case Range::TOP        | Range::IS_VISIBLE:
            case Range::BUFFER_TOP | Range::IS_VISIBLE:
                pos = Range::VISIBLE;
                q_ptr->setEdge(EdgeType::VISIBLE, item, Qt::TopEdge);
                break;
            case Range::VISIBLE | Range::IS_NOT_VISIBLE:
            case Range::VISIBLE | Range::IS_NOT_BUFFER:
                q_ptr->setEdge(EdgeType::VISIBLE, item->up(), Qt::BottomEdge);
                pos = Range::BOTTOM;
                break;
            case Range::VISIBLE | Range::IS_NOT_VISIBLE | Range::IS_BUFFER:
                pos = Range::BUFFER_BOTTOM;
                break;
            case Range::VISIBLE | Range::IS_NOT_BUFFER | Range::IS_NOT_VISIBLE:
                q_ptr->setEdge(EdgeType::BUFFERED, item->up(), Qt::BottomEdge);
                pos = Range::BOTTOM;
                break;
        }
    }
}

void ContentPrivate::enterState(IndexMetadata *tti, StateTracker::ModelItem::State s)
{
    Q_UNUSED(tti);
    Q_UNUSED(s);
    //TODO count the number of active objects in each states for the "frame" load balancing
}

void ContentPrivate::leaveState(IndexMetadata *tti, StateTracker::ModelItem::State s)
{
    Q_UNUSED(tti);
    Q_UNUSED(s);
    //TODO count the number of active objects in each states for the "frame" load balancing
}

void ContentPrivate::error(IndexMetadata *, StateTracker::ModelItem::State)
{
    Q_ASSERT(false);
}

StateTracker::Index *StateTracker::Content::firstItem() const
{
    return root()->firstChild();
}

StateTracker::Index *StateTracker::Content::lastItem() const
{
    auto candidate = d_ptr->m_pRoot->lastChild();

    while (candidate && candidate->lastChild())
        candidate = candidate->lastChild();

    return candidate;
}

bool ContentPrivate::isInsertActive(const QModelIndex& p, int first, int last) const
{
    Q_UNUSED(last) //TODO
    auto pitem = p.isValid() ? m_hMapper.value(p) : m_pRoot;

    StateTracker::Index *prev(nullptr);

    //FIXME use up()
    if (first && pitem)
        prev = pitem->childrenLookup(m_pModelTracker->modelCandidate()->index(first-1, 0, p));

    if (first && !prev)
        return false;

    if (q_ptr->edges(EdgeType::FREE)->m_Edges & (Qt::TopEdge|Qt::BottomEdge))
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
bool StateTracker::Content::isActive(const QModelIndex& parent, int first, int last)
{
    if (!parent.isValid()) {
        // There is nothing loaded, so everything is active (so far)
        if (!d_ptr->m_pRoot->firstChild())
            return true;

        // There is room for more. Assuming the code that makes sure all items
        // that can be loaded are, then it only happens when there is room for
        // more.
        if (edges(EdgeType::FREE)->m_Edges & (Qt::BottomEdge | Qt::TopEdge))
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
StateTracker::ModelItem* ContentPrivate::addChildren(const QModelIndex& index)
{
    Q_ASSERT(index.isValid() && !m_hMapper.contains(index));

    auto e = new StateTracker::ModelItem(m_pViewport);
    e->setModelIndex(index);

    m_hMapper[index] = e;

    return e;
}

void ContentPrivate::slotCleanup()
{
    // The whole slotCleanup cycle isn't necessary, it wont find anything.
    if (!m_pRoot->firstChild())
        return;

    m_pModelTracker << StateTracker::Model::Action::RESET;

    m_pRoot->metadata()
        << IndexMetadata::LoadAction::HIDE
        << IndexMetadata::LoadAction::DETACH;

    m_hMapper.clear();
    m_pRoot = new StateTracker::ModelItem(m_pViewport);

    // Reset the edges
    for (int i = 0; i < 3; i++)
        m_lRects[i] = ModelRect();
}

StateTracker::ModelItem* ContentPrivate::ttiForIndex(const QModelIndex& idx) const
{
    if (!idx.isValid())
        return nullptr;

    const auto parent = (!idx.parent().isValid()) ?
        m_pRoot : m_hMapper.value(idx.parent());

    return parent ?
        static_cast<StateTracker::ModelItem*>(parent->childrenLookup(idx)) : nullptr;
}

IndexMetadata *StateTracker::Content::metadataForIndex(const QModelIndex& idx) const
{
    const auto tti = d_ptr->ttiForIndex(idx);

    return tti ? tti->metadata() : nullptr;
}

void StateTracker::Content::setAvailableEdges(Qt::Edges es, EdgeType t)
{
    edges(t)->m_Edges = es;
}

Qt::Edges StateTracker::Content::availableEdges(EdgeType  t) const
{
    return edges(t)->m_Edges;
}

IndexMetadata *StateTracker::Content::getEdge(EdgeType t, Qt::Edge e) const
{
    const auto ret = edges(t)->getEdge(e);

    return ret ? ret->metadata() : nullptr;
}

ModelRect* StateTracker::Content::edges(EdgeType e) const
{
    Q_ASSERT((int) e >= 0 && (int)e <= 2);
    return (ModelRect*) &d_ptr->m_lRects[(int)e];
}

// Add more validation to detect possible invalid edges being set
void StateTracker::Content::
setEdge(EdgeType et, StateTracker::Index* tti, Qt::Edge e)
{
//DEBUG this only works with proxy models, not JIT/AOT strategies
//     if (tti && et == EdgeType::VISIBLE)
//         Q_ASSERT(TTI(tti)->metadata()->isValid());

    edges(et)->setEdge(tti, e);

    // Only test when `tti` is true because when removing the last item, it
    // will always need to remove both the top and bottom, so removing the
    // top will fail the test.
    if (tti) { _DO_TEST(_test_validate_edges_simple, this) }
}

StateTracker::Index *StateTracker::Content::
find(StateTracker::Index *from, Qt::Edge direction, std::function<bool(StateTracker::Index *i)> cond) const
{
    auto f = from;
    while(f && cond(f) && f->next(direction) && (f = f->next(direction)));
    return f;
}

StateTracker::Model *StateTracker::Content::modelTracker() const
{
    return d_ptr->m_pModelTracker;
}

//DEPRECATED use StateTracker::Proximity
QModelIndex ContentPrivate::getNextIndex(const QModelIndex& idx) const
{
    // There is 2 possibilities, a sibling or a [[great]grand]uncle

    if (m_pModelTracker->modelCandidate()->rowCount(idx))
        return m_pModelTracker->modelCandidate()->index(0,0, idx);

    auto sib = idx.sibling(idx.row()+1, idx.column());

    if (sib.isValid())
        return sib;

    if (!idx.parent().isValid())
        return {};

    auto p = idx.parent();

    while (p.isValid()) {
        sib = p.sibling(p.row()+1, p.column());
        if (sib.isValid())
            return sib;

        p = p.parent();
    }

    return {};
}

void StateTracker::Content::connectModel(QAbstractItemModel *m)
{
    QObject::connect(m, &QAbstractItemModel::rowsInserted, d_ptr,
        &ContentPrivate::slotRowsInserted );
    QObject::connect(m, &QAbstractItemModel::rowsAboutToBeRemoved, d_ptr,
        &ContentPrivate::slotRowsRemoved  );
    QObject::connect(m, &QAbstractItemModel::layoutAboutToBeChanged, d_ptr,
        &ContentPrivate::slotCleanup);
    QObject::connect(m, &QAbstractItemModel::layoutChanged, d_ptr,
        &ContentPrivate::slotLayoutChanged);
    QObject::connect(m, &QAbstractItemModel::modelAboutToBeReset, d_ptr,
        &ContentPrivate::slotCleanup);
    QObject::connect(m, &QAbstractItemModel::modelReset, d_ptr,
        &ContentPrivate::slotLayoutChanged);
    QObject::connect(m, &QAbstractItemModel::rowsAboutToBeMoved, d_ptr,
        &ContentPrivate::slotRowsMoved);
    QObject::connect(m, &QAbstractItemModel::dataChanged, d_ptr,
        &ContentPrivate::slotDataChanged);

#ifdef ENABLE_EXTRA_VALIDATION
    QObject::connect(m, &QAbstractItemModel::rowsMoved, d_ptr,
        [this](){_DO_TEST(_test_validateLinkedList, this);});
    QObject::connect(m, &QAbstractItemModel::rowsRemoved, d_ptr,
        [this](){_DO_TEST(_test_validateLinkedList, this);});
#endif
}

void StateTracker::Content::disconnectModel(QAbstractItemModel *m)
{
    QObject::disconnect(m, &QAbstractItemModel::rowsInserted, d_ptr,
        &ContentPrivate::slotRowsInserted);
    QObject::disconnect(m, &QAbstractItemModel::rowsAboutToBeRemoved, d_ptr,
        &ContentPrivate::slotRowsRemoved);
    QObject::disconnect(m, &QAbstractItemModel::layoutAboutToBeChanged, d_ptr,
        &ContentPrivate::slotCleanup);
    QObject::disconnect(m, &QAbstractItemModel::layoutChanged, d_ptr,
        &ContentPrivate::slotLayoutChanged);
    QObject::disconnect(m, &QAbstractItemModel::modelAboutToBeReset, d_ptr,
        &ContentPrivate::slotCleanup);
    QObject::disconnect(m, &QAbstractItemModel::modelReset, d_ptr,
        &ContentPrivate::slotLayoutChanged);
    QObject::disconnect(m, &QAbstractItemModel::rowsAboutToBeMoved, d_ptr,
        &ContentPrivate::slotRowsMoved);
    QObject::disconnect(m, &QAbstractItemModel::dataChanged, d_ptr,
        &ContentPrivate::slotDataChanged);

#ifdef ENABLE_EXTRA_VALIDATION
//     QObject::disconnect(m, &QAbstractItemModel::rowsMoved, d_ptr,
//         [this](){d_ptr->_test_validateLinkedList();});
//     QObject::disconnect(m, &QAbstractItemModel::rowsRemoved, d_ptr,
//         [this](){d_ptr->_test_validateLinkedList();});
#endif
}

StateTracker::Index *StateTracker::Content::root() const
{
    return d_ptr->m_pRoot;
}

void StateTracker::Content::resetRoot()
{
    root()->metadata() << IndexMetadata::LoadAction::RESET;

    for (int i = 0; i < 3; i++)
        d_ptr->m_lRects[i] = {};

    d_ptr->m_hMapper.clear();
    delete d_ptr->m_pRoot;
    d_ptr->m_pRoot = new StateTracker::ModelItem(d_ptr->m_pViewport);
}

void StateTracker::Content::perfromStateChange(Event e, IndexMetadata *md, StateTracker::ModelItem::State s)
{
    const auto ms = e == Event::LEAVE_STATE ? s : md->modelTracker()->state();
    (d_ptr->*ContentPrivate::m_fStateLogging[(int)ms][(int)e])(md, s);
}

void StateTracker::Content::forceInsert(const QModelIndex& idx)
{
    //TODO add some safety check or find a way to get rid of this hack
    d_ptr->slotRowsInserted(idx.parent(), idx.row(), idx.row());
}

// Also make sure not to load the same element twice
void StateTracker::Content::forceInsert(const QModelIndex& parent, int first, int last)
{
    auto parNode = parent.isValid() ? d_ptr->m_hMapper[parent] : d_ptr->m_pRoot;

    // If the parent isn't loaded, there is no risk of collision
    if (!parNode) {
        //TODO loading the parent chain of `parent` is needed
        d_ptr->slotRowsInserted(parent, first, last);
        return;
    }

    // If the parent has no children elements, then there is no collisions
    if (!parNode->firstChild()) {
        d_ptr->slotRowsInserted(parent, first, last);
        return;
    }

    // If the range isn't overlapping, there is no collisions
    if (parNode->lastChild()->effectiveRow() < first || parNode->firstChild()->effectiveRow() > last) {
        d_ptr->slotRowsInserted(parent, first, last);
        return;
    }

    // If there is a continuity encompassing `first` and `last` then there
    // is nothing to do.
    if (parNode->firstChild()->continuityTracker()->size() >= last) {
        return;
    }

    //TODO the case above only works if firstChild()->row() == 0. If another
    // case is hit, then it will assert in slotRowsInserted

    d_ptr->slotRowsInserted(parent, first, last);
}

#include <statetracker/content_p.moc>
