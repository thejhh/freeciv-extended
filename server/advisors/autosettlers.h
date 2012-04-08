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
#ifndef FC__AUTOSETTLERS_H
#define FC__AUTOSETTLERS_H

#include "fc_types.h"
#include "map.h"

struct settlermap;
struct pf_path;

void auto_settlers_init(void);
void auto_settlers_player(struct player *pplayer);

void auto_settler_findwork(struct player *pplayer, 
                           struct unit *punit,
                           struct settlermap *state,
                           int recursion);

void auto_settler_setup_work(struct player *pplayer, struct unit *punit,
                             struct settlermap *state, int recursion,
                             struct pf_path *path,
                             struct tile *best_tile,
                             enum unit_activity best_act,
                             int completion_time);

int settler_evaluate_improvements(struct unit *punit,
                                  enum unit_activity *best_act,
                                  struct tile **best_tile,
                                  struct pf_path **path,
                                  struct settlermap *state);

void ai_manage_settler(struct player *pplayer, struct unit *punit);

void init_settlers(void);

void initialize_infrastructure_cache(struct player *pplayer);

extern signed int *minimap;
#define MINIMAP(_tile) minimap[tile_index(_tile)]

#endif   /* FC__AUTOSETTLERS_H */
