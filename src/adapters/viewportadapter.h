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
#pragma once

class ViewportAdapterPrivate;

//WARNING on hold for now, nothing is implemented properly

/**
 * Define which QModelIndex get to be loaded at any given time and keep track
 * of the view size.
 */
class Q_DECL_EXPORT ViewportAdapter : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit ViewportAdapter(QObject *parent = nullptr){Q_UNUSED(parent)}
    virtual ~ViewportAdapter(){}

    bool isTotalSizeKnown() const{return false;};

    void setModel(QAbstractItemModel *m){Q_UNUSED(m)};

private:
    ViewportAdapterPrivate *d_ptr;
};
