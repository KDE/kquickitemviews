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
#ifndef KQUICKITEMVIEWS_SIZEHINTPROXYMODEL_H
#define KQUICKITEMVIEWS_SIZEHINTPROXYMODEL_H

// Qt
#include <QtCore/QIdentityProxyModel>
#include <QtCore/QVariant>
#include <QJSValue>
#include <QQmlScriptString>

class SizeHintProxyModelPrivate;

/**
 * This proxy model allows to predict the final size of a view delegate.
 *
 * This model needs to be implemented when both of the following things are
 * used:
 *
 * * A scrollbar or anything that depends on `contentHeight` being correct
 * * A non uniform delegate size
 *
 * Back in the QtWidgets days, the QItemDelegate/QStyledItemDelegate had the
 * `sizeHint` method. It was easy to use because it was the same class that
 * performed the painting so it had all the necessary knowledge to return
 * the correct value. In QML, however, it's not the case. Predicting the size
 * may or may not require knowledge of the style that are not officially in
 * public APIs. That being said, it should usually be possible to gather the
 * data in one way or another.
 *
 * This class is part of the public C++ API so the `sizeHint` method can be
 * re-implemented. Otherwise it is specified in JavaScript.
 */
class Q_DECL_EXPORT SizeHintProxyModel : public QIdentityProxyModel
{
    Q_OBJECT
public:
    /**
     * Return the width for a QModelIndex.
     *
     * All roles and all constants constants are exposed as variables in the
     * expression.
     */
    Q_PROPERTY(QQmlScriptString widthHint READ widthHint WRITE setWidthHint)

    /**
     * Return the width for a QModelIndex.
     *
     * All roles and all constants constants are exposed as variables in the
     * expression.
     */
    Q_PROPERTY(QQmlScriptString heightHint READ heightHint WRITE setHeightHint)

    /**
     * Add variables to the sizeHint callback context that likely wont change
     * over time. This is useful to store font metrics and static sizes.
     *
     * The function is called when the model changes or invalidateContext is
     * called.
     */
    Q_PROPERTY(QJSValue constants READ constants WRITE setConstants)

    /**
     * When `dataChanged` on the model is called with a list of invalidated roles
     * matching the entries in this list, assume the size hint needs to be
     * recomputed.
     *
     * When used with the other KQuickItemViews views, they will be notified.
     *
     * Note that the constants wont be invalidated.
     */
    Q_PROPERTY(QStringList invalidationRoles READ invalidationRoles WRITE setInvalidationRoles)

    explicit SizeHintProxyModel(QObject* parent = nullptr);
    virtual ~SizeHintProxyModel();

    virtual void setSourceModel(QAbstractItemModel *newSourceModel) override;

    QQmlScriptString widthHint() const;
    void setWidthHint(const QQmlScriptString& value);

    QQmlScriptString heightHint() const;
    void setHeightHint(const QQmlScriptString& value);

    QJSValue constants() const;
    void setConstants(const QJSValue& value);

    QStringList invalidationRoles() const;
    void setInvalidationRoles(const QStringList& l);

    Q_INVOKABLE QSizeF sizeHintForIndex(const QModelIndex& idx);

    Q_INVOKABLE QVariant getRoleValue(const QModelIndex& idx, const QString& roleName) const;

public Q_SLOTS:
    /**
     * Call this when the values in the constants may have changed.
     */
    void invalidateConstants();

private:
    SizeHintProxyModelPrivate* d_ptr;
    Q_DECLARE_PRIVATE(SizeHintProxyModel)
};

Q_DECLARE_METATYPE(SizeHintProxyModel*)

#endif
