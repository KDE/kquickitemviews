/***************************************************************************
 *   Copyright (C) 2017-2018 by Emmanuel Lepage Vallee                     *
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
#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>

// KQuickViews
struct TreeTraversalItem;
class VisualTreeItem;
class TreeTraversalReflectorPrivate;
class AbstractItemAdapter;
class Viewport;


/**
 * This class refects a QAbstractItemModel (realtime) topology.
 *
 * It helps to handle the various model events to always keep a correct
 * overview of the model content. While the models are trees, this class expose
 * a 2D linked list API. This is "better" for the views because in the end they
 * place widgets in a 2D plane (grid) so geometric navigation makes placing
 * the widget easier.
 *
 * The format this class exposes is not as optimal as it could. However it was
 * chosen because it made the consumer code more readable and removed most of
 * the corner cases that causes QtQuick.ListView to fail to be extended to
 * tables and trees (without exponentially more complexity). It also allows a
 * nice encapsulation and separation of concern which removes the need for
 * extensive and non-reusable private APIs.
 *
 * Note that you should only use this class directly when implementing low level
 * views such as charts or graphs. The `ViewBase` is a better base
 * for most traditional views.
 *
 * The `setAvailableEdges` must be kept up to date by the owner and `populate`
 * or `detachUntil` called to trigger manual changes. Otherwise it will listen
 * to the model and modify itself and the model changes.
 */
class TreeTraversalReflector final : public QObject
{
    Q_OBJECT
    friend struct TreeTraversalItem; // Internal representation
public:

    enum class TrackingState {
        NO_MODEL , /*!< The model is not set, there is nothing to do                  */
        PAUSED   , /*!< The model is set, but the reflector is not listening          */
        POPULATED, /*!< The initial insertion has been done, it is ready for tracking */
        TRACKING , /*!< The model is set and the reflector is listening to changes    */
    };

    enum class TrackingAction {
        POPULATE, /*!< Fetch the model content and fill the view */
        DISABLE , /*!< Disconnect the model tracking             */
        ENABLE  , /*!< Connect the pending model                 */
        RESET   , /*!< Remove the delegates but keep the trackers*/
        FREE    , /*!< Free the whole tracking tree              */
        MOVE    , /*!< Try to fix the viewport with content      */
        TRIM    , /*!< Remove the elements until the edge is free*/
    };

    enum class EdgeType {
        FREE    , /*!<  */
        VISIBLE , /*!<  */
        BUFFERED, /*!<  */
    };

    explicit TreeTraversalReflector(Viewport* parent = nullptr);
    virtual ~TreeTraversalReflector();

    /**
     * Manipulate the tracking state.
     */
    TrackingState performAction(TrackingAction);

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* m);

    void setAvailableEdges(Qt::Edges edges, EdgeType type);
    Qt::Edges availableEdges(EdgeType type) const;

    // Getter
    AbstractItemAdapter* itemForIndex(const QModelIndex& idx) const; //TODO remove
    bool isActive(const QModelIndex& parent, int first, int last); //TODO move to range

    /**
     * The relfector internally track its viewport from the model point of view,
     * which may or may not be the same as the view. This method will detach
     * every element from the edge to the item)
     */
//     bool detachUntil(Qt::Edge from, TreeTraversalItem *to);
//     bool detachUntil(Qt::Edge from, VisualTreeItem *to);

    //TODO remove those temporary helpers once its encapsulated
//     void moveEverything();sdfdsfdsf

//     void resetGeometry();sdfdsfds

    // Setters
    void setItemFactory(std::function<AbstractItemAdapter*()> factory);

// public Q_SLOTS:
//     void resetEverything();

Q_SIGNALS:
    void contentChanged();
    void countChanged  ();

private:
    TreeTraversalReflectorPrivate* d_ptr;
};
