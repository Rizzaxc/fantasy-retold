#ifndef GUARD_SELECT_MENU_H
#define GUARD_SELECT_MENU_H

bool32 SelectMenu_IsBlocked(void);
bool32 SelectMenu_TakeQueuedRegisteredItem(void);
void SelectMenu_BufferLabels(void);
void SelectMenu_ToggleRepel(void);
void SelectMenu_ToggleExpShare(void);
void SelectMenu_QueueRegisteredItem(void);

#endif // GUARD_SELECT_MENU_H
