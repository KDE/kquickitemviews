/***************************************************************************
 *   Copyright (C) 2019 by Emmanuel Lepage Vallee                          *
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
#ifndef SIZEHINTVIEW_H
#define SIZEHINTVIEW_H

#include <KQuickItemViews/singlemodelviewbase.h>

// KQuickItemViews
class SizeHintViewPrivate;

/**
 * This view uses the Qt::SizeHintRole for both the size and position of an
 * index delegate instance. It assumes the model (or proxy) has all the logic
 * to handle this.
 *
 * It can also be used as a base for more complex models with their own built-in
 * proxy. In some case it can be preferable over implementing a GeometryAdapter.
 * For example if the proxy already exists or has tightly coupled interaction
 * with more roles (or filters).
 */
class Q_DECL_EXPORT SizeHintView : public SingleModelViewBase //TODO use ViewBase
{
    Q_OBJECT
    friend class SizeHintViewItem;
public:
    explicit SizeHintView(QQuickItem* parent = nullptr);
    virtual ~SizeHintView();

private:
    SizeHintViewPrivate* d_ptr;
    Q_DECLARE_PRIVATE(SizeHintView)
};

#endif
