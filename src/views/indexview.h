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
#ifndef INDEXVIEW_H
#define INDEXVIEW_H

#include <QQuickItem>
#include <QQmlComponent>

class QAbstractItemModel;

class IndexViewPrivate;

/**
 * This view has a single delegate instance and display data for a single
 * QModelIndex.
 *
 * This is useful for mobile application pages to get more information out of
 * a list item. It allows for more compact list while avoiding the boilerplate
 * of having a non-model component.
 *
 * It can also be used to create the equivalent of "editor widgets" from the
 * QtWidgets era.
 *
 * The IndexView will take the implicit width and height of the component
 * unless it is resized or in a managed layout, where it will resize the
 * delegate.
 */
class Q_DECL_EXPORT IndexView : public QQuickItem
{
    Q_OBJECT
public:
    explicit IndexView(QQuickItem *parent = nullptr);
    virtual ~IndexView();

    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(QModelIndex modelIndex READ modelIndex WRITE setModelIndex NOTIFY indexChanged)
    Q_PROPERTY(QAbstractItemModel* model READ model NOTIFY indexChanged)

    virtual void setDelegate(QQmlComponent* delegate);
    QQmlComponent* delegate() const;

    QAbstractItemModel *model() const;

    QModelIndex modelIndex() const;
    void setModelIndex(const QModelIndex &index);

Q_SIGNALS:
    void delegateChanged(QQmlComponent* delegate);
    void indexChanged();

private:
    IndexViewPrivate *d_ptr;
    Q_DECLARE_PRIVATE(IndexView)
};

#endif
