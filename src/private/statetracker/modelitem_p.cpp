/***************************************************************************
 *   Copyright (C) 2018 by Emmanuel Lepage Vallee                          *
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
#include "modelitem_p.h"

#include "proximity_p.h"
#include <private/treetraversalreflector_p.h>
#include <viewport.h>
#include <private/viewport_p.h>
#include <private/statetracker/viewitem_p.h>

#define S StateTracker::ModelItem::State::
const StateTracker::ModelItem::State StateTracker::ModelItem::m_fStateMap[8][8] = {
/*                 SHOW         HIDE        ATTACH     DETACH      UPDATE       MOVE         RESET     REPARENT */
/*NEW      */ { S ERROR  , S ERROR    , S REACHABLE, S ERROR    , S ERROR  , S ERROR    , S ERROR    , S ERROR  },
/*BUFFER   */ { S VISIBLE, S BUFFER   , S BUFFER   , S DANGLING , S BUFFER , S BUFFER   , S BUFFER   , S MOVING },
/*REMOVED  */ { S ERROR  , S ERROR    , S ERROR    , S REACHABLE, S ERROR  , S ERROR    , S ERROR    , S MOVING },
/*REACHABLE*/ { S VISIBLE, S REACHABLE, S ERROR    , S REACHABLE, S ERROR  , S REACHABLE, S REACHABLE, S MOVING },
/*VISIBLE  */ { S VISIBLE, S BUFFER   , S ERROR    , S ERROR    , S VISIBLE, S VISIBLE  , S VISIBLE  , S MOVING },
/*ERROR    */ { S ERROR  , S ERROR    , S ERROR    , S ERROR    , S ERROR  , S ERROR    , S ERROR    , S ERROR  },
/*DANGLING */ { S ERROR  , S ERROR    , S ERROR    , S ERROR    , S ERROR  , S ERROR    , S ERROR    , S ERROR  },
/*MOVING   */ { S VISIBLE, S ERROR    , S ERROR    , S ERROR    , S ERROR  , S VISIBLE  , S ERROR    , S BUFFER },
};
#undef S

#define A &StateTracker::ModelItem::
const StateTracker::ModelItem::StateF StateTracker::ModelItem::m_fStateMachine[8][8] = {
/*                 SHOW       HIDE      ATTACH     DETACH      UPDATE     MOVE      RESET    REPARENT */
/*NEW      */ { A error  , A nothing, A nothing, A error   , A error  , A error  , A error, A error   },
/*BUFFER   */ { A show   , A remove2, A attach , A destroy , A refresh, A move   , A reset, A nothing },
/*REMOVED  */ { A error  , A error  , A error  , A detach  , A error  , A error  , A reset, A nothing },
/*REACHABLE*/ { A show   , A nothing, A error  , A detach  , A error  , A move   , A reset, A nothing },
/*VISIBLE  */ { A nothing, A hide   , A error  , A error   , A refresh, A move   , A reset, A nothing },
/*ERROR    */ { A error  , A error  , A error  , A error   , A error  , A error  , A error, A error   },
/*DANGLING */ { A error  , A error  , A error  , A error   , A error  , A error  , A error, A error   },
/*MOVING   */ { A move   , A error  , A error  , A error   , A error  , A nothing, A error, A nothing },
};
#undef A


StateTracker::ModelItem::ModelItem(TreeTraversalReflector* q):
  StateTracker::Index(q->viewport()), q_ptr(q)
{}

StateTracker::ModelItem* StateTracker::ModelItem::load(Qt::Edge e) const
{
    // First, if it's already loaded, then don't bother
    if (auto ret = next(e))
        return ret->metadata()->modelTracker();

    const auto t = metadata()->proximityTracker();
    const QModelIndexList l = t->getNext(e);

    // The consumer of this function only cares about the item above `this`,
    // not all the items that were loaded because they are dependencies.
    StateTracker::Index *i = nullptr;

    for (const QModelIndex& idx : qAsConst(l)) {
        Q_ASSERT(idx.isValid());
        q_ptr->forceInsert(idx);
    }

    // This works because only the TopEdge and LeftEdge can return multiple `l`
    i = next(e);

    _DO_TEST(_test_validateViewport, q_ptr);

    return i ? i->metadata()->modelTracker() : nullptr;
}

