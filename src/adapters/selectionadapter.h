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
#ifndef KQUICKITEMVIEWS_SELECTIONADAPTER_H
#define KQUICKITEMVIEWS_SELECTIONADAPTER_H

// Qt
#include <QtCore/QObject>
class QItemSelectionModel;
class QQmlComponent;

// KQuickItemViews
#include <adapters/contextadapter.h>
class SelectionAdapterSyncInterface;
class SelectionAdapterPrivate;
class ContextExtension;
namespace StateTracker {
    class ViewItem;
}

/**
 * This class adds support for multi-selection using selection models.
 *
 * It must be attached to a single instance of a `ViewBase` object.
 */
class SelectionAdapter : public QObject
{
    Q_OBJECT
    friend class ModelAdapter; // Notify of all relevant events
    friend class ModelAdapterPrivate; // Notify of all relevant events
    friend class StateTracker::ViewItem; // Notify of all relevant events
    friend class SelectionAdapterSyncInterface; // Its own internals
public:
    explicit SelectionAdapter(QObject* parent = nullptr);
    virtual ~SelectionAdapter();

    QQmlComponent* highlight() const;
    void setHighlight(QQmlComponent* h);

    QSharedPointer<QItemSelectionModel> selectionModel() const;
    void setSelectionModel(QSharedPointer<QItemSelectionModel> sm);

    ContextExtension *contextExtension() const;

Q_SIGNALS:
    void currentIndexChanged(const QModelIndex& index);
    void selectionModelChanged() const;

private:
    SelectionAdapterSyncInterface* s_ptr;
    SelectionAdapterPrivate* d_ptr;
    Q_DECLARE_PRIVATE(SelectionAdapter);
};

#endif
