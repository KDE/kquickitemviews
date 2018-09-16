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
class AbstractViewItem;

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
class ContextManager final
{
    friend class VisualTreeItem; // Factory
public:
    /**
     * Add more properties to the QML context.
     *
     * This allows to add a predefined set of extra properties to the QML
     * context. It does *not* support adding more properties at runtime.
     *
     * The memory ownership is not transferred, do not re-use the instance
     * across many views (it will crash).
     */
    class PropertyGroup {
    public:

        virtual ~PropertyGroup() {}

        /**
         * Return a list of properties.
         *
         * This **MUST NEVER CHANGE** and needs to always return the same
         * vector. Implementations should use a `static` QVector.
         *
         * The property index is what's passed to getProperty, setProperty
         * and is the argument of changeProperty.
         */
        virtual QVector<QByteArray>& propertyNames() const;

        /**
         * The default implementation returns propertyNames().size()
         *
         * Implementing this rather than using `propertyNames` makes sense
         * when the properties contained in an existing data structure or
         * associated with extra metadata.
         */
        virtual uint size() const;

        /**
         * The default implementation returns propertyNames()[id];
         *
         * Implementing this rather than using `propertyNames` makes sense
         * when the properties contained in an existing data structure or
         * associated with extra metadata.
         */
        virtual QByteArray getPropertyName(uint id);

        /**
         * The id comes from propertyNames.
         *
         * It is recommended to use a switch statement or if/else_if for the
         * implementation and avoid QHash (or worst).
         */
        virtual QVariant getProperty(AbstractViewItem* item, uint id) const = 0;

        /**
         * Optionally make the property read/write.
         */
        virtual void setProperty(AbstractViewItem* item, uint id, const QVariant& value) const;

        /**
         * Notify that content of this property has changed.
         */
        void changeProperty(AbstractViewItem* item, uint id);
    };

    explicit ContextManager();
    ~ContextManager();

    /**
     * Add more properties to the context.
     *
     * This must be called from the view constructor. If called later, it will
     * Q_ASSERT.
     *
     * Implementations must subclass PropertyGroup to use this.
     */
    void addPropertyGroup(PropertyGroup* pg);

private:
    ContextManagerPrivate* d_ptr;
    Q_DECLARE_PRIVATE(ContextManager);
};
