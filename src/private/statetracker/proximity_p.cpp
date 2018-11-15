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
#include "proximity_p.h"

#include "index_p.h"

using State = StateTracker::Proximity::State;

class ProximityPrivate
{
public:
    void nothing() {}

    QModelIndexList up  () const;
    QModelIndexList down() const;

    typedef void(ProximityPrivate::*StateF)();

    static const State  m_fStateMap    [6][3];
    static const StateF m_fStateMachine[6][3];

    enum Pos {Top, Left, Right, Bottom};

    State m_State[4] {
        State::UNKNOWN, State::UNKNOWN,
        State::UNKNOWN, State::UNKNOWN,
    };

    StateTracker::Index *m_pSelf;

    IndexMetadata *q_ptr;
};

#define S StateTracker::Proximity::State::
const StateTracker::Proximity::State ProximityPrivate::m_fStateMap[6][3] = {
/*                         QUERY     DISCARD     MOVE   */
/* UNKNOWN         */ { S UNKNOWN, S UNKNOWN, S UNKNOWN },
/* LOADED          */ { S UNKNOWN, S UNKNOWN, S UNKNOWN },
/* MOVED           */ { S UNKNOWN, S UNKNOWN, S UNKNOWN },
/* UNLOADED_TOP    */ { S UNKNOWN, S UNKNOWN, S UNKNOWN },
/* UNLOADED_BOTTOM */ { S UNKNOWN, S UNKNOWN, S UNKNOWN },
/* UNLOADED_BOTH   */ { S UNKNOWN, S UNKNOWN, S UNKNOWN },
};
#undef S

#define A &ProximityPrivate::
const ProximityPrivate::StateF ProximityPrivate::m_fStateMachine[6][3] = {
/*                         QUERY     DISCARD     MOVE   */
/* UNKNOWN         */ { A nothing, A nothing, A nothing },
/* LOADED          */ { A nothing, A nothing, A nothing },
/* MOVED           */ { A nothing, A nothing, A nothing },
/* UNLOADED_TOP    */ { A nothing, A nothing, A nothing },
/* UNLOADED_BOTTOM */ { A nothing, A nothing, A nothing },
/* UNLOADED_BOTH   */ { A nothing, A nothing, A nothing },
};
#undef A

static ProximityPrivate::Pos edgeToPos(Qt::Edge e)
{
    switch(e) {
        case Qt::TopEdge:
            return ProximityPrivate::Pos::Top;
        case Qt::LeftEdge:
            return ProximityPrivate::Pos::Left;
        case Qt::RightEdge:
            return ProximityPrivate::Pos::Right;
        case Qt::BottomEdge:
            return ProximityPrivate::Pos::Bottom;
    }

    Q_ASSERT(false);

    return {};
}

StateTracker::Proximity::Proximity(IndexMetadata *q, StateTracker::Index *self) :
    d_ptr(new ProximityPrivate())
{
    d_ptr->q_ptr   = q;
    d_ptr->m_pSelf = self;
}

void StateTracker::Proximity::performAction(Proximity::Action a, Qt::Edge e)
{

}

bool StateTracker::Proximity::canLoadMore(Qt::Edge e)
{
    Q_ASSERT(false);
    return false;
}

QModelIndexList StateTracker::Proximity::getNext(Qt::Edge e)
{
    //TODO right now this will return garbage, this will need some more code to handle
    Q_ASSERT(d_ptr->m_pSelf->lifeCycleState() != StateTracker::Index::LifeCycleState::TRANSITION); //TODO

    switch (e) {
        case Qt::TopEdge:
            return d_ptr->up();
        case Qt::BottomEdge:
            return d_ptr->down();
        case Qt::LeftEdge:
        case Qt::RightEdge:
            Q_ASSERT(false); //TODO
    }

    return {};
}

QModelIndexList ProximityPrivate::up() const
{
    QModelIndexList ret;

    const int r = m_pSelf->effectiveRow();

    if (r) {
        // When there is a previous sibling, return its last (grant) children
        // or return that sibling.
        auto idx = m_pSelf->index().model()->index(
            r - 1, m_pSelf->index().column(), m_pSelf->effectiveParentIndex()
        );

        if (!idx.isValid())
            return ret;

        ret << idx;
        Q_ASSERT(idx.isValid());

        // Find the deepest (grang)children
        while (auto rc = m_pSelf->index().model()->rowCount(idx)) {
            ret << (idx = m_pSelf->index().model()->index(
                rc - 1, 0, m_pSelf->effectiveParentIndex()
            ));
            Q_ASSERT(idx.isValid());
        }

        return ret;
    }
    else {
        // Up is either the parent or a (grand) children of the parent
        auto idx = m_pSelf->effectiveParentIndex();

        if (!idx.isValid())
            return ret;

        ret << idx;

        // Find the deepest (grang)children
        while (auto rc = m_pSelf->index().model()->rowCount(idx)) {
            if (!idx.isValid())
                break;

            ret << (idx = m_pSelf->index().model()->index(
                rc - 1, 0, m_pSelf->effectiveParentIndex()
            ));
        }

        return ret;
    }

    //TODO recurse parent->rowCount..->last
    Q_ASSERT(m_pSelf->parent() &&
        m_pSelf->parent()->lifeCycleState() == StateTracker::Index::LifeCycleState::ROOT
    );

    return ret;
}

QModelIndexList ProximityPrivate::down() const
{
    int effRow = m_pSelf->effectiveRow();
    auto par   = m_pSelf->effectiveParentIndex();

    // In *theory*, all relevant parents should be loaded, so it's always
    // returning a single item

    do {
        if (effRow + 1 < m_pSelf->index().model()->rowCount(par)) {
            return {
                m_pSelf->index().model()->index(effRow + 1, m_pSelf->index().column(), par)
            };
        }

        effRow = par.row();
    } while ((par = par.parent()).isValid());

    return {};
}
