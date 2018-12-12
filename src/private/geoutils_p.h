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
#ifndef KQUICKITEMVIEWS_GEOUTILS_P_H
#define KQUICKITEMVIEWS_GEOUTILS_P_H

class GeoUtils
{
public:
    enum class Pos {Top, Left, Right, Bottom};

//     static QPair<Qt::Edge,Qt::Edge> fromGravity() const;

    static constexpr Qt::Edge fromPos(Pos p) {
        return edgeMap[(int)p];
    }

    static constexpr Pos fromEdge(Qt::Edge e) {
        switch(e) {
            case Qt::TopEdge:
                return Pos::Top;
            case Qt::LeftEdge:
                return Pos::Left;
            case Qt::RightEdge:
                return Pos::Right;
            case Qt::BottomEdge:
                return Pos::Bottom;
        }
        return {};
    }

private:
    static constexpr const Qt::Edge edgeMap[] = {
        Qt::TopEdge, Qt::LeftEdge, Qt::RightEdge, Qt::BottomEdge
    };
//     Qt::TopEdge, Qt::LeftEdge, Qt::RightEdge, Qt::BottomEdge
};

template<typename T>
class GeoRect
{
public:
    T get(Qt::Edge e) const {
        return get(GeoUtils::fromEdge(e));
    }

    T get(GeoUtils::Pos p) const {
        return m_lRect[(int)p];
    }

    void set(T v, Qt::Edge e) {
        set(v, GeoUtils::fromEdge(e));
    }

    void set(T v, GeoUtils::Pos p) {
        m_lRect[(int)p] = v;
    }

    //TODO implement operator[]
private:
    T m_lRect[4] = {{}, {}, {}, {}};
};

// QPair<Qt::Edge,Qt::Edge> ViewportPrivate::fromGravity() const
// {
//     switch (m_pModelAdapter->view()->gravity()) {
//         case Qt::Corner::TopLeftCorner:
//             return {Qt::Edge::TopEdge, Qt::Edge::LeftEdge};
//         case Qt::Corner::TopRightCorner:
//             return {Qt::Edge::TopEdge, Qt::Edge::RightEdge};
//         case Qt::Corner::BottomLeftCorner:
//             return {Qt::Edge::BottomEdge, Qt::Edge::LeftEdge};
//         case Qt::Corner::BottomRightCorner:
//             return {Qt::Edge::BottomEdge, Qt::Edge::RightEdge};
//     }
//
//     Q_ASSERT(false);
//     return {};
// }

// static Pos edgeToPos(Qt::Edge e)
// {
//     switch(e) {
//         case Qt::TopEdge:
//             return Pos::Top;
//         case Qt::LeftEdge:
//             return Pos::Left;
//         case Qt::RightEdge:
//             return Pos::Right;
//         case Qt::BottomEdge:
//             return Pos::Bottom;
//     }
//
//     Q_ASSERT(false);
//
//     return {};
// }

#endif
