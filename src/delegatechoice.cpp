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
#include "delegatechoice.h"

// Qt
#include <QtCore/QModelIndex>

// KQukckItemViews
#include <delegatechooser.h>

using Modes = DelegateChooser::Modes;

class DelegateChoicePrivate
{
public:
    QPersistentModelIndex m_Index     {       };
    int                   m_Column    {  -1   };
    int                   m_Row       {  -1   };
    int                   m_Depth     {  -1   };
    QQmlScriptString      m_Expr      {       };
    QQmlComponent        *m_pDelegate {nullptr};
    QVariant              m_RoleValue {       };
};

DelegateChoice::DelegateChoice(QObject *parent) :
    QObject(parent), d_ptr(new DelegateChoicePrivate())
{}

DelegateChoice::~DelegateChoice()
{
    delete d_ptr;
}

int DelegateChoice::column() const
{
    return d_ptr->m_Column;
}

void DelegateChoice::setColumn(int c)
{
    d_ptr->m_Column = c;
}

int DelegateChoice::row() const
{
    return d_ptr->m_Row;
}

void DelegateChoice::setRow(int r)
{
    d_ptr->m_Row = r;
}

int DelegateChoice::depth() const
{
    return d_ptr->m_Depth;
}

void DelegateChoice::setDepth(int d)
{
    d_ptr->m_Depth = d;
}

QVariant DelegateChoice::index() const
{
    return d_ptr->m_Index;
}

void DelegateChoice::setIndex(const QVariant& i)
{
    d_ptr->m_Index = qvariant_cast<QModelIndex>(i);
}

QQmlComponent *DelegateChoice::delegate() const
{
    return d_ptr->m_pDelegate;
}

void DelegateChoice::setDelegate(QQmlComponent *d)
{
    d_ptr->m_pDelegate = d;
}

QQmlScriptString DelegateChoice::when() const
{
    return d_ptr->m_Expr;
}

void DelegateChoice::setWhen(const QQmlScriptString &w)
{
    d_ptr->m_Expr = w;
}

QVariant DelegateChoice::roleValue() const
{
    return d_ptr->m_RoleValue;
}

void DelegateChoice::setRoleValue(const QVariant &v)
{
    d_ptr->m_RoleValue = v;
}

bool DelegateChoice::evaluateIndex(const QModelIndex& idx)
{
    return false;
}

bool DelegateChoice::evaluateRow(int row, const QModelIndex& parent)
{
    return false;
}

bool DelegateChoice::evaluateColumn(int row, const QModelIndex& parent)
{
    return false;
}
