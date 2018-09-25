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

struct TreeTraversalItems;
class TreeTraversalReflector;
class VisualTreeItem;

/**
 * For size modes like UniformRowHeight, it's pointless to keep track of
 * every single elements (potentially without a TreeTraversalItem).
 *
 * Even in the case of individual item, it may not be worth the extra memory
 * and recomputing them on demand may make sense.
 */
struct BlockMetadata
{
    enum class Mode {
        SINGLE,
        BLOCK
    };

    Mode    m_Mode    {Mode::SINGLE};
    QPointF m_Position;
    QSizeF  m_Size;
    TreeTraversalItems* m_pTTI {nullptr};
};

/**
 * In order to keep the separation of concerns design goal intact, this
 * interface between the TreeTraversalReflector and Viewport internal
 * metadata without exposing them.
 */
class ViewportSync final
{
public:
    void geometryUpdated(VisualTreeItem* item);

    inline void updateSingleItem(const QModelIndex& index, BlockMetadata* b);

    Viewport *q_ptr;
};
