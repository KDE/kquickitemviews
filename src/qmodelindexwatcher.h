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
#ifndef KQUICKITEMVIEWS_QMODELINDEXWATCHER_H
#define KQUICKITEMVIEWS_QMODELINDEXWATCHER_H

#include <QtCore/QObject>
#include <QtCore/QModelIndex>

class QAbstractItemModel;
class QModelIndexWatcherPrivate;

/**
 * This class allows to get events on a QModelIndex from QML.
 */
class Q_DECL_EXPORT QModelIndexWatcher : public QObject
{
    Q_OBJECT
public:
    explicit QModelIndexWatcher(QObject *parent = nullptr);
    virtual ~QModelIndexWatcher();

    Q_PROPERTY(bool valid READ isValid NOTIFY validChanged)
    Q_PROPERTY(QModelIndex modelIndex READ modelIndex WRITE setModelIndex NOTIFY indexChanged)
    Q_PROPERTY(QAbstractItemModel* model READ model NOTIFY validChanged)

    QModelIndex modelIndex() const;
    void setModelIndex(const QModelIndex &index);

    QAbstractItemModel *model() const;

    bool isValid() const;

Q_SIGNALS:
    void removed();
    void moved();
    void dataChanged(const QVector<int>& roles);
    void validChanged();
    void indexChanged();

private:
    QModelIndexWatcherPrivate *d_ptr;
    Q_DECLARE_PRIVATE(QModelIndexWatcher)
};

#endif
