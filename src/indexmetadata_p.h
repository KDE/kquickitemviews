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

class VisualTreeItem;
class TreeTraversalItem;
class ContextAdapter;
class Viewport;
class GeometryCache;

class IndexMetadataPrivate;

/**
 * The shared view of a QModelIndex between the various adapters. The
 * TreeTraversalReflector has `TreeTraversalItem` and the AbstractItemAdapter
 * has the `VisualTreeItem`. Both are the QModelIndex from their point of view
 * (model for the former and view for the later).
 *
 * This class is the glue between those vision of the QModelIndex and holds
 * the little they can share.
 *
 * Also note that the TreeTraversalItem and AbstractItemAdapter have very
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
    explicit IndexMetadata(TreeTraversalItem *tti, Viewport *p);
    ~IndexMetadata();

    /**
     * The actions to perform on the model change tracking state machine.
     */
    enum class TrackingAction {
        SHOW   = 0, /*!< Make visible on screen (or buffer) */
        HIDE   = 1, /*!< Remove from the screen (or buffer) */
        ATTACH = 2, /*!< Track, but do not show             */
        DETACH = 3, /*!< Stop tracking for changes          */
        UPDATE = 4, /*!< Update the element                 */
        MOVE   = 5, /*!< Update the depth and lookup        */
        RESET  = 6, /*!< Flush the visual item              */
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

    // Getters
    QRectF decoratedGeometry() const;
    QModelIndex index() const;

    VisualTreeItem    *viewTracker   () const;
    TreeTraversalItem *modelTracker  () const;
    ContextAdapter    *contextAdapter() const;

    // Mutator
    bool performAction(TrackingAction a);
    bool performAction(GeometryAction a);

    // Navigation
    IndexMetadata *up   () const;
    IndexMetadata *down () const;
    IndexMetadata *left () const;
    IndexMetadata *right() const;

    bool isTopItem() const;

    /**
     * Check if the expected geometry and current geometry match.
     *
     * @return True if the view delegate (widget) is in the expected position.
     */
    bool isInSync() const;

    void setViewTracker(VisualTreeItem *i);

    /**
     * Return true when the metadata is complete enough to be displayed.
     */
    bool isValid() const;

    QSizeF sizeHint();

    qreal borderDecoration(Qt::Edge e) const;
    void setBorderDecoration(Qt::Edge e, qreal r);

    void setSize(const QSizeF& s);
    void setPosition(const QPointF& p);

    int removeMe() const; //TODO remove when time permit

private:
    IndexMetadataPrivate *d_ptr;
};
