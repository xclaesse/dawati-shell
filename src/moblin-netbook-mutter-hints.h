
#ifndef MOBLIN_MUTTER_HINTS_H
#define MOBLIN_MUTTER_HINTS_H

#include "moblin-netbook.h"

typedef enum
{
  MNB_STATE_UNSET = 0,
  MNB_STATE_YES,
  MNB_STATE_NO
} MnbThreeState;

MnbThreeState moblin_netbook_mutter_hints_on_new_workspace (MetaWindow *window);

#endif
