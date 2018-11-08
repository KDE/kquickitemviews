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
#include <treestructure_p.h>


// Use some constant for readability
#define PREVIOUS 0
#define NEXT 1
#define FIRST 0
#define LAST 1

TreeTraversalBase::~TreeTraversalBase()
{
    remove();
}

TreeTraversalBase* TreeTraversalBase::firstChild() const
{
    return m_tChildren[FIRST];
}

TreeTraversalBase* TreeTraversalBase::lastChild() const
{
    return m_tChildren[LAST];
}

TreeTraversalBase* TreeTraversalBase::nextSibling() const
{
    return m_tSiblings[NEXT];
}

TreeTraversalBase* TreeTraversalBase::previousSibling() const
{
    return m_tSiblings[PREVIOUS];
}

TreeTraversalBase *TreeTraversalBase::parent() const
{
    return m_pParent;
}

void TreeTraversalBase::insertChildAfter(TreeTraversalBase* self, TreeTraversalBase* other, TreeTraversalBase* parent)
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

        parent->_test_validate_chain();
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

    parent->_test_validate_chain();
}

void TreeTraversalBase::insertChildBefore(TreeTraversalBase* self, TreeTraversalBase* other, TreeTraversalBase* parent)
{
    Q_ASSERT(!self->m_pParent);
    Q_ASSERT(parent);
    Q_ASSERT(self->m_LifeCycleState == LifeCycleState::NEW);
    Q_ASSERT(!parent->m_hLookup.contains(self->m_Index));

    parent->_test_validate_chain();

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
        parent->_test_validate_chain();
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

        parent->_test_validate_chain();
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

    parent->_test_validate_chain();
}


// void TreeTraversalBase::createGap(TreeTraversalBase* first, TreeTraversalBase* last)
// {
//     Q_ASSERT(first->m_pParent == last->m_pParent);
//
//     // This is simpler
//     if (first == last) {
//         first->remove();
//         return;
//     }
//
//     if (first->previousSibling()) {
//         first->previousSibling()->m_tSiblings[NEXT] = last->nextSibling();
//     }
//
//     if (last->nextSibling()) {
//         last->nextSibling()->m_tSiblings[PREVIOUS] = first->previousSibling();
//     }
//
//     if (first->m_pParent->firstChild() ==first) {
//         first->m_pParent->m_tChildren[FIRST] = last->nextSibling();
//         Q_ASSERT(!first->m_pParent->firstChild()->previousSibling());
//     }
//
//     if (last->m_pParent->lastChild() ==last) {
//         last->m_pParent->m_tChildren[LAST] = first->previousSibling();
//
//         Q_ASSERT((!last->m_pParent->lastChild()) || !last->m_pParent->lastChild()->nextSibling());
//     }
//
//     Q_ASSERT((!first->previousSibling()) ||
//         first->previousSibling()->down() != first);
//     Q_ASSERT((!last->nextSibling()) ||
//         last->nextSibling()->up() != last);
//
//     Q_ASSERT((!first) || first->firstChild() || first->m_hLookup.isEmpty());
//     Q_ASSERT((!last) || last->firstChild() || last->m_hLookup.isEmpty());
//     Q_ASSERT((!first->m_pParent->firstChild()) || first->m_pParent->lastChild());
//
//     // Do not leave invalid pointers for easier debugging
//     last->m_tSiblings[NEXT]      = nullptr;
//     first->m_tSiblings[PREVIOUS] = nullptr;
//
//     first->m_pParent->_test_validate_chain();
// }

/// Fix the issues introduced by createGap (does not update m_pParent and m_hLookup)
void TreeTraversalBase::bridgeGap(TreeTraversalBase* first, TreeTraversalBase* second)
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
        if (second) {
            Q_ASSERT(second->parent()->lastChild() == second);
        }

        Q_ASSERT(!second);


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

    if (second && first && second->m_pParent && second->m_pParent->firstChild() ==second && second->m_pParent == first->m_pParent) {
        second->m_pParent->m_tChildren[FIRST] = first;
        Q_ASSERT((!first) || second->m_pParent->lastChild());
        Q_ASSERT((!first) || !first->previousSibling());
    }

    if ((!first) && second->m_MoveToRow != -1) {
        Q_ASSERT(second->m_pParent->firstChild());
        Q_ASSERT(second->m_pParent->firstChild() ==second ||
            second->m_pParent->firstChild()->m_Index.row() < second->m_MoveToRow);
    }

    if (first)
        Q_ASSERT(first->m_pParent->firstChild());
    if (second)
        Q_ASSERT(second->m_pParent->firstChild());

    if (first)
        Q_ASSERT(first->m_pParent->lastChild());
    if (second)
        Q_ASSERT(second->m_pParent->lastChild());


    Q_ASSERT((!second) || (!second->m_pParent->firstChild()) || second->m_pParent->lastChild());


//     if (first && second) { //Need to disable other asserts in down()
//         Q_ASSERT(first->down() == second);
//         Q_ASSERT(second->up() == first);
//     }

    // Close the gap between the old previous and next elements
    Q_ASSERT((!first ) || first->nextSibling()      != first );
    Q_ASSERT((!first ) || first->previousSibling()  != first );
    Q_ASSERT((!second) || second->nextSibling()     != second);
    Q_ASSERT((!second) || second->previousSibling() != second);
}

