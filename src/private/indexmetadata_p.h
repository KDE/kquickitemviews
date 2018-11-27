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
#include <QtCore/QModelIndex>

namespace StateTracker {
    class ViewItem;
    class Geometry;
    class ModelItem;
    class Proximity;
    class Index;
}

class ContextAdapter;
class Viewport;

class IndexMetadataPrivate;

/**
 * The shared view of a QModelIndex between the various adapters. The
 * TreeTraversalReflector has `StateTracker::ModelItem` and the AbstractItemAdapter
 * has the `StateTracker::ViewItem`. Both are the QModelIndex from their point of view
 * (model for the former and view for the later).
 *
 * This class is the glue between those vision of the QModelIndex and holds
 * the little they can share.
 *
 * Also note that the StateTracker::ModelItem and AbstractItemAdapter have very
 * different life cycle. It is why a more neutral entity is needed to bridge
 * them across the adapters.
 *
 * Note that some methods of this class are implemented in other "cpp" files
 * to give them the visibility of some otherwise private structures. This was
 * done to limit the proliferation of under-used _p.h classes and make them
 * easier to modify as time goes by.
 */
class IndexMetadata final
{

public:
    explicit IndexMetadata(StateTracker::ModelItem *tti, Viewport *p);
    ~IndexMetadata();

    /**
     * The actions to perform on the model change tracking state machine.
     */
    enum class LoadAction {
        SHOW     = 0, /*!< Make visible on screen (or buffer) */
        HIDE     = 1, /*!< Remove from the screen (or buffer) */
        ATTACH   = 2, /*!< Track, but do not show             */
        DETACH   = 3, /*!< Stop tracking for changes          */
        UPDATE   = 4, /*!< Update the element                 */
        MOVE     = 5, /*!< Update the depth and lookup        */
        RESET    = 6, /*!< Flush the visual item              */
        REPARENT = 7, /*!< Move in the item tree                     */
    };

    /**
     * The actions to perform on the geometry tracking state machine.
     */
    enum class GeometryAction {
        MOVE    , /*!< When moved                              */
        RESIZE  , /*!< When the content size changes           */
        PLACE   , /*!< When setting the position               */
        RESET   , /*!< The delegate, layout changes, or pooled */
        MODIFY  , /*!< When the QModelIndex role changes       */
        DECORATE, /*!< When the decoration size changes        */
        VIEW    , /*!< When the geometry is accessed           */
    };

    /**
     * The actions associated with the state of the (cartesian) siblings status
     * if this index.
     */
    enum class ProximityAction {
        QUERY  , /*!<  */
        DISCARD, /*!<  */
        MOVE   , /*!<  */
    };

    /**
     *
     */
    enum class EdgeType {
        FREE    , /*!<  */
        VISIBLE , /*!<  */
        BUFFERED, /*!<  */
        NONE    , /*!<  */
    };

    // Getters
    QRectF decoratedGeometry() const;
    QModelIndex index() const;

    StateTracker::ViewItem  *viewTracker     () const;
    StateTracker::Index     *modelTracker    () const;
    StateTracker::Proximity *proximityTracker() const;
    ContextAdapter          *contextAdapter  () const;

    // Mutator
    bool performAction(LoadAction      a);
    bool performAction(GeometryAction  a);
    bool performAction(ProximityAction a, Qt::Edge e);

    // Navigation
    IndexMetadata *up   () const; //TODO remove, use `next(Qt::Edge)`
    IndexMetadata *down () const; //TODO remove, use `next(Qt::Edge)`
    IndexMetadata *left () const; //TODO remove, use `next(Qt::Edge)`
    IndexMetadata *right() const; //TODO remove, use `next(Qt::Edge)`

    bool isTopItem() const;

    /**
     * Check if the expected geometry and current geometry match.
     *
     * @return True if the view delegate (widget) is in the expected position.
     */
    bool isInSync() const;

    void setViewTracker(StateTracker::ViewItem *i);

    /**
     * Return true when the metadata is complete enough to be displayed.
     *
     * This means the following thing:
     *
     *  * The QModelIndex is valid
     *  * The position is known
     *  * The size is known
     */
    bool isValid() const;

    /**
     * If the object is considered visible by the viewport.
     *
     * Note that it doesn't always means it is directly in on screen. It can
     * be shadowed by something else, 99% out of view or the view decided not
     * to actually display it.
     *
     * It only means the engine fully tracks it and assumes the tracking
     * overhead isn't wasted.
     */
    bool isVisible() const;

    QSizeF sizeHint();

    qreal borderDecoration(Qt::Edge e) const;
    void setBorderDecoration(Qt::Edge e, qreal r);

    void setSize(const QSizeF& s);
    void setPosition(const QPointF& p);

    int removeMe() const; //TODO remove when time permit

private:
    IndexMetadataPrivate *d_ptr;
};
