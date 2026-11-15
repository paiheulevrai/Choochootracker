#ifndef CHOOCHOO_EDIT_HISTORY_H
#define CHOOCHOO_EDIT_HISTORY_H

#include "common.h"

enum class EditObject { song, chain, phrase, table, pattern, playhead };

class EditHistoryScope {
 public:
  explicit EditHistoryScope(const char* label);
  ~EditHistoryScope();
  EditHistoryScope(const EditHistoryScope&) = delete;
  EditHistoryScope& operator=(const EditHistoryScope&) = delete;
};

bool editHistoryCapture(EditObject object, int index = 0);
bool editHistoryCapture(Project* project, EditObject object, int index = 0);
bool editHistoryCaptureInstrument(int index);
bool editHistoryCaptureModel(int index);
bool editHistoryCloneInstrument(int source, int destination);
void editHistoryClear();
void editHistoryBarrier();
void editHistoryGesture(bool held);
bool editHistoryUndo();
bool editHistoryRedo();
bool editHistoryCanUndo();
bool editHistoryCanRedo();
size_t editHistoryBytes();
size_t editHistoryResourceBytes();
void editHistoryOpenMenu();
bool editHistoryInput(InputCode input, bool down);

#endif
