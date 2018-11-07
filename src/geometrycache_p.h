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

struct BlockMetadata;

/**
 * Computing the absolute geometry of items isn't always trivial.
 *
 * There is always the case where you known part of the answer (point or size)
 * and/or where the item moves and the size is valid but not the position.
 *
 * If there is no scrollbar, having the absolute position right isn't
 * necessary as long as the relative one is correct.
 *
 * This structure tries to allow delaying the geometry computation as late
 * as possible.
 *
 * TODO add some kind of Frame to reduce the number of iterations to update
 * larger models with scrollbars
 */
struct GeometryCache
{
    enum class State {
        INIT    , /*!< The value has not been computed */
        SIZE    , /*!< The size had been computed      */
        POSITION, /*!< The position has been computed  */
        VALID   , /*!< The geometry has been computed  */
    };

    enum class Action {
        INSERT ,
        MOVE   ,
        REMOVE ,
        RESIZE ,
        VIEW   ,
        PLACE  ,
        RESET  , /*!<  */
        MODIFY , /*!< When the QModelIndex role change */
    };

    // actions
    void nothing(BlockMetadata* topLeft, BlockMetadata* bottomRight);
    void discardPosition(BlockMetadata* topLeft, BlockMetadata* bottomRight);
    void discardSize(BlockMetadata* topLeft, BlockMetadata* bottomRight);
    void invalidate(BlockMetadata* topLeft, BlockMetadata* bottomRight);

    void split(BlockMetadata* topLeft, State above, State below);

    State performAction(Action, BlockMetadata*, BlockMetadata*);

    void setPosition(const QPointF& pos);
    void setSize(const QSizeF& size);
    QRectF geometry() const;
    QSizeF size() const;
    QPointF position() const;

    State state() const;
private:

    QPointF m_Position;
    QSizeF  m_Size;

    typedef void(GeometryCache::*StateF)(BlockMetadata*, BlockMetadata*);

    static const State  m_fStateMap    [4][8];
    static const StateF m_fStateMachine[4][8];

    BlockMetadata *m_pFirst {nullptr};
    BlockMetadata *m_pLast  {nullptr};

    State m_State {State::INIT};
};

