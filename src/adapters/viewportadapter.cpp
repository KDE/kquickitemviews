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
#include "viewportadapter.h"

// KQuickItemViews
#include <viewport.h>
#include <adapters/geometryadapter.h>

class ViewportAdapterPrivate : public QObject
{
public:
    Viewport *m_pViewport        {nullptr};
    bool      m_IsTotalSizeKnown { false };

    // Helper
    GeometryAdapter *geoAdapter() const;
};

ViewportAdapter::ViewportAdapter(Viewport *parent) : QObject(parent),
    d_ptr(new ViewportAdapterPrivate())
{
    Q_ASSERT(parent);
    d_ptr->m_pViewport = parent;
}

ViewportAdapter::~ViewportAdapter()
{
    delete d_ptr;
}

bool ViewportAdapter::isTotalSizeKnown() const
{
    return d_ptr->m_IsTotalSizeKnown;
}

QRectF ViewportAdapter::availableFooterArea() const
{
    return {};
}

QRectF ViewportAdapter::availableHeaderArea() const
{
    return {};
}

Qt::Edges ViewportAdapter::anchoredEdges() const
{
    return {};
}

QSizeF ViewportAdapter::totalSize() const
{
    return {};
}

void ViewportAdapter::onResize(const QRectF& newSize, const QRectF& oldSize)
{
    Q_UNUSED(newSize)
    Q_UNUSED(oldSize)
}

void ViewportAdapter::onEnterViewport(AbstractItemAdapter *item)
{
    Q_UNUSED(item)
}

void ViewportAdapter::onLeaveViewport(AbstractItemAdapter *item)
{
    Q_UNUSED(item)
}

QSizeF ViewportAdapter::sizeHint(const QModelIndex &index, AbstractItemAdapter *adapter) const
{
    return d_ptr->m_pViewport->geometryAdapter()->sizeHint(index, adapter);
}

QPointF ViewportAdapter::positionHint(const QModelIndex &index, AbstractItemAdapter *adapter) const
{
    return d_ptr->m_pViewport->geometryAdapter()->positionHint(index, adapter);
}
