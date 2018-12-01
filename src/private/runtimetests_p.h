#pragma once

void _test_validateTree(StateTracker::Content *self, StateTracker::Index* p);
void _test_validateLinkedList(StateTracker::Content *self, bool skipVItemState = false);
void _test_validateViewport(StateTracker::Content *self, bool skipVItemState = false);
void _test_validate_edges(StateTracker::Content *self);
void _test_validate_move(
    StateTracker::Content *self,
    StateTracker::Index* parentTTI,
    StateTracker::Index* startTTI,
    StateTracker::Index* endTTI,
    StateTracker::Index* newPrevTTI,
    StateTracker::Index* newNextTTI,
    int row);
void _test_validate_edges_simple(StateTracker::Content *self);
void _test_validate_geometry_cache(StateTracker::Content *self);
void _test_print_state(StateTracker::Content *self);
void _test_validateUnloaded(StateTracker::Content *self, const QModelIndex& parent, int first, int last);
void _test_validateContinuity(StateTracker::Content *self);
void _test_validateAtEnd(StateTracker::Content *self);
void _test_validateModelAboutToReplace(StateTracker::Content *self);
