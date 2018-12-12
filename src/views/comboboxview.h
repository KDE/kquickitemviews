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
#ifndef COMBOBOXVIEW_H
#define COMBOBOXVIEW_H

// Qt
#include <QQuickItem>
#include <QtCore/QItemSelectionModel>

class ComboBoxViewPrivate;

/**
 * Extended QtQuickControls2 ComboBox with proper selection model support.
 */
class Q_DECL_EXPORT ComboBoxView : public QQuickItem
{
    Q_OBJECT
public:
    Q_PROPERTY(QItemSelectionModel* selectionModel READ selectionModel WRITE setSelectionModel)

    explicit ComboBoxView(QQuickItem* parent = nullptr);
    virtual ~ComboBoxView();

    QItemSelectionModel* selectionModel() const;
    void setSelectionModel(QItemSelectionModel* s);

private:
    ComboBoxViewPrivate* d_ptr;
    Q_DECLARE_PRIVATE(ComboBoxView)
};
// Q_DECLARE_METATYPE(ComboBoxView*)

#endif
