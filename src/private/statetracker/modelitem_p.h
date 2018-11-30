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
#pragma once

#include <private/statetracker/index_p.h>
#include <private/indexmetadata_p.h>

class TreeTraversalReflector;

namespace StateTracker {

/**
 * Hold the metadata associated with the QModelIndex.
 */
struct ModelItem final : public StateTracker::Index
{
    friend class ::IndexMetadata; //access the state machine

    explicit ModelItem(TreeTraversalReflector *q);

    enum class State {
        NEW       = 0, /*!< During creation, not part of the tree yet         */
        BUFFER    = 1, /*!< Not in the viewport, but close                    */
        REMOVED   = 2, /*!< Currently in a removal transaction                */
        REACHABLE = 3, /*!< The [grand]parent of visible indexes              */
        VISIBLE   = 4, /*!< The element is visible on screen                  */
        ERROR     = 5, /*!< Something went wrong                              */
        DANGLING  = 6, /*!< Being destroyed                                   */
        MOVING    = 7, /*!< Currently undergoing a move operation             */
    };

    typedef bool(ModelItem::*StateF)();

    virtual ~ModelItem() {}

    StateTracker::ModelItem* load(Qt::Edge e) const;

    // Getter
    IndexMetadata::EdgeType isTopEdge() const;
    IndexMetadata::EdgeType isBottomEdge() const;
    State state() const;

    // Mutator
    void rebuildState();

    //TODO refactor once the class are split
    virtual void remove(bool reparent = false) override;

private:
    // Actions
    bool nothing();
    bool error  ();
    bool show   ();
    bool hide   ();
    bool remove2();
    bool attach ();
    bool detach ();
    bool refresh();
    bool move   ();
    bool destroy();
    bool reset  ();

    // Attributes
    State m_State {State::BUFFER};

    static const State  m_fStateMap    [8][8];
    static const StateF m_fStateMachine[8][8];

    TreeTraversalReflector* q_ptr;
};

}
