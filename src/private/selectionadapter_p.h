/***************************************************************************
 *   Copyright (C) 2017 by Emmanuel Lepage Vallee                          *
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

// Qt
class QAbstractItemModel;

class ViewBase;
class Viewport;

// This class exists for the AbstractItemView to be able to internally
// notify the SelectionAdapter about some events. In theory the public
// interface of both of these class should be extended to handle such events
// but for now a private interface allows more flexibility.
class SelectionAdapterSyncInterface
{
public:
    virtual ~SelectionAdapterSyncInterface() {}

    virtual void updateSelection(const QModelIndex& idx);

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* m);

    ViewBase* view() const;
    void setView(ViewBase* v);

    void setViewport(Viewport *v);

    SelectionAdapter* q_ptr;
};
