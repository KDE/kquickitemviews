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
#include "index_p.h"


// Use some constant for readability
#define PREVIOUS 0
#define NEXT 1
#define FIRST 0
#define LAST 1

StateTracker::Index::~Index()
{
    remove();
}

StateTracker::Index* StateTracker::Index::firstChild() const
{
    return m_tChildren[FIRST];
}

StateTracker::Index* StateTracker::Index::lastChild() const
{
    return m_tChildren[LAST];
}

StateTracker::Index* StateTracker::Index::nextSibling() const
{
    return m_tSiblings[NEXT];
}

StateTracker::Index* StateTracker::Index::previousSibling() const
{
    return m_tSiblings[PREVIOUS];
}

StateTracker::Index *StateTracker::Index::parent() const
{
    return m_pParent;
}

void StateTracker::Index::insertChildAfter(StateTracker::Index* self, StateTracker::Index* other, StateTracker::Index* parent)
{
    Q_ASSERT(!self->m_pParent);
    Q_ASSERT(parent);
    Q_ASSERT(self->m_LifeCycleState == LifeCycleState::NEW);
    Q_ASSERT(parent->firstChild() || !other);

    //Always detach first
    Q_ASSERT(!self->m_tSiblings[PREVIOUS]);
    Q_ASSERT(!self->m_tSiblings[NEXT]);


    if (!parent->m_tChildren[FIRST]) {
        Q_ASSERT(!parent->m_tChildren[LAST]);
        Q_ASSERT(!parent->loadedChildrenCount());
        parent->m_hLookup[self->m_Index] = self;
        parent->m_tChildren[FIRST] = parent->m_tChildren[LAST] = self;
        self->m_pParent = parent;
        self->m_LifeCycleState = self->m_MoveToRow != -1 ?
            LifeCycleState::TRANSITION : LifeCycleState::NORMAL;

        _DO_TEST_IDX(_test_validate_chain, parent)
        return;
    }

    Q_ASSERT(other);
    Q_ASSERT(other->m_pParent == parent);
    Q_ASSERT(!parent->m_hLookup.contains(self->m_Index));

    Q_ASSERT(!parent->m_hLookup.values().contains(self));
    parent->m_hLookup[self->m_Index] = self;
    self->m_pParent = parent;

    self->m_LifeCycleState = self->m_MoveToRow != -1 ?
        LifeCycleState::TRANSITION : LifeCycleState::NORMAL;

    self->m_tSiblings[NEXT] = other->m_tSiblings[NEXT];

    if (other->m_tSiblings[NEXT]) {
        Q_ASSERT(other->m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] == other);
        other->m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] = self;
    }

    other->m_tSiblings[NEXT] = self;
    self->m_tSiblings[PREVIOUS] = other;

    if (parent->m_tChildren[LAST] ==  other)
        parent->m_tChildren[LAST] = self;

    _DO_TEST_IDX(_test_validate_chain, parent)
}

