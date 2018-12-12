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
#ifndef KQUICKITEMVIEWS_DELEGATECHOOSER_H
#define KQUICKITEMVIEWS_DELEGATECHOOSER_H

// Qt
#include <QQmlListProperty>
#include <QtCore/QObject>

// KQuickItemViews
#include <delegatechoice.h>

class DelegateChooserPrivate;

class Q_DECL_EXPORT DelegateChooser : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("DefaultProperty", "choices")
public:

    enum class Modes {
        Undefined,
        Columns,
        Rows,
        Indices,
        Expression,
    };
    Q_ENUM(Modes)

    Q_PROPERTY(Modes mode READ mode NOTIFY changed)

    Q_PROPERTY(QQmlListProperty<DelegateChoice> choices READ choices NOTIFY changed)
    Q_PROPERTY(QString role READ role WRITE setRole NOTIFY changed)

    QString role() const;
    void setRole(const QString &role);

    QQmlListProperty<DelegateChoice> choices();

    Modes mode() const;
    void setMode(Modes m);

protected:
    virtual bool evaluateIndex(const QModelIndex& idx);
    virtual bool evaluateRow(int row, const QModelIndex& parent);
    virtual bool evaluateColumn(int row, const QModelIndex& parent);

Q_SIGNALS:
    void changed();

private:
    DelegateChooserPrivate *d_ptr;
    Q_DECLARE_PRIVATE(DelegateChooser)
};

#endif
