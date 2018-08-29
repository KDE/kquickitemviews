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
#include <flickableview.h>

struct TreeTraversalItems;
class VisualTreeItem;
class TreeTraversalReflectorPrivate;
class TreeTraversalRange;


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
 * views such as charts or graphs. The `AbstractQuickView` is a better base
 * for most traditional views.
 */
class TreeTraversalReflector final : public QObject
{
    Q_OBJECT
    friend class TreeTraversalItems; // Internal representation
public:

    //TODO find a way to make this private again
    enum class ViewAction { //TODO make this private to AbstractQuickViewPrivate
        ATTACH       = 0, /*!< Activate the element (do not sync it) */
        ENTER_BUFFER = 1, /*!< Sync all roles                        */
        ENTER_VIEW   = 2, /*!< NOP (todo)                            */
        UPDATE       = 3, /*!< Reload the roles                      */
        MOVE         = 4, /*!< Move to a new position                */
        LEAVE_BUFFER = 5, /*!< Stop keeping track of data changes    */
        DETACH       = 6, /*!< Delete                                */
    };

    explicit TreeTraversalReflector(QObject* parent = nullptr);
    virtual ~TreeTraversalReflector();

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* m);
    void populate();

    /*
     * Invert responsibilities so the VisualTreeItem don't need
     * to know anything about itself it should not have to care about. The
     * TreeTraversalItems are the "source of truth" when it comes to interfacing
     * with the model. Implementing those methods directly in the VisualTreeItem
     * would force to expose some internal state.
     *
     */
    /*inline*/ VisualTreeItem* up    (const VisualTreeItem* i) const;
    /*inline*/ VisualTreeItem* down  (const VisualTreeItem* i) const;
    /*inline*/ VisualTreeItem* left  (const VisualTreeItem* i) const;
    /*inline*/ VisualTreeItem* right (const VisualTreeItem* i) const;
    /*inline*/ int row   (const VisualTreeItem* i) const;
    /*inline*/ int column(const VisualTreeItem* i) const;

    //TODO move this to the range once it works
    VisualTreeItem* getCorner(TreeTraversalRange* r, Qt::Corner c) const;

    // Getter
    VisualTreeItem* parentTreeItem(const QModelIndex& idx) const;
    VisualTreeItem* itemForIndex(const QModelIndex& idx) const;

    //TODO remove those temporary helpers once its encapsulated
    /*inline*/ QModelIndex indexForItem(const VisualTreeItem* i) const;
    /*inline*/ bool isItemVisible(const VisualTreeItem* i) const;
    void refreshEverything();
    void reloadRange(const QModelIndex& idx);
    void moveEverything();

    // Mutator
    bool addRange(TreeTraversalRange* range);
    bool removeRange(TreeTraversalRange* range);
    QList<TreeTraversalRange*> ranges() const;

    // Setters
    void setItemFactory(std::function<VisualTreeItem*()> factory);

    // factory
    VisualTreeItem* createItem() const;

    // Tests
    void _test_validateTree(TreeTraversalItems* p);

    // Helpers
    bool isActive(const QModelIndex& parent, int first, int last);
    TreeTraversalItems* addChildren(TreeTraversalItems* parent, const QModelIndex& index);
    void bridgeGap(TreeTraversalItems* first, TreeTraversalItems* second, bool insert = false);
    void createGap(TreeTraversalItems* first, TreeTraversalItems* last  );
    TreeTraversalItems* ttiForIndex(const QModelIndex& idx) const;

    void setTemporaryIndices(const QModelIndex &parent, int start, int end,
                             const QModelIndex &destination, int row);
    void resetTemporaryIndices(const QModelIndex &parent, int start, int end,
                               const QModelIndex &destination, int row);

public Q_SLOTS:
    void cleanup();
    void slotRowsInserted  (const QModelIndex& parent, int first, int last);
    void slotRowsRemoved   (const QModelIndex& parent, int first, int last);
    void slotLayoutChanged (                                              );
    void slotRowsMoved     (const QModelIndex &p, int start, int end,
                            const QModelIndex &dest, int row);
    void slotRowsMoved2    (const QModelIndex &p, int start, int end,
                            const QModelIndex &dest, int row);

Q_SIGNALS:
    void contentChanged();
    void countChanged  ();

private:
    TreeTraversalReflectorPrivate* d_ptr;
};