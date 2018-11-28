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

#include <singlemodelviewbase.h>

// Qt
class QQuickItem;

// KQuickItemViews
class HierarchyViewPrivate;

/**
 * Second generation of QtQuick treeview.
 *
 * The first one was designed for the chat view. It had a limited number of
 * requirements when it came to QtModel. However it required total control of
 * the layout.
 *
 * This is the opposite use case. The layout is classic, but the model support
 * has to be complete. Performance and lazy loading is also more important.
 *
 * It require less work to write a new treeview than refector the first one to
 * support the additional requirements. In the long run, the first generation
 * could be folded into this widget (if it ever makes sense, otherwise they will
 * keep diverging).
 */
class Q_DECL_EXPORT HierarchyView : public SingleModelViewBase
{
    Q_OBJECT

    friend class HierarchyViewItem;
public:
    explicit HierarchyView(QQuickItem* parent = nullptr);
    virtual ~HierarchyView();

private:
    HierarchyViewPrivate* d_ptr;
    Q_DECLARE_PRIVATE(HierarchyView)
};
