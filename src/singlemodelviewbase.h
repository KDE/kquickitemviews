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
#pragma once

#include <viewbase.h>

class SingleModelViewBasePrivate;

/**
 * Random code to get close to drop-in compatibility with both QML and QtWidgets
 * views.
 *
 * This library
 * tries to enforce smaller components with well defined scope. But that makes
 * its API larger and more complex. This class helps to collapse all the
 * concepts into a single thing. This removes some flexibility, but allows
 * the widgets to be (almost) drop-in replacements for the ones provided
 * by QtQuick2.
 */
class SingleModelViewBase : public ViewBase
{
    Q_OBJECT
public:
    Q_PROPERTY(Qt::Corner gravity READ gravity WRITE setGravity)
    Q_PROPERTY(QQmlComponent* highlight READ highlight WRITE setHighlight)
    Q_PROPERTY(QSharedPointer<QItemSelectionModel> selectionModel READ selectionModel WRITE setSelectionModel NOTIFY selectionModelChanged)
    Q_PROPERTY(bool sortingEnabled READ isSortingEnabled WRITE setSortingEnabled)
    Q_PROPERTY(QModelIndex currentIndex READ currentIndex WRITE setCurrentIndex)
    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(bool empty READ isEmpty NOTIFY countChanged)

    /// Assume each hierarchy level have the same height (for performance)
    Q_PROPERTY(bool uniformRowHeight READ hasUniformRowHeight   WRITE setUniformRowHeight)
    /// Assume each column has the same width (for performance)
    Q_PROPERTY(bool uniformColumnWidth READ hasUniformColumnWidth WRITE setUniformColumnColumnWidth)

    explicit SingleModelViewBase(QQuickItem* parent = nullptr);
    virtual ~SingleModelViewBase();

    Qt::Corner gravity() const;
    void setGravity(Qt::Corner g);

    QQmlComponent* highlight() const;
    void setHighlight(QQmlComponent* h);

    QVariant model() const;
    void setModel(const QVariant& m);

    void setDelegate(QQmlComponent* delegate);
    QQmlComponent* delegate() const;

    QSharedPointer<QItemSelectionModel> selectionModel() const;
    void setSelectionModel(QSharedPointer<QItemSelectionModel> m);

    QModelIndex currentIndex() const;
    Q_INVOKABLE void setCurrentIndex(const QModelIndex& index,
        QItemSelectionModel::SelectionFlags f = QItemSelectionModel::ClearAndSelect
    );

    bool isSortingEnabled() const;
    void setSortingEnabled(bool val);

    bool hasUniformRowHeight() const;
    void setUniformRowHeight(bool value);

    bool hasUniformColumnWidth() const;
    void setUniformColumnColumnWidth(bool value);

    bool isEmpty() const;

protected Q_SLOTS:

    /**
     * Calling `setModel` doesn't do anything that takes effect immediately.
     *
     * Rather, it will either wait to the next event loop iteration to make
     * sure other properties have been applied. If no delegate is set, it will
     * also avoid loading the model internal representation because that would
     * be useless anyway.
     *
     * Override this method if extra steps are to be taken when replacing the
     * model. Do not forget to call the superclass method.
     */
    virtual void applyModelChanges(QAbstractItemModel* m);

protected:
    QAbstractItemModel *rawModel() const;

Q_SIGNALS:
    void currentIndexChanged(const QModelIndex& index);
    void selectionModelChanged() const;
    void modelChanged();
    void delegateChanged(QQmlComponent* delegate);
    virtual void countChanged() override final;

private:
    SingleModelViewBasePrivate* d_ptr;
};
