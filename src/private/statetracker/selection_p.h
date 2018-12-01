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

// Qt
#include <QtCore/QRectF>
#include <QtCore/QItemSelectionModel>

#include <private/indexmetadata_p.h>
class SelectionAdapter;

namespace StateTracker {

/**
 * WORK IN PROGRESS
 *
 * Keep track of the selection status.
 *
 * This support both currentIndex and multi-selection modes and supports
 * multiple selection models on the same more.
 */
struct Selection
{
    enum class State {
        NONE               , /*!< No selection model hold a selection on this item                  */
        CURRENT_INDEX      , /*!< A single selection model sets it as currentIndex                  */
        MULTIPLE           , /*!< A single selection model mutli-select the item                    */
        MIXED_CURRENT_INDEX, /*!< Multiple sel. model select this item, at least 1 as current index */
        MIXED_MULTIPLE     , /*!< Multiple selection models multi-select this index                 */
    };

    bool performAction(QItemSelectionModel::SelectionFlag f, SelectionAdapter *ad);

    State state() const;

private:
    typedef void(Geometry::*StateF)();

    /*static const State  m_fStateMap    [5][7];
    static const StateF m_fStateMachine[5][7];*/

    // Actions
    //TODO

    State m_State {State::NONE};
};

}
