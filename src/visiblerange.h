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

#include <QtCore/QAbstractItemModel>
#include <QtCore/QRectF>

class AbstractViewItem;
class AbstractQuickView;
class ModelAdapter;

class VisibleRangePrivate;
class VisibleRangeSync;

/**
* This class exposes a way to track and iterate a subset of the model.
*
* It prevents all of the model reflection to have to be loaded in memory
* and offers a simpler API to access the loaded sections.
*
* This class is for internal use and should not be used by views. Please use
* `AbstractQuickView` for all relevant use cases.
*/
class VisibleRange
{
    friend class AbstractViewItem; // for the getters defined in visiblerange.cpp
    friend class TreeTraversalReflector; // notify of model changes
public:

    /// Some strategies to get the item size with or without loading them.
    enum class SizeHintStrategy {
        AOT    , /*!< Load everything ahead of time, doesn't scale but very reliable */
        JIT    , /*!< Do not try to compute the total size, scrollbars wont work     */
        UNIFORM, /*!< Assume all elements have the same size, scales well when true  */
        PROXY  , /*!< Use a QSizeHintProxyModel, require work by all developers      */
        ROLE   , /*!< Use one of the QAbstractItemModel role as size                 */
    };

    explicit VisibleRange(ModelAdapter* ma);
    virtual ~VisibleRange() {}

    /**
     * A generic iterator for the ModelIndexItem interface.
     *
     * The user of this class should not have to care about the model indices
     * themselves but rather the subset they "really" display.
     */
    class Iterator {
    public:
        explicit Iterator ();
        bool operator!= (const Iterator& other) const;
        AbstractViewItem* operator* () const;
        const Iterator& operator++ ();

    private:
        const Iterator *p_vec_;
    };

    /**
     * Iterate a range subset from a (relative) root index.
     */
    class Subset {
    public:
        Iterator constBegin() const;
        Iterator constEnd() const;
    };

    //Iterator();

    //Murator

    /// Track an additional QModelIndex toward to model root.
    bool incrementUpward();
    /// Track an additional QModelIndex toward to model tail.
    bool incrementDownward();
    /// Untrack an additional QModelIndex from the model root.
    bool decrementUpward();
    /// Untrack an additional QModelIndex from the model tail.
    bool decrementDownward();

    /**
     * Get an iterator for an index and all its children indices.
     */
    const Subset subset(const QModelIndex& idx) const;

    /**
     * Get the current (cartesian) rectangle represented by this range.
     */
    QRectF currentRect() const;

    QString sizeHintRole() const;
    void setSizeHintRole(const QString& s);

    SizeHintStrategy sizeHintStrategy() const;
    void setSizeHintStrategy(SizeHintStrategy s);

    virtual void applyModelChanges(QAbstractItemModel* m);

    ModelAdapter *modelAdapter() const;

    QSizeF size() const;
    void setSize(const QSizeF &size);

    QPointF position() const;
    void setPosition(const QPointF& point);

    //Iterators
    /*Iterator begin();
    Iterator end();*/

private:
    VisibleRangePrivate *d_ptr;
    VisibleRangeSync    *s_ptr;
};
