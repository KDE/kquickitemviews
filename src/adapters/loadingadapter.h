/***************************************************************************
 *   Copyright (C) 2018-2019 by Emmanuel Lepage Vallee                     *
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
#ifndef LOADING_ADAPTER_H
#define LOADING_ADAPTER_H

#include <QtCore/QWidget>

/**
 * This adapter helps to define strategies to choose which delegates to load
 * next. This can depend on the view type (for example a list ignoring all
 * children QModelIndex) or zoom level (for example, in a GIS view).
 */
class Q_DECL_EXPORT LoadingAdapter : public QObject
{
    Q_OBJECT
public:
    explicit LoadingAdapter(QObject *parent = nullptr);
    virtual ~LoadingAdapter();
};

#endif