void StateTracker::Index::insertChildBefore(StateTracker::Index* self, StateTracker::Index* other, StateTracker::Index* parent)
{
    Q_ASSERT(!self->m_pParent);
    Q_ASSERT(parent);
    Q_ASSERT(self->m_LifeCycleState == LifeCycleState::NEW);
    Q_ASSERT(!parent->m_hLookup.contains(self->m_Index));

    _DO_TEST_IDX(_test_validate_chain, parent)

    Q_ASSERT(!parent->m_hLookup.values().contains(self));
    parent->m_hLookup[self->m_Index] = self;
    self->m_pParent = parent;
    self->m_LifeCycleState = self->m_MoveToRow != -1 ?
        LifeCycleState::TRANSITION : LifeCycleState::NORMAL;

    if (!parent->firstChild()) {
        Q_ASSERT(!parent->lastChild());
        parent->m_tChildren[FIRST] = parent->m_tChildren[LAST] = self;
        self->m_tSiblings[PREVIOUS] = self->m_tSiblings[NEXT] = nullptr;
        Q_ASSERT(!self->nextSibling());
        Q_ASSERT(!self->previousSibling());
        _DO_TEST_IDX(_test_validate_chain, parent)
        return;
    }

//     Q_ASSERT(!self->nextSibling());
    Q_ASSERT(parent == self->m_pParent);

    if (!other) {
        if (auto f = parent->firstChild())
            f->m_tSiblings[PREVIOUS] = self;

        self->m_tSiblings[NEXT] = parent->firstChild();
        parent->m_tChildren[FIRST] = self;
        Q_ASSERT(!self->previousSibling());
        Q_ASSERT(parent->lastChild());

        _DO_TEST_IDX(_test_validate_chain, parent)
        return;
    }

    Q_ASSERT(other);
    Q_ASSERT(other->m_pParent == parent);
    Q_ASSERT(other->m_pParent == parent);

    if (other == parent->firstChild()) {
        Q_ASSERT(!other->previousSibling());
        Q_ASSERT(other->nextSibling() || other == parent->lastChild());
        parent->m_tChildren[FIRST] = self;
        self->m_tSiblings[NEXT] = other;
        self->m_tSiblings[PREVIOUS] = nullptr;
        other->m_tSiblings[PREVIOUS] = self;
        Q_ASSERT(!self->previousSibling());
        Q_ASSERT(parent->lastChild());
        Q_ASSERT(parent->firstChild());
    }
    else {
        Q_ASSERT(other->previousSibling());
        other->previousSibling()->m_tSiblings[NEXT] = self;
        self->m_tSiblings[PREVIOUS] = other->previousSibling();
        self->m_tSiblings[NEXT] = other;
        other->m_tSiblings[PREVIOUS] = self;
    }

    _DO_TEST_IDX(_test_validate_chain, parent)
}

/// Fix the issues introduced by createGap (does not update m_pParent and m_hLookup)
void StateTracker::Index::bridgeGap(StateTracker::Index* first, StateTracker::Index* second)
{
    // 3 possible case: siblings, first child or last child

    if (first && second && first->m_pParent == second->m_pParent) {
        // first and second are siblings

        if (first == first->m_pParent->lastChild())
            Q_ASSERT(false);//TODO

        if (first->m_pParent->lastChild() == first) {
            Q_ASSERT(!second->nextSibling());
            first->m_pParent->m_tChildren[LAST] = second;
        }

        first->m_tSiblings [  NEXT  ] = second;
        second->m_tSiblings[PREVIOUS] = first;

//         Q_ASSERT(!second->m_pParent->lastChild() ==second);

    }
    else if (second && ((!first) || first == second->m_pParent)) {
        auto par = second->m_pParent;
        second->remove(true);
        Q_ASSERT(second->m_LifeCycleState == LifeCycleState::NEW);
        insertChildBefore(second, nullptr, par);
    }
    else if (first && second && first->m_pParent->lastChild() == first) {
        // There is nothing to do
        Q_ASSERT(second->parent() == first->parent()->parent());
        Q_ASSERT(second->previousSibling() == first->parent());

    }
    else if (first) {

        // `first` may be the last child of a last child (of a...)
        Q_ASSERT((!second) || second->parent()->lastChild() == second);

        // It's the last element or the second is a last leaf and first is unrelated
        first->m_tSiblings[NEXT] = nullptr;

        if (!first->m_pParent->firstChild())
            first->m_pParent->m_tChildren[FIRST] = first;

        if (first->m_pParent->lastChild() && first->m_pParent->lastChild() != first) {
            first->m_pParent->lastChild()->m_tSiblings[NEXT] = first;
            first->m_tSiblings[PREVIOUS] = first->m_pParent->lastChild();
        }

        first->m_pParent->m_tChildren[LAST] = first;
        Q_ASSERT((!first) || first->m_pParent->lastChild());
//
//         //BEGIN test
//         int count =0;
//         for (auto c = first->m_pParent->lastChild(); c; c = c->previousSibling())
//             count++;
//
//         Q_ASSERT(first->m_pParent->firstChild());
//
//         Q_ASSERT(count == first->m_pParent->m_hLookup.size());
//         //END test
    }
    else {
        Q_ASSERT(false); //Something went really wrong elsewhere
    }

    _DO_TEST_IDX_STATIC(_test_bridgeGap, first, second)
}

