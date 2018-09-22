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

// Qt
#include <QtCore/QModelIndex>
class QQmlContext;

// KQuickItemViews
class AbstractItemAdapter;
class ContextAdapterFactory;
class DynamicContext;

/**
 * Calling `context()` on an instance of this object will create a
 * QQmlContext reflecting the model roles (and other property groups).
 *
 * One instance needs exists per context. It is possible to replace the
 * QModelIndex later on as needs suit. The two main use cases are to
 * either track multiple QModelIndex at once and display them or use a
 * single instance along with some QQmlExpression to add "just-in-time"
 * properties to objects (like QItemDelegate or
 * QSortFilterProxyModel::acceptRow)
 *
 * These objects MUST BE CREATED AFTER the last call to addContextExtension
 * has been made and the model(index) has been set.
 */
class ContextAdapter
{
    friend class AbstractItemAdapter;
    friend class ContextAdapterFactory; //factory
public:
    virtual ~ContextAdapter();

    bool isCacheEnabled() const;
    void setCacheEnabled(bool v);

    virtual QModelIndex index() const;
    void setModelIndex(const QModelIndex& index);

    virtual QQmlContext* context() const;
    virtual AbstractItemAdapter* item() const;

protected:
    QObject *contextObject() const;
    explicit ContextAdapter(QQmlContext *parentContext = nullptr);

private:
    DynamicContext* d_ptr;
};