/**
 * When the item is moved, the old loading state is worthless.
 *
 * Recompute it based on the siblings.
 */
void StateTracker::ModelItem::rebuildState()
{
    static const auto VISIBLE = StateTracker::ModelItem::State::VISIBLE;

    //TODO make a 3D matrix out of this
    auto u = up  () ? up  ()->metadata()->modelTracker() : nullptr;
    auto d = down() ? down()->metadata()->modelTracker() : nullptr;

    const bool betweenVis = u && d && u->state() == VISIBLE && d->state() == VISIBLE;
    const bool nearVisible = ((!u) && d && d->state() == VISIBLE) ||
        ((!d) && u && u->state() == VISIBLE);

    if (betweenVis || nearVisible)
        m_State = StateTracker::ModelItem::State::VISIBLE;
    else
        Q_ASSERT(false); //TODO not implemented
}

IndexMetadata::EdgeType StateTracker::ModelItem::isTopEdge() const
{
    //FIXME use the ModelItem cache

    const auto r = q_ptr->viewport()->s_ptr->m_pReflector;

    if (r->getEdge(IndexMetadata::EdgeType::VISIBLE, Qt::TopEdge)) {
        return IndexMetadata::EdgeType::VISIBLE;
    }

    if (r->getEdge(IndexMetadata::EdgeType::VISIBLE, Qt::TopEdge)) {
        return IndexMetadata::EdgeType::BUFFERED;
    }

    return IndexMetadata::EdgeType::NONE;
}

IndexMetadata::EdgeType StateTracker::ModelItem::isBottomEdge() const
{
    //FIXME use the ModelItem cache

    const auto r = q_ptr->viewport()->s_ptr->m_pReflector;

    if (r->getEdge(IndexMetadata::EdgeType::VISIBLE, Qt::BottomEdge)) {
        return IndexMetadata::EdgeType::VISIBLE;
    }

    if (r->getEdge(IndexMetadata::EdgeType::VISIBLE, Qt::BottomEdge)) {
        return IndexMetadata::EdgeType::BUFFERED;
    }

    return IndexMetadata::EdgeType::NONE;
}

StateTracker::ModelItem::State StateTracker::ModelItem::state() const
{
    return m_State;
}

