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
#ifndef KQUICKITEMVIEWS_CONTINUITY_P_H
#define KQUICKITEMVIEWS_CONTINUITY_P_H

namespace StateTracker {

class Index;

/**
 * This bare-bone state tracker helps to track the continuity between the
 * ELEMENTS WITH THE SAME PARENT.
 *
 * The use case for this is to prevent the O(N) operation of the list of
 * elements to check if there is missing holes in them. This could be done with
 * simple artmetic on the row of the last child, first child and the number of
 * children, but this would not handle the corner cases where there multiple
 * "holes". It would also not allow to know where the hole(s) are without
 * looping.
 *
 * This may seem overkill to waste memory tracking this, but there is 2 reasons
 * why it isn't such a bad idea:
 *
 *  * Some ViewportStategies need to know this in every frame (fps) of a scroll
 *    operation with tight frame duration deadlines.
 *  * If the viewport loads a full list model, the "N" of "O(N)" can be very
 *    large.
 *
 * @todo This should, once fully implemented, go hand in hand with
 *  StateTracker::Proximity to avoid useless rowCount other overhead in
 *  StateTracker::Content and make methods such as `isActive` reliable.
 *
 * @todo Slicing a StateTracker::Continuity is currently rather expensive. This
 *  can be brought down to "O(N*log(N))" by using double pointers to create
 *  "pages" of a size representative of the total number of elements. However
 *  this will take more memory and has an overhead in itself. It wont be done
 *  unless it is proven to be significantly faster for real-world models.
 */
class Continuity final
{
    friend class Index; // Factory, call private API

public:
    Index *first() const;
    Index *last () const;

    int size() const;

private:
    explicit Continuity(Index *first);

    StateTracker::Index *m_pFirst {nullptr};
    StateTracker::Index *m_pLast  {nullptr};

    static void split(StateTracker::Index *at);
    static void merge(StateTracker::Index *one, StateTracker::Index *two);
    static void remove(Index *i);
    static void setContinuity(Index *i, Continuity *c);
    static Continuity *select(StateTracker::Index *i);
};

}

#endif
