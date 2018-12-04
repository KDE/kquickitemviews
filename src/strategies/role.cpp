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
#include "role.h"

class RoleStrategiesPrivate
{
public:
    int     m_Role     { Qt::SizeHintRole };
    QString m_RoleName {    "sizeHint"    };

    void updateName();
};

GeometryStrategies::Role::Role(Viewport *parent) : GeometryAdapter(parent),
    d_ptr(new RoleStrategiesPrivate())
{
    setCapabilities(Capabilities::HAS_AHEAD_OF_TIME);
}

GeometryStrategies::Role::~Role()
{
    delete d_ptr;
}

QSizeF GeometryStrategies::Role::sizeHint(const QModelIndex &index, AbstractItemAdapter *adapter) const
{
    Q_UNUSED(index)
    Q_UNUSED(adapter)
    Q_ASSERT(false); //TODO
    return {};
}

int GeometryStrategies::Role::role() const
{
    return d_ptr->m_Role;
}

void GeometryStrategies::Role::setRole(int role)
{
    d_ptr->m_Role = role;
    d_ptr->updateName();
    emit roleChanged();
}

QString GeometryStrategies::Role::roleName() const
{
    return d_ptr->m_RoleName;
}

void GeometryStrategies::Role::setRoleName(const QString& roleName)
{
    d_ptr->m_RoleName = roleName;
    d_ptr->updateName();
    emit roleChanged();
}

void RoleStrategiesPrivate::updateName()
{
    /*d_ptr->m_SizeHintRole = s.toLatin1();

    if (!d_ptr->m_SizeHintRole.isEmpty() && d_ptr->m_pModelAdapter->rawModel())
        d_ptr->m_SizeHintRoleIndex = d_ptr->m_pModelAdapter->rawModel()->roleNames().key(
            d_ptr->m_SizeHintRole
        );*/

    Q_ASSERT(false); //TODO
}
