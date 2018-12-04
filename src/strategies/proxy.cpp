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
#include "proxy.h"

#include <proxies/sizehintproxymodel.h>
#include <viewport.h>
#include <adapters/modeladapter.h>

GeometryStrategies::Proxy::Proxy(Viewport *parent) : GeometryAdapter(parent)
{
    setCapabilities(Capabilities::HAS_AHEAD_OF_TIME);
}

GeometryStrategies::Proxy::~Proxy()
{}

QSizeF GeometryStrategies::Proxy::sizeHint(const QModelIndex &index, AbstractItemAdapter *adapter) const
{
    Q_UNUSED(adapter)

    //Q_ASSERT(adapter->d_ptr->m_pViewport->modelAdapter()->hasSizeHints());

    return qobject_cast<SizeHintProxyModel*>(
        viewport()->modelAdapter()->rawModel()
    )->sizeHintForIndex(index);
}
