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
#include "freefloatingmodel.h"

struct FreeFloatingNode {
    QRectF  pos;
    QString name;
};

static FreeFloatingNode ffn[3] = {
    {{10.0,10.0 ,10.0 ,10.0 }, "one"  },
    {{50.0,50.0 ,50.0 ,50.0 }, "two"  },
    {{0.0 ,200.0,100.0,100.0}, "three"},
};

bool FreeFloatingModel::setData( const QModelIndex& index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;

    if (role == Qt::SizeHintRole) {
        ffn[index.row()].pos = value.toRectF();
        emit dataChanged(index, index);
        return true;
    }

    return false;
}

QVariant FreeFloatingModel::data( const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    if (role == Qt::DisplayRole)
        return ffn[index.row()].name;
    else if (role == Qt::SizeHintRole)
        return ffn[index.row()].pos;

    return {};
}

int FreeFloatingModel::rowCount( const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 3;
}
