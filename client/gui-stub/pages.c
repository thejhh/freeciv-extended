/********************************************************************** 
 Freeciv - Copyright (C) 1996-2004 - The Freeciv Team
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "connectdlg_g.h"

#include "pages.h"

/**************************************************************************
  Sets the "page" that the client should show.  See also pages_g.h.
**************************************************************************/
void real_set_client_page(enum client_pages page)
{
  /* PORTME */
}

/****************************************************************************
  Set the list of available rulesets.  The default ruleset should be
  "default", and if the user changes this then set_ruleset() should be
  called.
****************************************************************************/
void gui_set_rulesets(int num_rulesets, char **rulesets)
{
  /* PORTME */
}

/**************************************************************************
  Returns current client page
**************************************************************************/
enum client_pages get_current_client_page(void)
{
  /* PORTME */
  return PAGE_MAIN;
}

/**************************************************************************
  update the start page.
**************************************************************************/
void update_start_page(void)
{
  /* PORTME */    
}