void StateTracker::Index::remove(bool reparent)
{
    if (m_LifeCycleState == LifeCycleState::NEW)
        return;

    // You can't remove ROOT, so this should always be true
    Q_ASSERT(m_pParent);

    Q_ASSERT(m_pParent->m_hLookup.values().contains(this));
    const int size = m_pParent->m_hLookup.size();
    m_pParent->m_hLookup.remove(m_Index);
    Q_ASSERT(size == m_pParent->m_hLookup.size()+1);
    Q_ASSERT(!m_pParent->m_hLookup.values().contains(this));

    if (!reparent) {
        Q_ASSERT(!firstChild());
        Q_ASSERT(m_hLookup.isEmpty());
    }

    const auto oldNext(nextSibling()), oldPrev(previousSibling());

    // Do not use bridgeGap to prevent rist of recursion on invalid trees
    if (oldPrev && oldNext) {
        Q_ASSERT(oldPrev != oldNext);
        oldPrev->m_tSiblings[NEXT] = oldNext;
        oldNext->m_tSiblings[PREVIOUS] = oldPrev;
    }
    Q_ASSERT((!oldPrev) || oldPrev->previousSibling() != oldPrev);
    Q_ASSERT((!oldNext) || oldNext->nextSibling() != oldNext);


    if (m_pParent->firstChild() == this) {
        Q_ASSERT(!oldPrev);
        Q_ASSERT((!oldNext) || oldNext->previousSibling() == this);

        if (auto next = oldNext)
            next->m_tSiblings[PREVIOUS] = nullptr;

        m_pParent->m_tChildren[FIRST] = oldNext;

        Q_ASSERT((!oldNext) || !oldNext->previousSibling());

        if (m_pParent->firstChild())
            m_pParent->firstChild()->m_tSiblings[PREVIOUS] = nullptr;
    }

    if (m_pParent->lastChild() == this) {
        Q_ASSERT((!oldNext));
        Q_ASSERT((!oldPrev) || oldPrev->nextSibling() ==this);

        m_pParent->m_tChildren[LAST] = oldPrev;

        if (m_pParent->lastChild())
            m_pParent->lastChild()->m_tSiblings[NEXT] = nullptr;
    }

    Q_ASSERT((!m_pParent->firstChild()) || m_pParent->lastChild());


//     else if (previousSibling()) {
//         Q_ASSERT(false); //TODO
//     }
//     else if (oldNext) {
//         Q_ASSERT(false); //TODO
//     }
//     else { //FIXME very wrong
//         Q_ASSERT(m_pParent->m_hLookup.isEmpty());
//         m_pParent->m_tChildren[FIRST] = nullptr;
//         m_pParent->m_tChildren[LAST] = nullptr;
//     }

    m_LifeCycleState = LifeCycleState::NEW;
    m_pParent = nullptr;
}

StateTracker::Index* StateTracker::Index::up() const
{
    StateTracker::Index* ret = nullptr;

    // Another simple case, there is no parent
    if (!m_pParent) {
        Q_ASSERT(!m_Index.parent().isValid()); //TODO remove, no longer true when partial loading is implemented

        return nullptr;
    }

    // Keep in mind that the "root" means no parent
    if (m_pParent && m_pParent->m_pParent && !previousSibling())
        return m_pParent;

    ret = previousSibling();

    while (ret && ret->lastChild())
        ret = ret->lastChild();

    return ret;
}

StateTracker::Index* StateTracker::Index::next(Qt::Edge e) const
{
    switch (e) {
        case Qt::TopEdge:
            return up();
        case Qt::BottomEdge:
            return down();
        case Qt::LeftEdge:
            return left();
        case Qt::RightEdge:
            return right();
    }

    Q_ASSERT(false);

    return nullptr;
}