void StateTracker::ModelItem::remove(bool reparent)
{
    // First, update the edges
    const auto te = isTopEdge();
    const auto be = isBottomEdge();

    //TODO turn this into a state tracker, really
    switch(te) {
        case IndexMetadata::EdgeType::NONE:
            break;
        case IndexMetadata::EdgeType::FREE:
            Q_ASSERT(false); // Impossible, only corrupted memory can cause this
            break;
        case IndexMetadata::EdgeType::VISIBLE:
            if (auto i = q_ptr->find(this, Qt::TopEdge, [](auto i) -> bool {
              return i->metadata()->modelTracker()->m_State == StateTracker::ModelItem::State::VISIBLE; } )) {
                Q_ASSERT(i->metadata()->modelTracker()->m_State == StateTracker::ModelItem::State::VISIBLE); //TODO
                q_ptr->setEdge(IndexMetadata::EdgeType::VISIBLE, i, Qt::TopEdge);
            }
            else {
                Q_ASSERT(false); //TODO clear the edges
            }
            break;
        case IndexMetadata::EdgeType::BUFFERED:
            Q_ASSERT(false); //TODO as of the time of this comment, it's hald implemented
            break;
    }

    switch(be) {
        case IndexMetadata::EdgeType::NONE:
            break;
        case IndexMetadata::EdgeType::FREE:
            Q_ASSERT(false); // Impossible, only corrupted memory can cause this
            break;
        case IndexMetadata::EdgeType::VISIBLE:
            if (auto i = q_ptr->find(this, Qt::BottomEdge, [](auto i) -> bool {
              return i->metadata()->modelTracker()->m_State == StateTracker::ModelItem::State::VISIBLE; } )) {
                Q_ASSERT(i->metadata()->modelTracker()->m_State == StateTracker::ModelItem::State::VISIBLE); //TODO
                q_ptr->setEdge(IndexMetadata::EdgeType::VISIBLE, i, Qt::BottomEdge);
            }
            else {
                Q_ASSERT(false); //TODO clear the edges
            }
            break;
        case IndexMetadata::EdgeType::BUFFERED:
            Q_ASSERT(false); //TODO as of the time of this comment, it's hald implemented
            break;
    }

    q_ptr->viewport()->s_ptr->notifyRemoval(metadata());
    metadata() << IndexMetadata::LoadAction::REPARENT;
    Index::remove(reparent);
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

    if (auto item = metadata()->viewTracker()) {
        //TODO Implementing this *may* make sense, but for now avoid it.
        // for now it is undefined behavior, but not fatal
        Q_ASSERT(false); //TODO not well thought, most definitely broken
        item->setVisible(true);
    }
    else {
        metadata()->setViewTracker(q_ptr->itemFactory()()->s_ptr);
        Q_ASSERT(metadata()->viewTracker());
        metadata()->viewTracker()->m_pTTI = this;

        metadata() << IndexMetadata::ViewAction::ATTACH;
        Q_ASSERT(metadata()->viewTracker()->m_State == StateTracker::ViewItem::State::POOLED);
        Q_ASSERT(m_State == State::VISIBLE);

        //DEBUG
        //if (auto item = metadata()->viewTracker()->item())
        //    qDebug() << "CREATE" << this << item->y() << item->height();
    }

    metadata() << IndexMetadata::ViewAction::ENTER_BUFFER;

    // Make sure no `performAction` loop caused the item to get out of view
    Q_ASSERT(m_State == State::VISIBLE);

    // When the delegate fails to load (or there is none), there is nothing to do
    if (metadata()->viewTracker()->m_State == StateTracker::ViewItem::State::FAILED)
        return false;

    Q_ASSERT(metadata()->viewTracker()->m_State == StateTracker::ViewItem::State::BUFFER);

    metadata() << IndexMetadata::ViewAction::ENTER_VIEW;

    // Make sure no `performAction` loop caused the item to get out of view
    Q_ASSERT(m_State == State::VISIBLE);

    q_ptr->viewport()->s_ptr->updateGeometry(metadata());

    // For some reason creating the visual element failed, this can and will
    // happen and need to be recovered from.
    if (metadata()->viewTracker()->hasFailed()) {
        metadata() << IndexMetadata::ViewAction::LEAVE_BUFFER;

        // Make sure no `performAction` loop caused the item to get out of view
        Q_ASSERT(m_State == State::VISIBLE);

        metadata()->setViewTracker(nullptr);
    }

    //FIXME do this less often
    // This has to be done after `setViewTracker` and performAction
    if (down() && down()->metadata()->isValid())
        q_ptr->viewport()->s_ptr->notifyInsert(down()->metadata());

    // Make sure no `performAction` loop caused the item to get out of view
    Q_ASSERT(m_State == State::VISIBLE);

    // Update the edges
    if (m_State == State::VISIBLE) {
        auto first = q_ptr->edges(IndexMetadata::EdgeType::VISIBLE)->getEdge(Qt::TopEdge);
        auto prev  = up();
        auto next  = down();
        auto last  = q_ptr->edges(IndexMetadata::EdgeType::VISIBLE)->getEdge(Qt::BottomEdge);

        // Make sure the geometry is up to data
        metadata()->decoratedGeometry();
        Q_ASSERT(metadata()->isValid());

        if (first == next) {
            Q_ASSERT((!up()) || (up()->metadata()->modelTracker()->m_State != State::VISIBLE));
            q_ptr->setEdge(IndexMetadata::EdgeType::VISIBLE, this, Qt::TopEdge);
        }

        if (prev == last) {
            Q_ASSERT((!down()) || (down()->metadata()->modelTracker()->m_State != State::VISIBLE));
            q_ptr->setEdge(IndexMetadata::EdgeType::VISIBLE, this, Qt::BottomEdge);
        }

    }
    else
        Q_ASSERT(false); //TODO handle failed elements

    //d_ptr->_test_validate_geometry_cache(); //TODO THIS_COMMIT
    //Q_ASSERT(metadata()->isInSync()); //TODO THIS_COMMIT

    _DO_TEST(_test_validateViewport, q_ptr)

    return true;
}

