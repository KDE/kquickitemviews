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
#include "uniform.h"

#include <adapters/abstractitemadapter.h>
#include <private/statetracker/viewitem_p.h>

GeometryStrategies::Uniform::Uniform(Viewport *parent) : GeometryAdapter(parent)
{
    setCapabilities(
        Capabilities::HAS_UNIFORM_HEIGHT |
        Capabilities::HAS_UNIFORM_WIDTH
    );
}

GeometryStrategies::Uniform::~Uniform()
{}

QSizeF GeometryStrategies::Uniform::sizeHint(const QModelIndex &index, AbstractItemAdapter *adapter) const
{
    Q_UNUSED(index)
    Q_ASSERT(adapter);
    Q_ASSERT(false);
    return {};
}
