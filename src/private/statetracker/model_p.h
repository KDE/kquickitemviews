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


class QAbstractItemModel;

namespace StateTracker
{
class Content;

class Model
{
public:
    explicit Model(StateTracker::Content* q);

    enum class State {
        NO_MODEL , /*!< The model is not set, there is nothing to do                  */
        PAUSED   , /*!< The model is set, but the reflector is not listening          */
        POPULATED, /*!< The initial insertion has been done, it is ready for tracking */
        TRACKING , /*!< The model is set and the reflector is listening to changes    */
        RESETING , /*!< The model is undergoing a reset process                       */
        MUTATING , /*!< Insertion events are not re-entrant, prevent this             */
    };

    enum class Action {
        POPULATE, /*!< Fetch the model content and fill the view */
        DISABLE , /*!< Disconnect the model tracking             */
        ENABLE  , /*!< Connect the pending model                 */
        RESET   , /*!< Remove the delegates but keep the trackers*/
        FREE    , /*!< Free the whole tracking tree              */
        MOVE    , /*!< Try to fix the viewport with content      */
        TRIM    , /*!< Remove the elements until the edge is free*/
    };

    /**
     * Manipulate the tracking state.
     */
    State performAction(Action);

    State state() const;

    QAbstractItemModel *trackedModel  () const;
    QAbstractItemModel *modelCandidate() const;
    void setModel(QAbstractItemModel* m);

private:
    typedef void(StateTracker::Model::*StateF)();

    // Attributes
    State m_State {State::NO_MODEL};
    QAbstractItemModel* m_pModel        {nullptr};
    QAbstractItemModel* m_pTrackedModel {nullptr};

    // Actions, do not call directly
    void track();
    void untrack();
    void nothing();
    void reset();
    void free();
    void error();
    void populate();
    void fill();
    void trim();

    static const State  m_fStateMap    [6][7];
    static const StateF m_fStateMachine[6][7];

    StateTracker::Content *q_ptr;
};

/**
 * Allow daisy-chaining when the return value isn't used.
 */
template<typename A>
inline Model *operator<<(Model* md, A a)
{
    md->performAction(a);
    return md;
}

}

