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

class QQmlEngine;
class QQmlComponent;
class ContextManager;
class AbstractSelectableView;
class AbstractQuickViewPrivate;
class AbstractViewItem;

/*
 * This class holds the private interface between the view and the items.
 *
 * A previous implementation used protected methods and `friend` classes, but
 * left exposed some methods that made little sense for the general users,
 * including many tuple types.
 *
 * Using such private class adds flexibility when it come to future changes.
 *
 */
class AbstractQuickViewSync
{
public:
    QQmlEngine    *engine   () const;
    QQmlComponent *component() const;

    AbstractQuickViewPrivate* d_ptr;
};
