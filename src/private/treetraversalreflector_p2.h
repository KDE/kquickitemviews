#pragma once
//FIXME remove

namespace StateTracker {
class Index;
class Model;
}

class AbstractItemAdapter;
class ModelRect;
class TreeTraversalReflector;
#include <private/indexmetadata_p.h>
#include <private/statetracker/modelitem_p.h>

class TreeTraversalReflectorPrivate : public QObject
{
//     Q_OBJECT
public:
    explicit TreeTraversalReflectorPrivate();

    // Helpers
    StateTracker::ModelItem* addChildren(const QModelIndex& index);
    StateTracker::ModelItem* ttiForIndex(const QModelIndex& idx) const;

    bool isInsertActive(const QModelIndex& p, int first, int last) const;

    QList<StateTracker::Index*> setTemporaryIndices(const QModelIndex &parent, int start, int end,
                             const QModelIndex &destination, int row);
    void resetTemporaryIndices(const QList<StateTracker::Index*>&);

    void reloadEdges();
    void resetEdges();

    StateTracker::Index *lastItem() const;

    StateTracker::ModelItem* m_pRoot {new StateTracker::ModelItem(this)};

    //TODO add a circular buffer to GC the items
    // * relative index: when to trigger GC
    // * absolute array index: kept in the StateTracker::ModelItem so it can
    //   remove itself

    /// All elements with loaded children
    QHash<QPersistentModelIndex, StateTracker::ModelItem*> m_hMapper;
    QAbstractItemModel* m_pModel {nullptr};
    QAbstractItemModel* m_pTrackedModel {nullptr};
    std::function<AbstractItemAdapter*()> m_fFactory;
    Viewport *m_pViewport;

    ModelRect m_lRects[3];
    ModelRect* edges(IndexMetadata::EdgeType e) const;
    void setEdge(IndexMetadata::EdgeType et, StateTracker::Index* tti, Qt::Edge e);

    StateTracker::Model *m_pModelTracker;

    StateTracker::Index *find(
        StateTracker::Index *i,
        Qt::Edge direction,
        std::function<bool(StateTracker::Index *i)>
    ) const;

    QModelIndex getNextIndex(const QModelIndex& idx) const;

    TreeTraversalReflector* q_ptr;

    // Update the ModelRect
    void insertEdge(StateTracker::ModelItem*, StateTracker::ModelItem::State);
    void removeEdge(StateTracker::ModelItem*, StateTracker::ModelItem::State);
    void enterState(StateTracker::ModelItem*, StateTracker::ModelItem::State);
    void leaveState(StateTracker::ModelItem*, StateTracker::ModelItem::State);
    void error     (StateTracker::ModelItem*, StateTracker::ModelItem::State);
    void resetState(StateTracker::ModelItem*, StateTracker::ModelItem::State);

    typedef void(TreeTraversalReflectorPrivate::*StateFS)(StateTracker::ModelItem*, StateTracker::ModelItem::State);
    static const StateFS m_fStateLogging[8][2];

    // Tests
    void _test_validateTree(StateTracker::Index *p);
    void _test_validateViewport(bool skipVItemState = false);
    void _test_validateLinkedList(bool skipVItemState = false);
    void _test_validate_edges();
    void _test_validate_edges_simple();
    void _test_validate_geometry_cache();
    void _test_validate_move(StateTracker::Index* parentTTI,StateTracker::Index* startTTI,
                             StateTracker::Index* endTTI, StateTracker::Index* newPrevTTI,
                             StateTracker::Index* newNextTTI, int row);
    void _test_print_state();
    void _test_validateUnloaded(const QModelIndex& parent, int first, int last);
    void _test_validateContinuity();
    void _test_validateAtEnd();
    void _test_validateModelAboutToReplace();

public Q_SLOTS:
    void cleanup();
    void slotRowsInserted  (const QModelIndex& parent, int first, int last);
    void slotRowsRemoved   (const QModelIndex& parent, int first, int last);
    void slotLayoutChanged (                                              );
    void slotRowsMoved     (const QModelIndex &p, int start, int end,
                            const QModelIndex &dest, int row);
};
