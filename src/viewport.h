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
#ifndef KQUICKITEMVIEWS_VIEWPORT_H
#define KQUICKITEMVIEWS_VIEWPORT_H

// Qt
#include <QtCore/QAbstractItemModel>
#include <QtCore/QRectF>

// KQuickItemViews
#include <viewbase.h>
class ViewportPrivate;
class ViewportSync;
class AbstractItemAdapter;
class GeometryAdapter;

/**
* This class exposes a way to track and iterate a subset of the model.
*
* It prevents all of the model reflection to have to be loaded in memory
* and offers a simpler API to access the loaded sections.
*
* This class is for internal use and should not be used by views. Please use
* `ViewBase` for all relevant use cases.
*/
class Q_DECL_EXPORT Viewport : public QObject
{
    friend class AbstractItemAdapter; // for the getters defined in viewport.cpp
    friend class ViewportSync; // its own internal API

    Q_OBJECT
public:
    explicit Viewport(ModelAdapter* ma);
    virtual ~Viewport();

    /**
     * Get the current (cartesian) rectangle represented by this range.
     */
    QRectF currentRect() const;

    ModelAdapter *modelAdapter() const;

    QSizeF size() const;

    QPointF position() const;

    QSizeF totalSize() const;

    Qt::Edges availableEdges() const;

    GeometryAdapter *geometryAdapter() const;
    void setGeometryAdapter(GeometryAdapter *a);

    void setItemFactory(ViewBase::ItemFactoryBase *factory);

    void resize(const QRectF& rect);

    ViewportSync *s_ptr;

Q_SIGNALS:
    void contentChanged();

public:
    ViewportPrivate *d_ptr;
};

#endif
