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
#include <QtGlobal>

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
 * The geometry is built using 3 components:
 *
 * * The content size
 * * The outer decoration size
 * * The position
 *
 * What this entity doesn't known (and should not ever know)
 *
 * * Anything related about its neighbor.
 *
 */
struct GeometryCache
{
    enum class State {
        INIT    , /*!< The value has not been computed        */
        SIZE    , /*!< The size had been computed             */
        POSITION, /*!< The position has been computed         */
        PENDING , /*!< Has all information, but not assembled */
        VALID   , /*!< The geometry has been computed         */
    };

    enum class Action {
        MOVE    , /*!< When moved                              */
        RESIZE  , /*!< When the content size changes           */
        PLACE   , /*!< When setting the position               */
        RESET   , /*!< The delegate, layout changes, or pooled */
        MODIFY  , /*!< When the QModelIndex role changes       */
        DECORATE, /*!< When the decoration size changes        */
        VIEW    , /*!< When the geometry is accessed           */
    };



    State performAction(Action);

    void setPosition(const QPointF& pos);
    void setSize(const QSizeF& size);
    QSizeF size() const;
    QPointF position() const;
    QRectF decoratedGeometry() const;
    QRectF contentGeometry() const;

    qreal borderDecoration(Qt::Edge e) const;
    void setBorderDecoration(Qt::Edge e, qreal r);

    /// When the geometry can be used
    bool isReady() const;

    State state() const;
private:
    QRectF rawGeometry() const;

    QPointF m_Position;
    QSizeF  m_Size;

    qreal m_lBorderDecoration[4] {0.0, 0.0, 0.0, 0.0};

    typedef void(GeometryCache::*StateF)();

    static const State  m_fStateMap    [5][7];
    static const StateF m_fStateMachine[5][7];

    // Actions
    void nothing();
    void invalidate();
    void error();
    void dropCache();
    void dropSize();
    void dropPos();
    void buildCache();

    State m_State {State::INIT};
};

