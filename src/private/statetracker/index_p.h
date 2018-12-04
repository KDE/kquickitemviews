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

#include <private/geoutils_p.h>
#include <private/indexmetadata_p.h>

class Viewport;

namespace StateTracker {

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
class Index
{
public:
    enum class LifeCycleState {
        NEW        , /*!< Not part of a tree yet                              */
        NORMAL     , /*!< There is a valid index and a parent node            */
        TRANSITION , /*!< Between rowsAbouttoMove and rowsMoved (and removed) */
        ROOT       , /*!< This is the root element                            */
    };

    explicit Index(Viewport *p) : m_Geometry(this, p) {}
    virtual ~Index();

    static void insertChildBefore(Index* self, StateTracker::Index* other, StateTracker::Index* parent);
    static void insertChildAfter(Index* self, StateTracker::Index* other, StateTracker::Index* parent);

    Index *firstChild() const;
    Index *lastChild () const;

    Index *nextSibling    () const;
    Index *previousSibling() const;

    Index *parent() const;

    // Geometric navigation
    Index* up   () const; //TODO remove
    Index* down () const; //TODO remove
    Index* left () const; //TODO remove
    Index* right() const; //TODO remove

    Index* next(Qt::Edge e) const;

    int depth() const;

    int effectiveRow() const;
    int effectiveColumn() const;
    QPersistentModelIndex effectiveParentIndex() const;

    virtual void remove(bool reparent = false);
    static void bridgeGap(Index* first, StateTracker::Index* second);

    Index *childrenLookup(const QPersistentModelIndex &index) const;
    bool hasChildren(Index *child) const;
    int loadedChildrenCount() const;
    QList<Index*> allLoadedChildren() const;
    bool withinRange(QAbstractItemModel* m, int last, int first) const;

    void resetTemporaryIndex();
    bool hasTemporaryIndex();
    void setTemporaryIndex(const QModelIndex& newParent, int row, int column);


    QPersistentModelIndex index() const;
    void setModelIndex(const QPersistentModelIndex& idx);

    LifeCycleState lifeCycleState() const {return m_LifeCycleState;}

    IndexMetadata *metadata() const;

    // Runtime tests
#ifdef ENABLE_EXTRA_VALIDATION
    void _test_validate_chain() const;
    static void _test_bridgeGap(Index *first, Index *second);
#endif

private:
    // Because slotRowsMoved is called before the change take effect, cache
    // the "new real row and column" since the current index()->row() is now
    // garbage.
    int m_MoveToRow    {-1};
    int m_MoveToColumn {-1};
    QPersistentModelIndex m_MoveToParent;

    uint m_Depth {0};

    LifeCycleState m_LifeCycleState {LifeCycleState::NEW};

    Index* m_tSiblings[2] = {nullptr, nullptr};
    Index* m_tChildren[2] = {nullptr, nullptr};
    // Keep the parent to be able to get back to the root
    Index* m_pParent {nullptr};
    QPersistentModelIndex m_Index;

    //TODO use a btree, not an hash
    QHash<QPersistentModelIndex, Index*> m_hLookup;
    mutable IndexMetadata m_Geometry;
};

}

/**
 * TODO get rid of this and implement paging
 *
 * Keep track of state "edges".
 *
 * This allows to manipulate rectangles of QModelIndex similar to QtCore
 * dataChanged and other signals.
 */
class ModelRect final
{
public:
    StateTracker::Index* getEdge(Qt::Edge e) const;
    void setEdge(StateTracker::Index* tti, Qt::Edge e);

    Qt::Edges m_Edges {Qt::TopEdge|Qt::LeftEdge|Qt::RightEdge|Qt::BottomEdge};
private:
    GeoRect<StateTracker::Index*> m_lpEdges; //TODO port to ModelRect
};

// Inject some extra validation when executed in debug mode.
#ifdef ENABLE_EXTRA_VALIDATION
 #define _DO_TEST_IDX(f, self, ...) self->f(__VA_ARGS__);
 #define _DO_TEST_IDX_STATIC(f, ...) f(__VA_ARGS__);
#else
 #define _DO_TEST_IDX(f, self, ...) /*NOP*/;
 #define _DO_TEST_IDX_STATIC(f, ...) /*NOP*/;
#endif
