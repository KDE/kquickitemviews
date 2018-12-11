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
#include <QQmlComponent>
#include <QQmlScriptString>
#include <QtCore/QVariant>

class DelegateChoicePrivate;

class Q_DECL_EXPORT DelegateChoice : public QObject
{
    Q_OBJECT
public:

    Q_PROPERTY(int column READ column WRITE setColumn NOTIFY changed)
    Q_PROPERTY(int row READ row WRITE setRow NOTIFY changed)
    Q_PROPERTY(int depth READ depth WRITE setDepth NOTIFY changed)

    /// Either an `int` or a `QModelIndex`
    Q_PROPERTY(QVariant index READ index WRITE setIndex NOTIFY changed)
    Q_PROPERTY(QVariant roleValue READ roleValue WRITE setRoleValue NOTIFY changed)

    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY changed)

    /**
     * A JavaScript expression with access to all role values.
     */
    Q_PROPERTY(QQmlScriptString when READ when WRITE setWhen NOTIFY changed)

    explicit DelegateChoice(QObject *parent = nullptr);
    virtual ~DelegateChoice();

    int column() const;
    void setColumn(int c);

    int row() const;
    void setRow(int r);

    int depth() const;
    void setDepth(int d);

    QVariant index() const;
    void setIndex(const QVariant& i);

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *d);

    QQmlScriptString when() const;
    void setWhen(const QQmlScriptString &w);

    QVariant roleValue() const;
    void setRoleValue(const QVariant &v);

protected:
    virtual bool evaluateIndex(const QModelIndex& idx);
    virtual bool evaluateRow(int row, const QModelIndex& parent);
    virtual bool evaluateColumn(int row, const QModelIndex& parent);

Q_SIGNALS:
    void changed();

private:
    DelegateChoicePrivate *d_ptr;
    Q_DECLARE_PRIVATE(DelegateChoice)
};
