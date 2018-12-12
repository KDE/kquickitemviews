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
#ifndef KQUICKITEMVIEWS_CONTEXTADAPTERFACTORY_H
#define KQUICKITEMVIEWS_CONTEXTADAPTERFACTORY_H

// Qt
#include <QtCore/QObject>
class QAbstractItemModel;
class QQmlContext;
class QQuickItem;

// LibStdC++
#include <functional>

// KQuickItemViews
class ContextAdapterFactoryPrivate;
class ContextExtension;
class ContextAdapter;

/**
 * This class manage the content of the AbstractItemAdapter::context().
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
class Q_DECL_EXPORT ContextAdapterFactory
{
    friend class ContextAdapter; // Public API
    friend class DynamicContext; // call finish()
public:
    /**
     * This constructor has an optional template parameter to specify a
     * different class to be built by the factory. By default it builds a
     * plain ContextAdapter
     */
    template<typename T = ContextAdapter> explicit ContextAdapterFactory() :
        ContextAdapterFactory([](QQmlContext* c)->ContextAdapter*{return new T(c);}){}

    virtual ~ContextAdapterFactory();

    QAbstractItemModel *model() const;
    void setModel(QAbstractItemModel *m);

    /**
     * Add more properties to the context.
     *
     * This must be called from the view constructor. If called later, it will
     * Q_ASSERT.
     *
     * Implementations must subclass ContextExtension to use this.
     */
    void addContextExtension(ContextExtension* pg);

    QSet<QByteArray> usedRoles() const;

    /**
     * Create a context adapter.
     *
     * This can only be done after the last context extension is added.
     */
    ContextAdapter* createAdapter(QQmlContext *parentContext = nullptr) const;

    /**
     * Alternate version of createAdapter with the ability to create derivative
     * of the ContextAdapter.
     */
    template<typename T>
    T* createAdapter(QQmlContext *p = nullptr) const {
        return static_cast<T*>(createAdapter([](QQmlContext* c)->ContextAdapter*{return new T(c);}, p));
    }

private:
    using FactoryFunctor = std::function<ContextAdapter*(QQmlContext*)>;
    ContextAdapterFactory(FactoryFunctor f);
    ContextAdapter* createAdapter(FactoryFunctor, QQmlContext*) const;

    ContextAdapterFactoryPrivate* d_ptr;
    Q_DECLARE_PRIVATE(ContextAdapterFactory);
};

#endif
