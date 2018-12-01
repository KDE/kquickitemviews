/***************************************************************************
 *   Copyright (C) 2017-2018 by Emmanuel Lepage Vallee                     *
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
#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>

// KQuickViews

class ContentPrivate;
class AbstractItemAdapter;
class Viewport;
class ModelRect;
#include <private/indexmetadata_p.h>
#include "modelitem_p.h"

namespace StateTracker {

struct ModelItem;
struct Model;

/**
 * This class refects a QAbstractItemModel (realtime) topology.
 *
 * It helps to handle the various model events to always keep a correct
 * overview of the model content. While the models are trees, this class expose
 * a 2D linked list API. This is "better" for the views because in the end they
 * place widgets in a 2D plane (grid) so geometric navigation makes placing
 * the widget easier.
 *
 * The format this class exposes is not as optimal as it could. However it was
 * chosen because it made the consumer code more readable and removed most of
 * the corner cases that causes QtQuick.ListView to fail to be extended to
 * tables and trees (without exponentially more complexity). It also allows a
 * nice encapsulation and separation of concern which removes the need for
 * extensive and non-reusable private APIs.
 *
 * Note that you should only use this class directly when implementing low level
 * views such as charts or graphs. The `ViewBase` is a better base
 * for most traditional views.
 *
 * The `setAvailableEdges` must be kept up to date by the owner and `populate`
 * or `detachUntil` called to trigger manual changes. Otherwise it will listen
 * to the model and modify itself and the model changes.
 */
class Content final : public QObject
{
    Q_OBJECT
public:
    enum class Event {
        ENTER_STATE,
        LEAVE_STATE,
    };

    explicit Content(Viewport* parent = nullptr);
    virtual ~Content();

    // Edges management
    void setAvailableEdges(Qt::Edges edges, IndexMetadata::EdgeType type);
    Qt::Edges availableEdges(IndexMetadata::EdgeType type) const;
    IndexMetadata *getEdge(IndexMetadata::EdgeType t, Qt::Edge e) const;
    void setEdge(IndexMetadata::EdgeType et, StateTracker::Index* tti, Qt::Edge e);

    // Getter
    IndexMetadata *metadataForIndex(const QModelIndex& idx) const;
    Viewport *viewport() const;
    StateTracker::Index *root() const;
    StateTracker::Index *lastItem() const;
    StateTracker::Index *firstItem() const;
    ModelRect* edges(IndexMetadata::EdgeType e) const;
    const std::function<AbstractItemAdapter*()>& itemFactory() const;
    StateTracker::Model *modelTracker() const;

    // Setters
    void setItemFactory(const std::function<AbstractItemAdapter*()>& factory);

    // Mutator
    void connectModel(QAbstractItemModel *m);
    void disconnectModel(QAbstractItemModel *m);
    void resetRoot();
    void resetEdges();
    void perfromStateChange(Event e, IndexMetadata *md, StateTracker::ModelItem::State s);
    void forceInsert(const QModelIndex& idx);
    void forceInsert(const QModelIndex& parent, int first, int last);

    // Helpers
    bool isActive(const QModelIndex& parent, int first, int last); //TODO move to range
    StateTracker::Index *find(
        StateTracker::Index *i,
        Qt::Edge direction,
        std::function<bool(StateTracker::Index *i)>
    ) const;

Q_SIGNALS:
    void contentChanged();
    void countChanged  ();

private:
    ContentPrivate* d_ptr;
};

}

// Inject some extra validation when executed in debug mode.
#ifdef ENABLE_EXTRA_VALIDATION
 #define _DO_TEST(f, ...) f(__VA_ARGS__);
 #include <private/runtimetests_p.h>
#else
 #define _DO_TEST(f, ...) /*NOP*/;
#endif
