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
#ifndef KQUICKITEMVIEWS_CONTENT_P_H
#define KQUICKITEMVIEWS_CONTENT_P_H

// Qt
#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>

// KQuickViews
class ContentPrivate;
class Viewport;
class ModelRect;
#include <private/indexmetadata_p.h>
#include "modelitem_p.h"

namespace StateTracker {

struct ModelItem;
struct Model;

/**
 * Listen to the model changes and forward those changes to the other relevant
 * state trackers.
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
    ModelRect* edges(IndexMetadata::EdgeType e) const;

    // Getter
    StateTracker::Index *root        () const;
    StateTracker::Index *lastItem    () const;
    StateTracker::Index *firstItem   () const;
    StateTracker::Model *modelTracker() const;

    // Mutator
    void connectModel(QAbstractItemModel *m);
    void disconnectModel(QAbstractItemModel *m);
    void resetRoot();
    void resetEdges();
    void perfromStateChange(Event e, IndexMetadata *md, StateTracker::ModelItem::State s);
    void forceInsert(const QModelIndex& idx);
    void forceInsert(const QModelIndex& parent, int first, int last);

    // Helpers
    IndexMetadata *metadataForIndex(const QModelIndex& idx) const;
    bool isActive(const QModelIndex& parent, int first, int last);
    Index *find(Index *i, Qt::Edge direction, std::function<bool(Index *i)>) const;

Q_SIGNALS:
    void contentChanged();

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

#endif
