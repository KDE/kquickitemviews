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
#ifndef KQUICKITEMVIEWS_VIEWPORTADAPTER_H
#define KQUICKITEMVIEWS_VIEWPORTADAPTER_H

// Qt
#include <QtCore/QObject>
#include <QtCore/QRectF>

// KQuickItemViews
class ViewportAdapterPrivate;
class Viewport;
class AbstractItemAdapter;

//WARNING on hold for now, nothing is implemented properly

/**
 * Define which QModelIndex get to be loaded at any given time and keep track
 * of the view size.
 */
class Q_DECL_EXPORT ViewportAdapter : public QObject
{
    Q_OBJECT
public:

    /**
     * The empty area that can be used by the footer widget.
     *
     * This is only relevant for footer widget that use `anchors.fill` for
     * their size instead of using their implicit size to resize the area.
     */
    Q_PROPERTY(QRectF availableFooterArea READ availableFooterArea NOTIFY footerChanged)


    /**
     * The empty area that can be used by the header widget.
     *
     * This is only relevant for header widget that use `anchors.fill` for
     * their size instead of using their implicit size to resize the area.
     */
    Q_PROPERTY(QRectF availableHeaderArea READ availableHeaderArea NOTIFY headerChanged)

    /**
     * Define how the viewport should move when new elements are inserted.
     *
     * When anchored, the anchor should be preserved even when elements are
     * inserted above and below the viewport in ways that would normally move
     * the viewport.
     */
    Q_PROPERTY(Qt::Edges anchoredEdges READ anchoredEdges NOTIFY anchorsChanged)

    Q_PROPERTY(bool totalSizeKnown READ isTotalSizeKnown NOTIFY totalSizeChanged)

    Q_PROPERTY(QSizeF totalSize READ totalSize NOTIFY totalSizeChanged)

    Q_INVOKABLE explicit ViewportAdapter(Viewport *parent);

    virtual ~ViewportAdapter();

    virtual bool isTotalSizeKnown() const;

    virtual QRectF availableFooterArea() const;
    virtual QRectF availableHeaderArea() const;
    virtual Qt::Edges anchoredEdges() const;
    virtual QSizeF totalSize() const;

    /**
     * Get the size hints for an AbstractItemAdapter.
     *
     * The implementation should take into account the GeometryAdapter size
     * hints and apply its own containts to them. For example a ListView entry
     * takes the full width and a TableView might resize the column based on
     * the content size.
     *
     * @see GeometryAdapter::sizeHint
     */
    Q_INVOKABLE virtual QSizeF sizeHint(const QModelIndex &index, AbstractItemAdapter *adapter) const;

    /**
     * Get the position hints for an AbstractItemAdapter.
     *
     * @see GeometryAdapter::positionHint
     */
    Q_INVOKABLE virtual QPointF positionHint(const QModelIndex &index, AbstractItemAdapter *adapter) const;

protected:

    /**
     * Called when the viewport is resized.
     */
    virtual void onResize(const QRectF& newSize, const QRectF& oldSize);

    /**
     * Called when
     */
    virtual void onEnterViewport(AbstractItemAdapter *item);

    virtual void onLeaveViewport(AbstractItemAdapter *item);

Q_SIGNALS:
    void totalSizeChanged();
    void anchorsChanged();
    void headerChanged();
    void footerChanged();

private:
    ViewportAdapterPrivate *d_ptr;
};

#endif
