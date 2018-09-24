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
#include <QtCore/QAbstractItemModel>
#include <QtCore/QRectF>

// KQuickItemViews
#include <viewbase.h>
class VisibleRangePrivate;
class VisibleRangeSync;
class AbstractItemAdapter;

/**
* This class exposes a way to track and iterate a subset of the model.
*
* It prevents all of the model reflection to have to be loaded in memory
* and offers a simpler API to access the loaded sections.
*
* This class is for internal use and should not be used by views. Please use
* `ViewBase` for all relevant use cases.
*/
class VisibleRange : public QObject
{
    friend class AbstractItemAdapter; // for the getters defined in visiblerange.cpp
    friend class TreeTraversalReflector; // notify of model changes

    Q_OBJECT
public:

    /// Some strategies to get the item size with or without loading them.
    enum class SizeHintStrategy {
        AOT     , /*!< Load everything ahead of time, doesn't scale but very reliable */
        JIT     , /*!< Do not try to compute the total size, scrollbars wont work     */
        UNIFORM , /*!< Assume all elements have the same size, scales well when true  */
        PROXY   , /*!< Use a QSizeHintProxyModel, require work by all developers      */
        ROLE    , /*!< Use one of the QAbstractItemModel role as size                 */
        DELEGATE, /*!< Assume the view re-implemented ::sizeHint is correct           */
    };

    explicit VisibleRange(ModelAdapter* ma);

    virtual ~VisibleRange() {}

    /**
     * Get the current (cartesian) rectangle represented by this range.
     */
    QRectF currentRect() const;

    QString sizeHintRole() const;
    void setSizeHintRole(const QString& s);

    SizeHintStrategy sizeHintStrategy() const;
    void setSizeHintStrategy(SizeHintStrategy s);

    ModelAdapter *modelAdapter() const;

    QSizeF size() const;

    QPointF position() const;

    bool isTotalSizeKnown() const;

    QSizeF totalSize() const;

    void setItemFactory(ViewBase::ItemFactoryBase *factory);

    //TODO remove
    AbstractItemAdapter* itemForIndex(const QModelIndex& idx) const;

Q_SIGNALS:
    void contentChanged();

private:
    VisibleRangePrivate *d_ptr;
    VisibleRangeSync    *s_ptr;
};
