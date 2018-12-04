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
class QQmlComponent;
class QQmlEngine;

// KItemViews
class Viewport;
class IndexMetadata;
class AbstractItemAdapter;
class GeoStrategySelector;
class ViewBaseItemVariables;

namespace StateTracker {
class Content;
}

#include <QtCore/QRectF>
#include <QtCore/QModelIndex>

#include "statetracker/geometry_p.h"

/**
 * In order to keep the separation of concerns design goal intact, this
 * interface between the StateTracker::Content and Viewport internal
 * metadata without exposing them.
 */
class ViewportSync final
{
public:

    /**
     * From the model or feedback loop
     */
    void updateGeometry(IndexMetadata* item);

    /**
     * From the widget
     */
    void geometryUpdated(IndexMetadata* item);

    /**
     * From the model or feedback loop
     */
    void notifyRemoval(IndexMetadata* item);

    /**
     * From the model or feedback loop
     */
    void notifyInsert(IndexMetadata* item);

    /**
     * From the model when the QModelIndex role change
     */
    void notifyChange(IndexMetadata* item);

    /**
     * Manually trigger the sizes and positions to be updated.
     */
    void refreshVisible();

    QQmlEngine    *engine();
    QQmlComponent *component();

    IndexMetadata *metadataForIndex(const QModelIndex& idx) const;

    Viewport *q_ptr;
    StateTracker::Content *m_pReflector {nullptr};
    GeoStrategySelector *m_pGeoAdapter  { nullptr };
    std::function<AbstractItemAdapter*()> m_fFactory;

private:
    QQmlEngine    *m_pEngine    {nullptr};
    QQmlComponent *m_pComponent {nullptr};
};
