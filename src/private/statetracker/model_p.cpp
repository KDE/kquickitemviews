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
#include "model_p.h"

#include <QtGlobal>

#define S StateTracker::Model::State::
const StateTracker::Model::State StateTracker::Model::m_fStateMap[5][7] = {
/*                POPULATE     DISABLE      ENABLE       RESET        FREE         MOVE         TRIM   */
/*NO_MODEL */ { S NO_MODEL , S NO_MODEL , S NO_MODEL, S NO_MODEL, S NO_MODEL , S NO_MODEL , S NO_MODEL },
/*PAUSED   */ { S POPULATED, S PAUSED   , S TRACKING, S PAUSED  , S PAUSED   , S PAUSED   , S PAUSED   },
/*POPULATED*/ { S TRACKING , S PAUSED   , S TRACKING, S RESETING, S POPULATED, S POPULATED, S POPULATED},
/*TRACKING */ { S TRACKING , S POPULATED, S TRACKING, S RESETING, S TRACKING , S TRACKING , S TRACKING },
/*RESETING */ { S RESETING , S RESETING , S TRACKING, S RESETING, S PAUSED   , S RESETING , S RESETING },
};
#undef S

// This state machine is self healing, error can be called in release mode
// and it will only disable the view without further drama.
#define A &StateTracker::Model::
const StateTracker::Model::StateF StateTracker::Model::m_fStateMachine[5][7] = {
/*               POPULATE     DISABLE    ENABLE     RESET       FREE       MOVE   ,   TRIM */
/*NO_MODEL */ { A nothing , A nothing, A nothing, A nothing, A nothing, A nothing , A error },
/*PAUSED   */ { A populate, A nothing, A error  , A nothing, A free   , A nothing , A trim  },
/*POPULATED*/ { A error   , A nothing, A track  , A reset  , A free   , A fill    , A trim  },
/*TRACKING */ { A nothing , A untrack, A nothing, A reset  , A free   , A fill    , A trim  },
/*RESETING */ { A nothing , A error  , A track  , A error  , A free   , A error   , A error },
};

StateTracker::Model::Model(TreeTraversalReflectorPrivate* d) : d_ptr(d)
{}

StateTracker::Model::State StateTracker::Model::performAction(Action a)
{
    const int s = (int)m_State;
    m_State = m_fStateMap[s][(int)a];

    (this->*StateTracker::Model::m_fStateMachine[s][(int)a])();

    return m_State;
}

StateTracker::Model::State StateTracker::Model::state() const
{
    return m_State;
}

//TODO removing this requires moving some TreeTraversalReflectorPrivate
// attributes into this state tracker.
void StateTracker::Model::forcePaused()
{
    Q_ASSERT(m_State == State::NO_MODEL || m_State == State::PAUSED);
    m_State = State::PAUSED;
}
