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

#include <QQuickItem>

class QModelIndexBinderPrivate;

/**
 *
 * Bind a children component property with a QModelIndex role.
 *
 * It is intended to be used within an IndexView.
 *
 * @todo auto-create as attached properties in IndexView
 */
class Q_DECL_EXPORT QModelIndexBinder : public QQuickItem
{
    Q_OBJECT
public:

    /**
     * Only call setData after `delay` milliseconds.
     */
    Q_PROPERTY(int delay READ delay WRITE setDelay NOTIFY changed)

    /**
     * The name of the role to sychronize.
     */
    Q_PROPERTY(QString modelRole READ modelRole WRITE setModelRole NOTIFY changed)

    /**
     * The (child) object property to bind with the role.
     */
    Q_PROPERTY(QString objectProperty READ objectProperty WRITE setObjectProperty NOTIFY changed)

    /**
     * Automatically call setData when the data is modified.
     */
    Q_PROPERTY(bool autoSave READ autoSave WRITE setAutoSave NOTIFY changed)

    /**
     * If the model data match the local one.
     */
    Q_PROPERTY(bool synchronized READ isSynchronized NOTIFY changed)

    // The content of the container
    Q_PROPERTY(QObject* _object READ _object WRITE _setObject NOTIFY changed)
    Q_CLASSINFO("DefaultProperty", "_object")

    explicit QModelIndexBinder(QQuickItem *parent = nullptr);
    virtual ~QModelIndexBinder();

    QString modelRole() const;
    void setModelRole(const QString& role);

    QString objectProperty() const;
    void setObjectProperty(const QString& op);

    QObject *_object() const;
    void _setObject(QObject *o);

    int delay() const;
    void setDelay(int d);

    bool autoSave() const;
    void setAutoSave(bool v);

    bool isSynchronized() const;

    /**
     * Reset the local property to match the model data.
     */
    Q_INVOKABLE void reset() const;

    /**
     * Call `setData` now.
     */
    Q_INVOKABLE bool applyNow() const;

Q_SIGNALS:
    void changed();

private:
    QModelIndexBinderPrivate *d_ptr;
    Q_DECLARE_PRIVATE(QModelIndexBinder)
};
