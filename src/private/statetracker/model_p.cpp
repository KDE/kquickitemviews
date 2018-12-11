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
#include "model_p.h"

#include <QtCore/QDebug>

#include <private/indexmetadata_p.h>
#include <private/statetracker/content_p.h>
#include <private/statetracker/index_p.h>

using EdgeType = IndexMetadata::EdgeType;

#include <QtGlobal>

#define S StateTracker::Model::State::
const StateTracker::Model::State StateTracker::Model::m_fStateMap[6][7] = {
/*                POPULATE     DISABLE       ENABLE       RESET         FREE         MOVE         TRIM   */
/*NO_MODEL */ { S NO_MODEL , S NO_MODEL , S NO_MODEL , S NO_MODEL , S NO_MODEL , S NO_MODEL , S NO_MODEL },
/*PAUSED   */ { S POPULATED, S PAUSED   , S TRACKING , S PAUSED   , S PAUSED   , S PAUSED   , S PAUSED   },
/*POPULATED*/ { S TRACKING , S PAUSED   , S TRACKING , S RESETING , S POPULATED, S POPULATED, S POPULATED},
/*TRACKING */ { S TRACKING , S POPULATED, S TRACKING , S RESETING , S TRACKING , S TRACKING , S TRACKING },
/*RESETING */ { S RESETING , S RESETING , S TRACKING , S RESETING , S PAUSED   , S RESETING , S RESETING },
/*MUTATING */ { S MUTATING , S MUTATING , S MUTATING , S MUTATING , S MUTATING , S MUTATING , S MUTATING },
};
#undef S

// This state machine is self healing, error can be called in release mode
// and it will only disable the view without further drama.
#define A &StateTracker::Model::
const StateTracker::Model::StateF StateTracker::Model::m_fStateMachine[6][7] = {
/*               POPULATE     DISABLE    ENABLE     RESET       FREE       MOVE   ,   TRIM */
/*NO_MODEL */ { A nothing , A nothing, A nothing, A nothing, A nothing, A nothing , A error },
/*PAUSED   */ { A populate, A nothing, A error  , A nothing, A free   , A nothing , A trim  },
/*POPULATED*/ { A error   , A nothing, A track  , A reset  , A free   , A fill    , A trim  },
/*TRACKING */ { A nothing , A untrack, A nothing, A reset  , A free   , A fill    , A trim  },
/*RESETING */ { A nothing , A error  , A track  , A error  , A free   , A error   , A error },
/*MUTATING */ { A nothing , A nothing, A nothing, A nothing, A nothing, A nothing , A nothing},
};

StateTracker::Model::Model(StateTracker::Content* d) : q_ptr(d)
{}

StateTracker::Model::State StateTracker::Model::performAction(Action a)
{
    const int s = (int)m_State;
    m_State = m_fStateMap[s][(int)a];

    (this->*StateTracker::Model::m_fStateMachine[s][(int)a])();

    return m_State;
}

StateTracker::Model::State StateTracker::Model::state() const
{
    return m_State;
}

void StateTracker::Model::track()
{
    Q_ASSERT(m_pModel && !m_pTrackedModel);

    q_ptr->connectModel(m_pModel);

    m_pTrackedModel = m_pModel;
}

void StateTracker::Model::untrack()
{
    Q_ASSERT(m_pTrackedModel);
    if (!m_pTrackedModel) //TODO fix the state machine not to get here
        return;

    q_ptr->disconnectModel(m_pTrackedModel);

    m_pTrackedModel = nullptr;
}

void StateTracker::Model::free()
{
    q_ptr->resetRoot();
}

void StateTracker::Model::reset()
{
    const bool wasTracked = m_pTrackedModel != nullptr;

    if (wasTracked)
        untrack(); //TODO THIS_COMMIT

    q_ptr->root()->metadata() << IndexMetadata::LoadAction::RESET;
    q_ptr->resetEdges();

    //HACK
    if (!m_pModel) {
        m_State = State::NO_MODEL;
        return;
    }

    // Make sure it never stay in RESETING mode outside of the method
    performAction(wasTracked ? Action::ENABLE : Action::FREE);
}

/**
 * Keep loading more and more items until the view is filled.
 *
 * @todo Eventually add an adapter to track what should and should not be loaded
 * instead of using edges.
 */
