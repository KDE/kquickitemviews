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
#include "plugin.h"

#include <QtCore/QDebug>

#include "pixmapwrapper.h"
#include "modelscrolladapter.h"
#include "hierarchyview.h"
#include "quicklistview.h"
#include "quicktreeview.h"
#include "bindedcombobox.h"
#include "flickablescrollbar.h"

void KQuickView::registerTypes(const char *uri)
{
    Q_ASSERT(uri == QLatin1String("org.kde.playground.kquickview"));

    qmlRegisterType<HierarchyView>(uri, 1, 0, "HierarchyView");
    qmlRegisterType<QuickTreeView>(uri, 1, 0, "QuickTreeView");
    qmlRegisterType<QuickListView>(uri, 1, 0, "QuickListView");
    qmlRegisterType<ModelScrollAdapter>(uri, 1, 0, "ModelScrollAdapter");
    qmlRegisterType<PixmapWrapper>(uri, 1,0, "PixmapWrapper");
    qmlRegisterType<BindedComboBox>(uri, 1, 0, "BindedComboBox");
    qmlRegisterType<FlickableScrollBar>(uri, 1, 0, "FlickableScrollBar");
    qmlRegisterUncreatableType<QuickListViewSections>(uri, 1, 0, "QuickListViewSections", "");
}

void KQuickView::initializeEngine(QQmlEngine *engine, const char *uri)
{
    qDebug() << "\n\n\nCALLED!";
}