bool StateTracker::ModelItem::hide()
{
    if (!metadata()->viewTracker())
        return true;

    // This needs more reflection, is using "visible" instead of freeing
    // memory a good idea?
    //item->setVisible(false);
    metadata() << IndexMetadata::ViewAction::LEAVE_BUFFER;

    return true;
}

bool StateTracker::ModelItem::remove2()
{
    if (!metadata()->viewTracker())
        return true;

    Q_ASSERT(metadata()->viewTracker()->m_State != StateTracker::ViewItem::State::POOLED  );
    Q_ASSERT(metadata()->viewTracker()->m_State != StateTracker::ViewItem::State::POOLING );
    Q_ASSERT(metadata()->viewTracker()->m_State != StateTracker::ViewItem::State::DANGLING);

    // Move the item lifecycle forward
    while (metadata()->viewTracker()->m_State != StateTracker::ViewItem::State::POOLED
        && metadata()->viewTracker()->m_State != StateTracker::ViewItem::State::DANGLING)
        metadata() << IndexMetadata::ViewAction::DETACH;

    // It should still exists, it may crash otherwise, so make sure early
    Q_ASSERT(metadata()->viewTracker()->m_State == StateTracker::ViewItem::State::POOLED
        || metadata()->viewTracker()->m_State == StateTracker::ViewItem::State::DANGLING
    );

    metadata()->setViewTracker(nullptr);

    return true;
}

bool StateTracker::ModelItem::attach()
{
    Q_ASSERT(m_State != State::VISIBLE && !metadata()->viewTracker());
    _DO_TEST(_test_validate_edges_simple, q_ptr)

    //TODO For now the buffer isn't fully implemented, so items always get
    // shown when attached.

    //FIXME this ain't correct
    return metadata() << IndexMetadata::LoadAction::SHOW;
}

bool StateTracker::ModelItem::detach()
{
    const auto children = allLoadedChildren();

    // First, detach any remaining children
    for (auto i : qAsConst(children)) {
        i->metadata()
            << IndexMetadata::LoadAction::HIDE
            << IndexMetadata::LoadAction::DETACH;
    }

    remove2();
    StateTracker::Index::remove();

    q_ptr->viewport()->s_ptr->refreshVisible();

    Q_ASSERT(!loadedChildrenCount() && ((!parent()) || !parent()->childrenLookup(index())));
    Q_ASSERT(!metadata()->viewTracker());

    return true;
}

bool StateTracker::ModelItem::refresh()
{
    q_ptr->viewport()->s_ptr->updateGeometry(metadata());

    for (auto i = firstChild(); i; i = i->nextSibling()) {
        Q_ASSERT(i != this);
        i->metadata() << IndexMetadata::LoadAction::UPDATE;

        if (i == lastChild())
            break;
    }

    // If this isn't true then the state machine is broken
    Q_ASSERT(metadata()->viewTracker());

    return true;
}

bool StateTracker::ModelItem::move()
{
    //FIXME Currently this is O(N^2) because refreshVisible does it too

    // Propagate to all children
    for (auto i = firstChild(); i; i = i->nextSibling()) {
        Q_ASSERT(i != this);
        i->metadata() << IndexMetadata::LoadAction::MOVE;

        if (i == lastChild())
            break;
    }

    if (metadata()->viewTracker()) {
        metadata() << IndexMetadata::ViewAction::MOVE;
        //Q_ASSERT(metadata()->isInSync());//TODO THIS_COMMIT
    }

    Q_ASSERT(metadata()->isValid());

    return true;
}

bool StateTracker::ModelItem::destroy()
{
    Q_ASSERT(parent() || this == q_ptr->root());
    Q_ASSERT((!parent()) || parent()->hasChildren(this));

    detach();

    Q_ASSERT((!metadata()->viewTracker()) && !loadedChildrenCount());

    delete this;
    return true;
}

bool StateTracker::ModelItem::reset()
{
    for (auto i = firstChild(); i; i = i->nextSibling()) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->metadata() << IndexMetadata::LoadAction::RESET;

        if (i == lastChild())
            break;
    }

    if (metadata()->viewTracker()) {
        metadata()
            << IndexMetadata::ViewAction::LEAVE_BUFFER
            << IndexMetadata::ViewAction::DETACH;

        metadata()->setViewTracker(nullptr);
    }

    metadata() << IndexMetadata::GeometryAction::RESET;

    return true;
}
