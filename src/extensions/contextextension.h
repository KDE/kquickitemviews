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
#include <QtCore/QVariant>
#include <QtCore/QByteArray>
#include <QtCore/QModelIndex>

/**
 * Add more properties to the QML context.
 *
 * This allows to add a predefined set of extra properties to the QML
 * context. It does *not* support adding more properties at runtime.
 *
 * The memory ownership is not transferred, do not re-use the instance
 * across many views (it will crash).
 */
class ContextExtension
{
public:

    virtual ~ContextExtension() {}

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
    virtual QByteArray getPropertyName(uint id) const;

    /**
        * Some variants, like QModelIndex ones, cannot be cached and will
        * most likely point to invalid memory.
        *
        * If the group has such properties, implement this function. Note
        * that the return value cannot change.
        *
        * The default implementation always returns true.
        */
    virtual bool supportCaching(uint id) const;

    /**
        * The id comes from propertyNames.
        *
        * It is recommended to use a switch statement or if/else_if for the
        * implementation and avoid QHash (or worst).
        */
    virtual QVariant getProperty(AbstractItemAdapter* item, uint id, const QModelIndex& index) const = 0;

    /**
        * Optionally make the property read/write.
        */
    virtual void setProperty(AbstractItemAdapter* item, uint id, const QVariant& value) const;

    /**
        * Notify that content of this property has changed.
        */
    void changeProperty(AbstractItemAdapter* item, uint id);
};
