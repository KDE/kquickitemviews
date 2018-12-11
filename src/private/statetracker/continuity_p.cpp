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
#include "continuity_p.h"

#include "index_p.h"

void StateTracker::Continuity::remove(StateTracker::Index *i)
{
    if (!i->m_pContinuity)
        return;

    // Free the existing StateTracker::Continuity
    if (i->m_pContinuity->size() == 1) {
        delete i->m_pContinuity;
        i->m_pContinuity = nullptr;
        return;
    }

    // Make sure first()/last() are not outdated
    if (i->m_pContinuity->m_pFirst == i)
        i->m_pContinuity->m_pFirst = i->nextSibling();

    if (i->m_pContinuity->m_pLast == i)
        i->m_pContinuity->m_pLast = i->previousSibling();

    i->m_pContinuity = nullptr;
}

void StateTracker::Continuity::setContinuity(StateTracker::Index *i, StateTracker::Continuity *c)
{
    if (i->m_pContinuity == c)
        return;

    remove(i);

    i->m_pContinuity = c;
}

StateTracker::Continuity::Continuity(Index *first) :
    m_pFirst(first), m_pLast(first)
{}

int StateTracker::Continuity::size() const
{
    if (!first())
        return 0;

    // +1 it starts at zero
    return (last()->effectiveRow() - first()->effectiveRow()) + 1;
}

StateTracker::Index *StateTracker::Continuity::first() const
{
    return m_pFirst;
}

StateTracker::Index *StateTracker::Continuity::last() const
{
    return m_pLast;
}

StateTracker::Continuity *StateTracker::Continuity::select(StateTracker::Index *i)
{
    auto prev = i->previousSibling();
    auto next = i->nextSibling();
    auto old  = i->m_pContinuity;

    if (prev && i->isNeighbor(prev)) {
        if (prev->continuityTracker()->last() == prev)
            prev->continuityTracker()->m_pLast = i;
        i->m_pContinuity = prev->continuityTracker();
    }
    else if (next && i->isNeighbor(next)) {
        if (next->continuityTracker()->first() == next)
            next->continuityTracker()->m_pFirst = i;
        i->m_pContinuity = next->continuityTracker();
    }
    else if (!old)
        i->m_pContinuity = new Continuity(i);

    Q_ASSERT(i->m_pContinuity);

    return i->m_pContinuity;
}

void StateTracker::Continuity::split(StateTracker::Index *at)
{
    Q_ASSERT(false); //TODO
    auto old = at->m_pContinuity;
    auto n   = new Continuity(at);

    do {
        at->m_pContinuity = n;
    } while((at = at->nextSibling()) && at->m_pContinuity == old);
}

void StateTracker::Continuity::merge(StateTracker::Index *one, StateTracker::Index *two)
{
    Q_UNUSED(one)
    Q_UNUSED(two)
    Q_ASSERT(false); //TODO
}
