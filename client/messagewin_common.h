/**********************************************************************
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifndef FC__MESSAGEWIN_COMMON_H
#define FC__MESSAGEWIN_COMMON_H

/* utility */
#include "support.h"            /* bool type */

/* common */
#include "events.h"		/* enum event_type */
#include "fc_types.h"		/* struct tile */
#include "featured_text.h"      /* struct text_tag_list */

struct message {
  char *descr;
  struct text_tag_list *tags;
  struct tile *tile;
  enum event_type event;
  bool location_ok, city_ok, visited;
};

void meswin_clear(void);
void meswin_add(const char *message, const struct text_tag_list *tags,
                       struct tile *ptile, enum event_type event);

const struct message *meswin_get_message(int message_index);
int meswin_get_num_messages(void);
void meswin_set_visited_state(int message_index, bool state);
void meswin_popup_city(int message_index);
void meswin_goto(int message_index);
void meswin_double_click(int message_index);

#endif /* FC__MESSAGEWIN_COMMON_H */
