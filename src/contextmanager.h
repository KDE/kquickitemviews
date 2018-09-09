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

#include <QtCore/QObject>
class QAbstractItemModel;
class QQmlContext;
class QQuickItem;

class ContextManagerPrivate;

/**
 * This class manage the content of the AbstractViewItem::context().
 *
 * It dynamically compute the roles used by the delegate and optimize
 * the updates.
 *
 * This avoids calling setProperty on every single role and refresh them in
 * every single `emit dataChanged`.
 *
 * `applyRoles` can be re-implemented to either add more properties to the
 * context or to convert some roles into a format easier to consume from QML.
 */
class ContextManager : public QObject
{
    Q_OBJECT
public:
    explicit ContextManager(QObject* parent = nullptr);
    virtual ~ContextManager();

    void setModel(QAbstractItemModel* m);
    QAbstractItemModel* model() const;

    /**
     * This will create a new QObject attached
     */
    void registerQuickItem(QQmlContext* ctx, QQuickItem* i);

    void updateRoles(const QModelIndex& index, QQmlContext* ctx, const QVector<int> &modified) const;

    /**
     * TODO merge with updateRoles
     */
    virtual void applyRoles(QQmlContext* ctx, const QModelIndex& self) const;

Q_SIGNALS:
    /**
     * Filtered version of the dataChanged
     */
    void dataChanged(const QModelIndex& tl, const QModelIndex& br, const QVector<int> &roles);

private:
    ContextManagerPrivate* d_ptr;
    Q_DECLARE_PRIVATE(ContextManager);
};
