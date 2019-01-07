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
#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QRectF>

/**
 * This model help to test models that provide their own size and position
 * hints.
 *
 * Without this, many code path that are not used by the list/table/tree
 * would go untested.
 */
class FreeFloatingModel : public QAbstractListModel
{
    Q_OBJECT

public:
    virtual bool     setData  ( const QModelIndex& index, const QVariant &value, int role ) override;
    virtual QVariant data     ( const QModelIndex& index, int role = Qt::DisplayRole      ) const override;
    virtual int      rowCount ( const QModelIndex& parent = {}                            ) const override;
};
