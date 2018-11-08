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

struct TreeTraversalItem;
class TreeTraversalReflector;
class VisualTreeItem;
class ViewportPrivate;
class Viewport;
class ContextAdapter;
class ViewItemContextAdapter;

#include <QtCore/QRectF>
#include <QtCore/QModelIndex>

#include "geometrycache_p.h"

/**
 * For size modes like UniformRowHeight, it's pointless to keep track of
 * every single elements (potentially without a TreeTraversalItem).
 *
 * Even in the case of individual item, it may not be worth the extra memory
 * and recomputing them on demand may make sense.
 */
struct BlockMetadata
{
    explicit BlockMetadata(ViewportPrivate* p) : m_pViewport(p) {}
    ~BlockMetadata();
    int removeMe = -1;

    enum class Mode {
        SINGLE,
        BLOCK
    };

    enum class Source {
        NONE,
        HINTS,
        WIDGETS,
    };

    enum class Action {
        SHOW   = 0, /*!< Make visible on screen (or buffer) */
        HIDE   = 1, /*!< Remove from the screen (or buffer) */
        ATTACH = 2, /*!< Track, but do not show             */
        DETACH = 3, /*!< Stop tracking for changes          */
        UPDATE = 4, /*!< Update the element                 */
        MOVE   = 5, /*!< Update the depth and lookup        */
        RESET  = 6, /*!< Flush the visual item              */
    };

    QRectF geometry() const;
    QModelIndex index() const;


    Qt::Edge m_BufferEdge {Qt::TopEdge};
    Qt::Edges m_IsEdge {};

    bool performAction(Action);

    Source  m_Source  {Source::NONE};
    Mode    m_Mode    {Mode::SINGLE};
    TreeTraversalItem *m_pTTI  {nullptr};

    BlockMetadata *up   () const;
    BlockMetadata *down () const;
    BlockMetadata *left () const;
    BlockMetadata *right() const;

    bool isTopItem() const;

    // Check of the QQuickItem exists and is in the right spot
    bool isInSync() const;

    void setVisualItem(VisualTreeItem *i);

    VisualTreeItem *visualItem() const;

    ContextAdapter* contextAdapter() const;

    GeometryCache m_State; //FIXME I could not find a stable enough other way

private:
    ViewportPrivate *m_pViewport;
    VisualTreeItem  *m_pItem {nullptr};
    mutable ViewItemContextAdapter* m_pContextAdapter {nullptr};
};

/**
 * In order to keep the separation of concerns design goal intact, this
 * interface between the TreeTraversalReflector and Viewport internal
 * metadata without exposing them.
 */
class ViewportSync final
{
public:

    /**
     * From the model or feedback loop
     */
    void updateGeometry(BlockMetadata* item);

    /**
     * From the widget
     */
    void geometryUpdated(BlockMetadata* item);

    /**
     * From the model or feedback loop
     */
    void notifyRemoval(BlockMetadata* item);

    /**
     * From the model or feedback loop
     */
    void notifyInsert(BlockMetadata* item);

    /**
     * From the model when the QModelIndex role change
     */
    void notifyChange(BlockMetadata* item);

    /**
     * Manually trigger the sizes and positions to be updated.
     */
    void refreshVisible();

    inline void updateSingleItem(const QModelIndex& index, BlockMetadata* b);

    Viewport *q_ptr;
};
