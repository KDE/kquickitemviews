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

#include <QtCore/QModelIndex>
#include <QtCore/QList>

/**
 * Internal navigation structure designed to support partial tree loading.
 *
 * This isn't intended to be used directly. It exists as its own class because
 * previous iterations were embedded within the model event slots and it made
 * it generally unmaintainable. The idea is to provide atomic and batch
 * operations that can be unit tested individually (eventually).
 *
 * * Support continuous but incomplete sets of indices
 * * Support indices currently being moved or removed
 * * Support moving and reparenting
 * * Provide both a geometric/cartesian and tree navigation API.
 */
struct TreeTraversalBase
{
public:
    enum class LifeCycleState {
        NEW        , /*!< Not part of a tree yet                              */
        NORMAL     , /*!< There is a valid index and a parent node            */
        TRANSITION , /*!< Between rowsAbouttoMove and rowsMoved (and removed) */
        ROOT       , /*!< This is the root element                            */
    };

    virtual ~TreeTraversalBase();

    static void insertChildBefore(TreeTraversalBase* self, TreeTraversalBase* other, TreeTraversalBase* parent);
    static void insertChildAfter(TreeTraversalBase* self, TreeTraversalBase* other, TreeTraversalBase* parent);

    TreeTraversalBase *firstChild() const;
    TreeTraversalBase *lastChild() const;

    TreeTraversalBase *nextSibling() const;
    TreeTraversalBase *previousSibling() const;

    TreeTraversalBase *parent() const;

    // Geometric navigation
    TreeTraversalBase* up   () const;
    TreeTraversalBase* down () const;
    TreeTraversalBase* left () const;
    TreeTraversalBase* right() const;

    int effectiveRow() const;
    int effectiveColumn() const;
    QPersistentModelIndex effectiveParentIndex() const;

//     void reparentUntil(TreeTraversalBase *np, TreeTraversalBase *until);

    void remove(bool reparent = false);
    static void bridgeGap(TreeTraversalBase* first, TreeTraversalBase* second);
//     static void createGap(TreeTraversalBase* first, TreeTraversalBase* last  );

    TreeTraversalBase *childrenLookup(const QPersistentModelIndex &index) const;
    bool hasChildren(TreeTraversalBase *child) const;
    int loadedChildrenCount() const;
    QList<TreeTraversalBase*> allLoadedChildren() const;
    bool withinRange(QAbstractItemModel* m, int last, int first) const;

    void resetTemporaryIndex();
    bool hasTemporaryIndex();
    void setTemporaryIndex(const QModelIndex& newParent, int row, int column);

    // Runtime tests
    void _test_validate_chain() const;

    QPersistentModelIndex index() const;
    void setModelIndex(const QPersistentModelIndex& idx);

    LifeCycleState m_LifeCycleState {LifeCycleState::NEW};
private:
    // Because slotRowsMoved is called before the change take effect, cache
    // the "new real row and column" since the current index()->row() is now
    // garbage.
    int m_MoveToRow    {-1};
    int m_MoveToColumn {-1};
    QPersistentModelIndex m_MoveToParent;

    TreeTraversalBase* m_tSiblings[2] = {nullptr, nullptr};
    TreeTraversalBase* m_tChildren[2] = {nullptr, nullptr};
    // Keep the parent to be able to get back to the root
    TreeTraversalBase* m_pParent {nullptr};
    QPersistentModelIndex m_Index;


    //TODO use a btree, not an hash
    QHash<QPersistentModelIndex, TreeTraversalBase*> m_hLookup;
};

/**
 * Keep track of state "edges".
 *
 * This allows to manipulate rectangles of QModelIndex similar to QtCore
 * dataChanged and other signals.
 */
class ModelRect final
{
public:
    TreeTraversalBase* getEdge(Qt::Edge e) const;
    void setEdge(TreeTraversalBase* tti, Qt::Edge e);

    Qt::Edges m_Edges {Qt::TopEdge|Qt::LeftEdge|Qt::RightEdge|Qt::BottomEdge};
private:
    enum Pos {Top, Left, Right, Bottom};
    int edgeToIndex(Qt::Edge e) const;
    TreeTraversalBase *m_lpEdges[Pos::Bottom+1] {nullptr, nullptr, nullptr, nullptr}; //TODO port to ModelRect
};
