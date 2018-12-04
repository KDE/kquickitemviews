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
#include "geometryadapter.h"

#include <viewport.h>

class GeometryAdapterPrivate
{
public:
    bool m_hasPositionHints {false};
    bool m_hasUniformHeight {false};
    bool m_hasUniformWidth  {false};

    int m_Flags {GeometryAdapter::Capabilities::NONE};

    Viewport *m_pViewport;
};

GeometryAdapter::GeometryAdapter(Viewport *parent) : QObject(parent),
    d_ptr(new GeometryAdapterPrivate())
{
    d_ptr->m_pViewport = parent;
}

GeometryAdapter::~GeometryAdapter()
{
    delete d_ptr;
}

Viewport *GeometryAdapter::viewport() const
{
    return d_ptr->m_pViewport;
}

QPointF GeometryAdapter::positionHint(const QModelIndex &index, AbstractItemAdapter *adapter) const
{
    Q_UNUSED(adapter)
    Q_UNUSED(index)
    return {};
}

QSizeF GeometryAdapter::sizeHint(const QModelIndex &index, AbstractItemAdapter *adapter) const
{
    Q_UNUSED(adapter)
    Q_UNUSED(index)

    return {};
}

void GeometryAdapter::setCapabilities(int f)
{
    if (f == d_ptr->m_Flags)
        return;

    d_ptr->m_Flags = f;

    emit flagsChanged();
}

void GeometryAdapter::addCapabilities(int f)
{
    if ((d_ptr->m_Flags | f) == d_ptr->m_Flags)
        return;

    d_ptr->m_Flags |= f;

    emit flagsChanged();
}

void GeometryAdapter::removeCapabilities(int f)
{
    int newFlags = d_ptr->m_Flags & (~f);

    if (d_ptr->m_Flags == newFlags)
        return;

    d_ptr->m_Flags = newFlags;

    emit flagsChanged();
}

int GeometryAdapter::capabilities() const
{
    return d_ptr->m_Flags;
}
