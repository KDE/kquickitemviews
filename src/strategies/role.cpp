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

#include <QtCore/QModelIndex>
#include <QtCore/QRectF>
#include <QtCore/QHash>

// KQuickItemViews
#include <viewport.h>
#include <adapters/modeladapter.h>

class RoleStrategiesPrivate
{
public:
    bool    m_Reload           {       true       };
    int     m_SizeRole         { Qt::SizeHintRole };
    int     m_PositionRole     {        -1        };
    QString m_SizeRoleName     {    "sizeHint"    };
    QString m_PositionRoleName {                  };

    void updateName();

    GeometryStrategies::Role *q_ptr;
};

GeometryStrategies::Role::Role(Viewport *parent) : GeometryAdapter(parent),
    d_ptr(new RoleStrategiesPrivate())
{
    d_ptr->q_ptr = this;
    setCapabilities(Capabilities::HAS_AHEAD_OF_TIME);
}

GeometryStrategies::Role::~Role()
{
    delete d_ptr;
}

QSizeF GeometryStrategies::Role::sizeHint(const QModelIndex &index, AbstractItemAdapter *adapter) const
{
    Q_UNUSED(adapter)

    if (d_ptr->m_Reload)
        d_ptr->updateName();

    const auto val = index.data(d_ptr->m_SizeRole);
    Q_ASSERT(val.isValid());

    if (val.type() == QMetaType::QRectF) {
        const QRectF rect = val.toRectF();

        return rect.size();
    }

    Q_ASSERT(val.toSizeF().isValid());

    return val.toSizeF();
}

QPointF GeometryStrategies::Role::positionHint(const QModelIndex &index, AbstractItemAdapter *adapter) const
{
    Q_UNUSED(adapter)
    if (d_ptr->m_Reload)
        d_ptr->updateName();

    const auto val = index.data(d_ptr->m_SizeRole);
    Q_ASSERT(val.isValid());

    if (val.type() == QMetaType::QRectF) {
        const QRectF rect = val.toRectF();

        return rect.topLeft();
    }

    return val.toPointF();
}

int GeometryStrategies::Role::sizeRole() const
{
    return d_ptr->m_SizeRole;
}

void GeometryStrategies::Role::setSizeRole(int role)
{
    d_ptr->m_SizeRole = role;
    d_ptr->updateName();
    emit roleChanged();
}

QString GeometryStrategies::Role::sizeRoleName() const
{
    return d_ptr->m_SizeRoleName;
}

void GeometryStrategies::Role::setSizeRoleName(const QString& roleName)
{
    if (d_ptr->m_SizeRoleName == roleName)
        return;

    d_ptr->m_SizeRoleName = roleName;
    d_ptr->updateName();
    emit roleChanged();
}

int GeometryStrategies::Role::positionRole() const
{
    return d_ptr->m_PositionRole;
}

void GeometryStrategies::Role::setPositionRole(int role)
{
    setCapabilities(
        Capabilities::HAS_AHEAD_OF_TIME | Capabilities::HAS_POSITION_HINTS
    );

    d_ptr->m_PositionRole = role;
    d_ptr->updateName();
    emit roleChanged();
}

QString GeometryStrategies::Role::positionRoleName() const
{
    return d_ptr->m_PositionRoleName;
}

void GeometryStrategies::Role::setPositionRoleName(const QString& roleName)
{
    if (d_ptr->m_PositionRoleName == roleName)
        return;

    d_ptr->m_PositionRoleName = roleName;
    d_ptr->updateName();
    emit roleChanged();
}

void RoleStrategiesPrivate::updateName()
{
    //TODO set the role name from the role index
    const auto ma = q_ptr->viewport()->modelAdapter();

    if (!ma->rawModel()) {
        m_Reload = true;
        return;
    }

    const auto rn  = ma->rawModel()->roleNames();
    const auto rnv = rn.values();

    const QByteArray srn = m_SizeRoleName.toLatin1();
    const QByteArray prn = m_PositionRoleName.toLatin1();

    if ((!srn.isEmpty()) && ma->rawModel() && rnv.contains(srn)) {
        q_ptr->setSizeRole(rn.key(srn));
    }

    if ((!prn.isEmpty()) && ma->rawModel() && rnv.contains(prn)) {
        q_ptr->setPositionRole(rn.key(prn));
    }

    m_Reload = false;
}
