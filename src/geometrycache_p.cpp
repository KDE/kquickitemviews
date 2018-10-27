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
#include "geometrycache_p.h"

#include <QtCore/QDebug>

#include "viewport_p.h"

#define S GeometryCache::State::
const GeometryCache::State GeometryCache::m_fStateMap[4][7] = {
/*               INSERT     MOVE    REMOVE    RESIZE      VIEW       PLACE      RESET*/
/*INIT     */ { S INIT , S INIT , S INIT , S SIZE    , S INIT    , S POSITION, S INIT},
/*SIZE     */ { S VALID, S SIZE , S INIT , S SIZE    , S SIZE    , S VALID   , S INIT},
/*POSITION */ { S VALID, S INIT , S INIT , S VALID   , S POSITION, S POSITION, S INIT},
/*VALID    */ { S VALID, S SIZE , S SIZE , S VALID   , S VALID   , S VALID   , S INIT},
};
#undef S

#define A &GeometryCache::
const GeometryCache::StateF GeometryCache::m_fStateMachine[4][7] = {
/*                INSERT      MOVE      REMOVE     RESIZE      VIEW       PLACE , A nothing */
/*INIT     */ { A nothing, A nothing, A nothing, A nothing, A nothing, A nothing, A nothing },
/*SIZE     */ { A nothing, A nothing, A nothing, A nothing, A nothing, A nothing, A nothing },
/*POSITION */ { A nothing, A nothing, A nothing, A nothing, A nothing, A nothing, A nothing },
/*VALID    */ { A nothing, A nothing, A invalidate, A nothing, A nothing, A nothing, A invalidate },
};
#undef A


void GeometryCache::nothing(BlockMetadata* topLeft, BlockMetadata* bottomRight)
{

}

void GeometryCache::discardPosition(BlockMetadata* topLeft, BlockMetadata* bottomRight)
{

}

void GeometryCache::discardSize(BlockMetadata* topLeft, BlockMetadata* bottomRight)
{

}

void GeometryCache::invalidate(BlockMetadata* topLeft, BlockMetadata* bottomRight)
{
    qDebug() << "INVALIDATE" << m_Position << m_Size;
}

void GeometryCache::split(BlockMetadata* topLeft, State above, State below)
{

}

GeometryCache::State GeometryCache::performAction(Action a, BlockMetadata* tl, BlockMetadata* br)
{
    const int s = (int)m_State;
    m_State     = m_fStateMap[s][(int)a];

    (this->*GeometryCache::m_fStateMachine[s][(int)a])(tl, br);

    // Validate the state
    switch(m_State) {
        case GeometryCache::State::VALID:
            Q_ASSERT(!(m_Position.x() == -1 && m_Position.y() == -1));
            Q_ASSERT(m_Size.isValid());
            break;
        case GeometryCache::State::SIZE:
            Q_ASSERT(m_Size.isValid());
            break;
        case GeometryCache::State::POSITION:
            Q_ASSERT(!(m_Position.x() == -1 && m_Position.y() == -1));
            break;
        case GeometryCache::State::INIT:
            break;
    }

    return m_State;
}

QRectF GeometryCache::geometry() const
{
    // So far don't let getting the geometry "accidentally" happen. It has valid
    // use case where this code could lazy-load it. However it makes everything
    // very hard to debug after the fact since doing so in the middle of a
    // transaction will cache an invalid geometry, voiding the whole point of
    // this state machine.
    switch(m_State) {
        case GeometryCache::State::VALID:
            break;
        case GeometryCache::State::INIT:
            // Try never to get here, it may be possible to add a new state for no_item
            Q_ASSERT(false);
            break;
        case GeometryCache::State::SIZE:
            //TODO check the size hint strategy, it may be available
            Q_ASSERT(false);
            [[clang::fallthrough]]
        case GeometryCache::State::POSITION:
            Q_ASSERT(false);
            //TODO check the size hint strategy, it may be available
    }

    return QRectF(m_Position, m_Size);
}

QSizeF GeometryCache::size() const
{
    return m_State == GeometryCache::State::SIZE ?
        m_Size : geometry().size();
}

QPointF GeometryCache::position() const
{
    return m_State == GeometryCache::State::POSITION ?
        m_Position : geometry().topLeft();
}

void GeometryCache::setPosition(const QPointF& pos)
{
    m_Position = pos;
    performAction(Action::PLACE, nullptr, nullptr);
}

void GeometryCache::setSize(const QSizeF& size)
{
    // setSize should not be used to reset the size
    Q_ASSERT(size.isValid());

    m_Size = size;
    performAction(Action::RESIZE, nullptr, nullptr);
}

GeometryCache::State GeometryCache::state() const
{
    return m_State;
}
