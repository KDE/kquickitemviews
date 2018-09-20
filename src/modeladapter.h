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

class QQmlComponent;
class QAbstractItemModel;

class ModelAdapterPrivate;

/**
 * Wrapper object to assign a model to a view.
 *
 * It supports both QSharedPointer based models and "raw" QAbstractItemModel.
 *
 * This class also allow multiple properties to be attached to the model, such
 * as a GUI delegate.
 */
class ModelAdapter : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(bool empty READ isEmpty NOTIFY countChanged)

    explicit ModelAdapter(QObject* parent = nullptr);
    virtual ~ModelAdapter();

    QVariant model() const;
    void setModel(const QVariant& var);

    virtual void setDelegate(QQmlComponent* delegate);
    QQmlComponent* delegate() const;

    bool isEmpty() const;

Q_SIGNALS:
    void modelAboutToChange(QAbstractItemModel* m);
    void modelChanged(QAbstractItemModel* m);
    void countChanged();
    void delegateChanged(QQmlComponent* delegate);

private:
    ModelAdapterPrivate *d_ptr;
};
