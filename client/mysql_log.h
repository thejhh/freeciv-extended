/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
           Copyright (C) 2012 - J.H. Heusala

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/
#ifndef FC__MYSQL_LOG_H
#define FC__MYSQL_LOG_H

#include "fc_types.h"		/* struct connection, struct government */
#include "events.h"		/* enum event_type */
#include "map.h"

void fc_mysql_log_unit(const struct unit* punit);
void fc_mysql_log_city(const struct city* pcity);
void fc_mysql_log_unit_removed(const struct unit* punit);
void fc_mysql_log_city_removed(const struct city* pcity);
void fc_mysql_log_nuke(const struct tile* ptile);
void fc_mysql_log_combat(const struct unit* punit0, const struct unit* punit1);
void fc_mysql_log_tc(int year, int turn);
void fc_mysql_log_chat(const char *message, const struct tile* tile, enum event_type event, int conn_id);

#endif /* FC__MYSQL_LOG_H */
