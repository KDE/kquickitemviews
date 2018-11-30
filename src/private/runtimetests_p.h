#pragma once

void _test_validateTree(TreeTraversalReflector *self, StateTracker::Index* p);
void _test_validateLinkedList(TreeTraversalReflector *self, bool skipVItemState = false);
void _test_validateViewport(TreeTraversalReflector *self, bool skipVItemState = false);
void _test_validate_edges(TreeTraversalReflector *self);
void _test_validate_move(
    TreeTraversalReflector *self,
    StateTracker::Index* parentTTI,
    StateTracker::Index* startTTI,
    StateTracker::Index* endTTI,
    StateTracker::Index* newPrevTTI,
    StateTracker::Index* newNextTTI,
    int row);
void _test_validate_edges_simple(TreeTraversalReflector *self);
void _test_validate_geometry_cache(TreeTraversalReflector *self);
void _test_print_state(TreeTraversalReflector *self);
void _test_validateUnloaded(TreeTraversalReflector *self, const QModelIndex& parent, int first, int last);
void _test_validateContinuity(TreeTraversalReflector *self);
void _test_validateAtEnd(TreeTraversalReflector *self);
void _test_validateModelAboutToReplace(TreeTraversalReflector *self);
