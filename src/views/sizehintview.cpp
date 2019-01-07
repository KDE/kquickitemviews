/***************************************************************************
 *   Copyright (C) 2019 by Emmanuel Lepage Vallee                          *
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
#include "sizehintview.h"

#include <KQuickItemViews/adapters/AbstractItemAdapter>
#include <KQuickItemViews/adapters/ModelAdapter>
#include <KQuickItemViews/Viewport>
#include <KQuickItemViews/strategies/role.h>

class SizeHintViewPrivate
{
public:
    ModelAdapter *m_pModelAdapter;
    Viewport     *m_pViewport;
};

SizeHintView::SizeHintView(QQuickItem* parent) : SingleModelViewBase(new ItemFactory<AbstractItemAdapter>(), parent),
    d_ptr(new SizeHintViewPrivate())
{
    //TODO use ViewBase directly once there is some Q interfaces for its parts.
    d_ptr->m_pModelAdapter = modelAdapters().first();
    d_ptr->m_pViewport     = d_ptr->m_pModelAdapter->viewports().first();
    auto a = new GeometryStrategies::Role(d_ptr->m_pViewport);
    setDelegateSizeForced(true);
    a->setPositionRole(Qt::SizeHintRole);
    d_ptr->m_pViewport->setGeometryAdapter(a);
}

SizeHintView::~SizeHintView()
{
    delete d_ptr;
}
