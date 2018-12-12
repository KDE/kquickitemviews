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
#ifndef KQUICKITEMVIEWS_PROXIMITY_P_H
#define KQUICKITEMVIEWS_PROXIMITY_P_H

#include <QtCore/QRectF>
#include <QtCore/QModelIndexList>

#include <private/indexmetadata_p.h>

class ProximityPrivate;

namespace StateTracker {

class Index;

/**
 * Track if the elements next to this one are loaded.
 *
 * This tries to prevent accidental "holes" with unloaded elements between
 * two visible ones.
 *
 * This state tracker doesn't know about what's loaded and what's not. To
 * make it work, it is important to send all relevent events otherwise it will
 * go out of sync and start doing useless model queries (but hopefully nothing
 * worst).
 */
class Proximity
{
public:
    explicit Proximity(IndexMetadata *q, StateTracker::Index *self);

    enum class State {
        UNKNOWN , /*!< The information is not availablr           */
        LOADED  , /*!< The edges are valid                        */
        MOVED   , /*!< It was valid, but some elements moved      */
        UNLOADED, /*!< It is known that some edges are not loaded */
    };

    void performAction(IndexMetadata::ProximityAction a, Qt::Edge e);

    bool canLoadMore(Qt::Edge e);

    QModelIndexList getNext(Qt::Edge e);

private:
    ProximityPrivate *d_ptr;
};

}

#endif
