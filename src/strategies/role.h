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
#ifndef KQUICKITEMVIEWS_ROLE_H
#define KQUICKITEMVIEWS_ROLE_H

#include <adapters/geometryadapter.h>

class Viewport;
class RoleStrategiesPrivate;

namespace GeometryStrategies
{

/**
 * A GeometryAdapter to use size hints provided by the model as a role.
 */
class Q_DECL_EXPORT Role : public GeometryAdapter
{
    Q_OBJECT
public:
    explicit Role(Viewport *parent = nullptr);
    virtual ~Role();

    Q_PROPERTY(int role READ role WRITE setRole NOTIFY roleChanged)
    Q_PROPERTY(QString roleName READ roleName WRITE setRoleName NOTIFY roleChanged)

    int role() const;
    void setRole(int role);

    QString roleName() const;
    void setRoleName(const QString& roleName);

    Q_INVOKABLE virtual QSizeF sizeHint(const QModelIndex &index, AbstractItemAdapter *adapter) const override;

Q_SIGNALS:
    void roleChanged();

private:
    RoleStrategiesPrivate *d_ptr;
};

}

#endif
