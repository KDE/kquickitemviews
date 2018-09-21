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

#include <simpleflickable.h>

// Qt
#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>
class QQmlComponent;

class AbstractQuickViewPrivate;
class AbstractQuickViewSync;
class AbstractQuickView;
class AbstractSelectableView;
class AbstractViewItem;
class ContextManager;
class ModelAdapter;
class VisibleRange;

/**
 * Second generation of QtQuick treeview.
 *
 * The first one was designed for the chat view. It had a limited number of
 * requirement when it came to QtModel. However it required total control of
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
class AbstractQuickView : public SimpleFlickable
{
    Q_OBJECT
    friend struct TreeTraversalItems;
    friend class VisualTreeItem;
    friend class AbstractQuickViewSync; // internal API
    friend class ModelAdapter; // call createItem
public:
    explicit AbstractQuickView(QQuickItem* parent = nullptr);

    virtual ~AbstractQuickView();

    QVector<ModelAdapter*> modelAdapters() const;

    /**
     * Get the AbstractViewItem associated with a model index.
     *
     * Note that if the index is not currently visible or buferred, it will
     * return nullptr.
     */
    AbstractViewItem* itemForIndex(const QModelIndex& idx) const; //FIXME remove

protected:
    virtual void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;


    /// To be used with moderation. Necessary when the delegate is replaced.
    void reload();

    /**
     * Extend this function when tasks need to be taken care of when an
     * exlicit refresh is needed. Remember to call the parent class
     * ::refresh() from the extended one.
     *
     * It is called, for example, when the model change.
     */
    virtual void refresh() final;

    void addModelAdapter(ModelAdapter* a);
    void removeModelAdapter(ModelAdapter* a);

    /**
     * To be implemented by the final class.
     */
    virtual AbstractViewItem* createItem(VisibleRange* r) const = 0;

Q_SIGNALS:
    virtual void contentChanged() = 0;
    virtual void countChanged() = 0;

public:
    // Private API
    AbstractQuickViewSync* s_ptr;
private:
    AbstractQuickViewPrivate* d_ptr;
    Q_DECLARE_PRIVATE(AbstractQuickView)
};

Q_DECLARE_METATYPE(QSharedPointer<QItemSelectionModel>)
