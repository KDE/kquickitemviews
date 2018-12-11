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
#include "delegatechooser.h"

using Modes = DelegateChooser::Modes;

class DelegateChooserPrivate
{
public:
    Modes m_Mode {Modes::Undefined};
    QQmlListProperty<DelegateChoice> m_Choices;
    QString m_Role;
};

Modes DelegateChooser::mode() const
{
    return d_ptr->m_Mode;
}

void DelegateChooser::setMode(Modes m)
{
    d_ptr->m_Mode = m;
}

QQmlListProperty<DelegateChoice> DelegateChooser::choices()
{
    return d_ptr->m_Choices;
}

bool DelegateChooser::evaluateIndex(const QModelIndex& idx)
{
    return false;
}

bool DelegateChooser::evaluateRow(int row, const QModelIndex& parent)
{
    return false;
}

bool DelegateChooser::evaluateColumn(int row, const QModelIndex& parent)
{
    return false;
}

QString DelegateChooser::role() const
{
    return d_ptr->m_Role;
}

void DelegateChooser::setRole(const QString &role)
{
    d_ptr->m_Role = role;
}