void TreeTraversalBase::remove(bool reparent)
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

    auto oldNext = nextSibling();
    auto oldPrev = previousSibling();

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

TreeTraversalBase* TreeTraversalBase::up() const
{
    TreeTraversalBase* ret = nullptr;

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

TreeTraversalBase* TreeTraversalBase::down() const
{
    TreeTraversalBase* ret = nullptr;
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

TreeTraversalBase* TreeTraversalBase::left() const
{
    return nullptr; //TODO
}

TreeTraversalBase* TreeTraversalBase::right() const
{
    return nullptr; //TODO
}

// void TreeTraversalBase::reparentUntil(TreeTraversalBase *np, TreeTraversalBase *until)
// {
//     const auto oldP = m_pParent;
//
//     Q_ASSERT(until->m_pParent == oldP);
//
//     if (oldP != np) {
//         for (auto i = this; i; i = i->nextSibling()) {
//             Q_ASSERT(i->m_pParent == oldP);
//             auto idx = i->m_Index;
//
//             const int size = oldP->m_hLookup.size();
//             oldP->m_hLookup.remove(idx);
//             Q_ASSERT(oldP->m_hLookup.size() == size-1);
//
//             Q_ASSERT(!i->m_pParent->m_hLookup.values().contains(i));
//             np->m_hLookup[idx] = i;
//             i->m_pParent = np;
//             Q_ASSERT(i->m_pParent->m_hLookup.values().contains(i));
//             i->m_LifeCycleState = LifeCycleState::NORMAL;
//             if (i == until)
//                 break;
//         }
//     }
//
//     if (oldP)
//         oldP->_test_validate_chain();
// }

int ModelRect::edgeToIndex(Qt::Edge e) const
{
    switch(e) {
        case Qt::TopEdge:
            return Pos::Top;
        case Qt::LeftEdge:
            return Pos::Left;
        case Qt::RightEdge:
            return Pos::Right;
        case Qt::BottomEdge:
            return Pos::Bottom;
    }

    Q_ASSERT(false);
    return Pos::Top;
}

void ModelRect::setEdge(TreeTraversalBase* tti, Qt::Edge e)
{
    m_lpEdges[edgeToIndex(e)] = tti;
}

TreeTraversalBase* ModelRect::getEdge(Qt::Edge e) const
{
    return m_lpEdges[edgeToIndex(e)];
}

TreeTraversalBase *TreeTraversalBase::childrenLookup(const QPersistentModelIndex &index) const
{
    return m_hLookup.value(index);
}

bool TreeTraversalBase::hasChildren(TreeTraversalBase *child) const
{
    return !m_hLookup.keys(child).isEmpty();
}

int TreeTraversalBase::loadedChildrenCount() const
{
    return m_hLookup.size();
}

QList<TreeTraversalBase*> TreeTraversalBase::allLoadedChildren() const
{
    return m_hLookup.values();
}

bool TreeTraversalBase::withinRange(QAbstractItemModel* m, int last, int first) const
{
    // Return true if the previous element or next element are loaded
    const QModelIndex prev = first ? m->index(first - 1, 0, m_Index) : QModelIndex();
    const QModelIndex next = first ? m->index(last  + 1, 0, m_Index) : QModelIndex();

    return (prev.isValid() && m_hLookup.contains(prev))
        || (next.isValid() && m_hLookup.contains(next));
}

int TreeTraversalBase::effectiveRow() const
{
    return m_MoveToRow == -1 ?
        m_Index.row() : m_MoveToRow;
}

int TreeTraversalBase::effectiveColumn() const
{
    return m_MoveToColumn == -1 ?
        m_Index.column() : m_MoveToColumn;
}

QPersistentModelIndex TreeTraversalBase::effectiveParentIndex() const
{
    return m_LifeCycleState == LifeCycleState::TRANSITION ?
        m_MoveToParent : m_pParent->index();
}

void TreeTraversalBase::resetTemporaryIndex()
{
    Q_ASSERT(m_pParent);
    Q_ASSERT(m_LifeCycleState == LifeCycleState::TRANSITION);
    m_LifeCycleState = LifeCycleState::NORMAL;
    m_MoveToRow    = -1;
    m_MoveToColumn = -1;
    m_MoveToParent = QModelIndex();
}

bool TreeTraversalBase::hasTemporaryIndex()
{
    return m_MoveToRow != -1 || m_MoveToColumn != -1;
}

void TreeTraversalBase::setTemporaryIndex(const QModelIndex& newParent, int row, int column)
{
    //TODO handle parent
    Q_ASSERT(m_LifeCycleState == LifeCycleState::NORMAL);
    m_MoveToParent = newParent;
    m_MoveToRow    = row;
    m_MoveToColumn = column;
    m_LifeCycleState = LifeCycleState::TRANSITION;
}

QPersistentModelIndex TreeTraversalBase::index() const
{
    return m_Index;
}

void TreeTraversalBase::setModelIndex(const QPersistentModelIndex& idx)
{
    Q_ASSERT(m_LifeCycleState == LifeCycleState::NEW);
    m_Index = idx;
}

#undef PREVIOUS
#undef NEXT
#undef FIRST
#undef LAST
#undef TTI
