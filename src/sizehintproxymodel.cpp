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
#include "sizehintproxymodel.h"

#include <QJSValue>

class SizeHintProxyModelPrivate
{
public:
    QJSValue m_Callback;
};

SizeHintProxyModel::SizeHintProxyModel(QObject* parent) : QIdentityProxyModel(parent),
    d_ptr(new SizeHintProxyModelPrivate())
{}

SizeHintProxyModel::~SizeHintProxyModel()
{
    delete d_ptr;
}

void SizeHintProxyModel::setSourceModel(QAbstractItemModel *newSourceModel)
{
    QIdentityProxyModel::setSourceModel(newSourceModel);
}

QJSValue SizeHintProxyModel::callback() const
{
    return d_ptr->m_Callback;
}

void SizeHintProxyModel::setCallback(const QJSValue& value)
{
    d_ptr->m_Callback = value;
}
