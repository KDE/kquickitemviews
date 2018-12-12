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
#ifndef KQUICKITEMVIEWS_GEOMETRYADAPTER_H
#define KQUICKITEMVIEWS_GEOMETRYADAPTER_H

#include <QtCore/QObject>
#include <QtCore/QSizeF>
#include <QtCore/QPointF>

class GeometryAdapterPrivate;
class AbstractItemAdapter;
class Viewport;

/**
 * Add strategies to get the size (and optionally position) hints.
 *
 * One size don't fit everything here. QtWidgets had a `sizeHint` method.
 * However getting this information in QML is a lot harder for many reasons.
 */
class Q_DECL_EXPORT GeometryAdapter : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit GeometryAdapter(Viewport *parent = nullptr);
    virtual ~GeometryAdapter();

    /**
     * Features this adapter provide and requires.
     */
    enum Capabilities {
        NONE                      = 0x0 << 0,

       /**
        * Most GeometryAdapter only provide size hints.
        *
        * However some views might not have built-in layouts and then the position
        * is also necessary.
        *
        * This property only state that the adapter *can* provide positions.
        * `alwaysHasPositionHints` will tell if this is always the case.
        */
        HAS_POSITION_HINTS        = 0x1 << 0,

       /**
        * Allow the view to skip the GeometryAdapter when they only need the height.
        *
        * It will be called once, then the view will assume the result will always
        * be the same.
        */
        HAS_UNIFORM_HEIGHT        = 0x1 << 1,

       /**
        * Allow the view to skip the GeometryAdapter when they only need the width.
        *
        * It will be called once, then the view will assume the result will always
        * be the same.
        */
        HAS_UNIFORM_WIDTH         = 0x1 << 2,

       /**
        * The sizeHint will *always* return a valid size.
        *
        * Some GeometryAdapter may only provide size hints when some preconditions
        * are met. For example something that depends on the current delegate
        * will only work once the delegate is created.
        */
        ALWAYS_HAS_SIZE_HINTS     = 0x1 << 3,

        /** @see alwaysHasSizeHints */
        ALWAYS_HAS_POSITION_HINTS = 0x1 << 4,

       /**
        * This GeometryAdapter allows to query the size before creating a
        * delegate instance.
        */
        HAS_AHEAD_OF_TIME         = 0x1 << 5,

       /**
        * This GeometryAdapter requires the view to track each QQuickItem
        * delegate geometry.
        */
        TRACKS_QQUICKITEM_GEOMETRY= 0x1 << 6,

       /**
        * This GeometryAdapter requires a single delegate instance to work.
        */
        REQUIRES_SINGLE_INSTANCE  = 0x1 << 7,
    };
//     Q_FLAGS(Capabilities)

    Q_PROPERTY(int capabilities READ capabilities NOTIFY flagsChanged)

    virtual int capabilities() const;

    /**
     * Get the size hints for an AbstractItemAdapter.
     */
    Q_INVOKABLE virtual QSizeF sizeHint(const QModelIndex &index, AbstractItemAdapter *adapter) const;

    /**
     * Get the position hints for an AbstractItemAdapter.
     */
    Q_INVOKABLE virtual QPointF positionHint(const QModelIndex &index, AbstractItemAdapter *adapter) const;

    Viewport *viewport() const;

protected:
    /**
     * Set the flags to be used by the internal optimization engine.
     */
    void setCapabilities(int f);

    void addCapabilities(int f);

    void removeCapabilities(int f);

Q_SIGNALS:

    /**
     * Tell the view to disregard previously returned values and start over.
     */
    void dismissResult();

    void flagsChanged();


private:
    GeometryAdapterPrivate *d_ptr;
    Q_DECLARE_PRIVATE(GeometryAdapter)
};

#endif