void StateTracker::Model::populate()
{
    Q_ASSERT(m_pModel);

    _DO_TEST(_test_validateContinuity, q_ptr);

    if (!q_ptr->edges(EdgeType::FREE)->m_Edges)
        return;

    //HACK stash the state, this needs more refactoring
    // Sometime, if the viewport size depends on the content size, inserting
    // will cause some viewport "Resize" events which will call `populate`.
    // This could potentially create a very confusing situation where in single
    // QModelIndex is being inserted recursively and a stack overflow.
    const auto s = m_State;
    m_State = State::MUTATING;

    if (q_ptr->root()->firstChild() && (q_ptr->edges(EdgeType::FREE)->m_Edges & (Qt::TopEdge|Qt::BottomEdge))) {
        while (q_ptr->edges(EdgeType::FREE)->m_Edges & Qt::TopEdge) {
            const auto was = q_ptr->edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge);
            //FIXME when everything fails to load, it would otherwise make an infinite loop
            if (!was)
                break;

            const auto u = was->metadata()->modelTracker()->load(Qt::TopEdge);

            Q_ASSERT(u || q_ptr->edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge)->effectiveRow() == 0);
            Q_ASSERT(u || !q_ptr->edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge)->effectiveParentIndex().isValid());

            if (!u)
                break;

            u->metadata() << IndexMetadata::LoadAction::SHOW;
        }

        while (q_ptr->edges(EdgeType::FREE)->m_Edges & Qt::BottomEdge) {
            const auto was = q_ptr->edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge);

            //FIXME when everything fails to load, it would otherwise make an infinite loop
            if (!was)
                break;

            const auto u = was->metadata()->modelTracker()->load(Qt::BottomEdge);

            if (!u)
                break;

            u->metadata() << IndexMetadata::LoadAction::SHOW;
        }
    }
    else if (auto rc = m_pModel->rowCount()) {

        //TODO support anchors (load from the bottom)
        q_ptr->forceInsert({}, 0, rc - 1);

    }

    //HACK Restore the stashed state
    m_State = s;

    if (q_ptr->edges(EdgeType::FREE)->m_Edges & Qt::BottomEdge) {
        _DO_TEST(_test_validateAtEnd, q_ptr);
    }
}

void StateTracker::Model::trim()
{
    //FIXME For now the delegates are only freed when the QModelIndex is removed.
    // This also prevent pooling from being used/finished.
    return;

//     QTimer::singleShot(0, [this]() {
//         if (m_TrackingState != StateTracker::Content::TrackingState::TRACKING)
//             return;
//
//         _test_validateViewport();
//
//         qDebug() << "TRIM" << edges(EdgeType::VISIBLE)->m_Edges;
//     //     if (!edges(EdgeType::FREE)->getEdge(Qt::TopEdge)) {
//     //         Q_ASSERT(!edges(EdgeType::FREE)->getEdge(Qt::BottomEdge));
//     //         return;
//     //     }
//
//         auto elem = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge)); //FIXME have a visual and reacheable rect
//
//         while (!(edges(EdgeType::FREE)->m_Edges & Qt::TopEdge)) {
//             Q_ASSERT(elem);
//             Q_ASSERT(edges(EdgeType::FREE)->getEdge(Qt::TopEdge));
//             Q_ASSERT(!m_pViewport->currentRect().intersects(elem->metadata()->geometry()));
//
//             elem->metadata() << IndexMetadata::LoadAction::HIDE);
//             elem = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge));
//         }
//
//         elem = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge)); //FIXME have a visual and reacheable rect
//         while (!(edges(EdgeType::FREE)->m_Edges & Qt::BottomEdge)) {
//             Q_ASSERT(elem);
//             Q_ASSERT(!m_pViewport->currentRect().intersects(elem->metadata()->geometry()));
//             elem->metadata() << IndexMetadata::LoadAction::HIDE);
//             elem = TTI(edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge));
//         }
//     });


    // Unload everything below the cache area to get rid of the insertion/moving
    // overhead. Not doing so would also require to handle "holes" in the tree
    // which is very complex and unnecessary.
    StateTracker::Index *be = q_ptr->edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge); //TODO use BUFFERED

    if (!be)
        return;

    StateTracker::Index *item = q_ptr->lastItem();

    if (be == item)
        return;

    do {
        item->metadata() << IndexMetadata::LoadAction::DETACH;
    } while((item = item->up()) != be);
}

void StateTracker::Model::fill()
{
    trim();
    populate();
}

void StateTracker::Model::nothing()
{}

void StateTracker::Model::error()
{
    Q_ASSERT(false);
    this << StateTracker::Model::Action::DISABLE
         << StateTracker::Model::Action::RESET
         << StateTracker::Model::Action::ENABLE;
}

QAbstractItemModel *StateTracker::Model::trackedModel() const
{
    Q_ASSERT(m_pTrackedModel);
    return m_pTrackedModel;
}

QAbstractItemModel *StateTracker::Model::modelCandidate() const
{
    return m_pModel;
}

void StateTracker::Model::setModel(QAbstractItemModel* m)
{
    if (m == m_pTrackedModel)
        return;

    this << StateTracker::Model::Action::DISABLE
         << StateTracker::Model::Action::RESET
         << StateTracker::Model::Action::FREE;

    //_test_validateModelAboutToReplace();

    m_pModel = m;

    if (m) {
        Q_ASSERT(m_State == State::NO_MODEL || m_State == State::PAUSED);
        m_State = State::PAUSED;
    }

    if (!m)
        return;
}

