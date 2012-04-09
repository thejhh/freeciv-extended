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
#ifndef FC__MYSQL_H
#define FC__MYSQL_H

#include "fc_types.h"		/* struct connection, struct government */

#include "events.h"		/* enum event_type */
#include "map.h"

#include "packhand_gen.h"

void fc_mysql_query(char *msgfmt, ...);

#endif /* FC__MYSQL_H */