StateTracker::Index* StateTracker::Index::down() const
{
    StateTracker::Index* ret = nullptr;
    auto i = this;

    if (firstChild()) {
        //Q_ASSERT(m_pParent->firstChild()->m_Index.row() == 0); //racy
        ret = firstChild();
        return ret;
    }

    // Recursively unwrap the tree until an element is found
    while(i) {
        if (i->nextSibling() && (ret = i->nextSibling()))
            return ret;

        i = i->parent();
    }

    // Can't happen, exists to detect corrupted code
    if (m_Index.parent().isValid()) {
        Q_ASSERT(m_pParent);
//         Q_ASSERT(m_pParent->parent()->parent()->m_hLookup.size()
//             == m_pParent->m_Index.parent().row()+1);
    }

    return ret;
}

StateTracker::Index* StateTracker::Index::left() const
{
    return nullptr; //TODO
}

StateTracker::Index* StateTracker::Index::right() const
{
    return nullptr; //TODO
}

void ModelRect::setEdge(StateTracker::Index* tti, Qt::Edge e)
{
    m_lpEdges.set(tti, e);
}

StateTracker::Index* ModelRect::getEdge(Qt::Edge e) const
{
    return m_lpEdges.get(e);
}

StateTracker::Index *StateTracker::Index::childrenLookup(const QPersistentModelIndex &index) const
{
    return m_hLookup.value(index);
}

bool StateTracker::Index::hasChildren(StateTracker::Index *child) const
{
    return !m_hLookup.keys(child).isEmpty();
}

int StateTracker::Index::loadedChildrenCount() const
{
    return m_hLookup.size();
}

QList<StateTracker::Index*> StateTracker::Index::allLoadedChildren() const
{
    return m_hLookup.values();
}

bool StateTracker::Index::withinRange(QAbstractItemModel* m, int last, int first) const
{
    // Return true if the previous element or next element are loaded
    const QModelIndex prev = first ? m->index(first - 1, 0, m_Index) : QModelIndex();
    const QModelIndex next = first ? m->index(last  + 1, 0, m_Index) : QModelIndex();

    return (prev.isValid() && m_hLookup.contains(prev))
        || (next.isValid() && m_hLookup.contains(next));
}

int StateTracker::Index::effectiveRow() const
{
    return m_MoveToRow == -1 ?
        m_Index.row() : m_MoveToRow;
}

int StateTracker::Index::effectiveColumn() const
{
    return m_MoveToColumn == -1 ?
        m_Index.column() : m_MoveToColumn;
}

QPersistentModelIndex StateTracker::Index::effectiveParentIndex() const
{
    return m_LifeCycleState == LifeCycleState::TRANSITION ?
        m_MoveToParent : m_pParent->index();
}

void StateTracker::Index::resetTemporaryIndex()
{
    Q_ASSERT(m_pParent);
    Q_ASSERT(m_LifeCycleState == LifeCycleState::TRANSITION);
    m_LifeCycleState = LifeCycleState::NORMAL;
    m_MoveToRow    = -1;
    m_MoveToColumn = -1;
    m_MoveToParent = QModelIndex();
}

bool StateTracker::Index::hasTemporaryIndex()
{
    return m_MoveToRow != -1 || m_MoveToColumn != -1;
}

void StateTracker::Index::setTemporaryIndex(const QModelIndex& newParent, int row, int column)
{
    //TODO handle parent
    Q_ASSERT(m_LifeCycleState == LifeCycleState::NORMAL);
    m_MoveToParent = newParent;
    m_MoveToRow    = row;
    m_MoveToColumn = column;
    m_LifeCycleState = LifeCycleState::TRANSITION;
}

QPersistentModelIndex StateTracker::Index::index() const
{
    return m_Index;
}

void StateTracker::Index::setModelIndex(const QPersistentModelIndex& idx)
{
    Q_ASSERT(m_LifeCycleState == LifeCycleState::NEW);
    m_Index = idx;
}

int StateTracker::Index::depth() const
{
    int d = 0;

    auto s = this;

    while((s = s->parent()) && ++d);

    return d;//FIXME m_pTTI->m_Depth;
}

IndexMetadata *StateTracker::Index::metadata() const
{
    return &m_Geometry;
}

#undef PREVIOUS
#undef NEXT
#undef FIRST
#undef LAST
#undef TTI
