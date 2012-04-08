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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* utility */
#include "fcintl.h"
#include "log.h"
#include "mem.h"
#include "rand.h"
#include "shared.h"
#include "support.h"

/* common */
#include "city.h"
#include "fc_interface.h"
#include "game.h"
#include "government.h"
#include "idex.h"
#include "improvement.h"
#include "map.h"
#include "research.h"
#include "tech.h"
#include "unit.h"
#include "unitlist.h"
#include "vision.h"

#include "player.h"


struct player_slot {
  struct player *player;
};

static struct {
  struct player_slot *slots;
  int used_slots; /* number of used/allocated players in the player slots */
} player_slots;

static void player_defaults(struct player *pplayer);

static void player_diplstate_new(const struct player *plr1,
                                 const struct player *plr2);
static void player_diplstate_defaults(const struct player *plr1,
                                      const struct player *plr2);
static void player_diplstate_destroy(const struct player *plr1,
                                     const struct player *plr2);

/* Names of AI levels. These must correspond to enum ai_level in
 * player.h. Also commands to set AI level in server/commands.c
 * must match these. */
static const char *ai_level_names[] = {
  NULL, N_("Away"), N_("Novice"), N_("Easy"), NULL, N_("Normal"),
  NULL, N_("Hard"), N_("Cheating"), NULL, N_("Experimental")
};

/***************************************************************
  Returns true iff p1 can cancel treaty on p2.

  The senate may not allow you to break the treaty.  In this 
  case you must first dissolve the senate then you can break 
  it.  This is waived if you have statue of liberty since you 
  could easily just dissolve and then recreate it. 
***************************************************************/
enum dipl_reason pplayer_can_cancel_treaty(const struct player *p1, 
                                           const struct player *p2)
{
  enum diplstate_type ds = player_diplstate_get(p1, p2)->type;

  if (p1 == p2 || ds == DS_WAR) {
    return DIPL_ERROR;
  }
  if (players_on_same_team(p1, p2)) {
    return DIPL_ERROR;
  }
  if (player_diplstate_get(p1, p2)->has_reason_to_cancel == 0
      && get_player_bonus(p1, EFT_HAS_SENATE) > 0
      && get_player_bonus(p1, EFT_ANY_GOVERNMENT) == 0) {
    return DIPL_SENATE_BLOCKING;
  }
  return DIPL_OK;
}

/***************************************************************
  Returns true iff p1 can be in alliance with p2.

  Check that we are not at war with any of p2's allies. Note 
  that for an alliance to be made, we need to check this both 
  ways.

  The reason for this is to avoid the dread 'love-love-hate' 
  triad, in which p1 is allied to p2 is allied to p3 is at
  war with p1. These lead to strange situations.
***************************************************************/
static bool is_valid_alliance(const struct player *p1, 
                              const struct player *p2)
{
  players_iterate(pplayer) {
    enum diplstate_type ds = player_diplstate_get(p1, pplayer)->type;

    if (pplayer != p1
        && pplayer != p2
        && pplayers_allied(p2, pplayer)
        && ds == DS_WAR /* do not count 'never met' as war here */
        && pplayer->is_alive) {
      return FALSE;
    }
  } players_iterate_end;

  return TRUE;
}

/***************************************************************
  Returns true iff p1 can make given treaty with p2.

  We cannot regress in a treaty chain. So we cannot suggest
  'Peace' if we are in 'Alliance'. Then you have to cancel.

  For alliance there is only one condition: We are not at war 
  with any of p2's allies.
***************************************************************/
enum dipl_reason pplayer_can_make_treaty(const struct player *p1,
                                         const struct player *p2,
                                         enum diplstate_type treaty)
{
  enum diplstate_type existing = player_diplstate_get(p1, p2)->type;

  if (p1 == p2) {
    return DIPL_ERROR; /* duh! */
  }
  if (get_player_bonus(p1, EFT_NO_DIPLOMACY)
      || get_player_bonus(p2, EFT_NO_DIPLOMACY)) {
    return DIPL_ERROR;
  }
  if (treaty == DS_WAR 
      || treaty == DS_NO_CONTACT 
      || treaty == DS_ARMISTICE 
      || treaty == DS_TEAM
      || treaty == DS_LAST) {
    return DIPL_ERROR; /* these are not negotiable treaties */
  }
  if (treaty == DS_CEASEFIRE && existing != DS_WAR) {
    return DIPL_ERROR; /* only available from war */
  }
  if (treaty == DS_PEACE 
      && (existing != DS_WAR && existing != DS_CEASEFIRE)) {
    return DIPL_ERROR;
  }
  if (treaty == DS_ALLIANCE) {
    if (!is_valid_alliance(p1, p2)) {
      /* Our war with a third party prevents entry to alliance. */
      return DIPL_ALLIANCE_PROBLEM_US;
    } else if (!is_valid_alliance(p2, p1)) {
      /* Their war with a third party prevents entry to alliance. */
      return DIPL_ALLIANCE_PROBLEM_THEM;
    }
  }
  /* this check must be last: */
  if (treaty == existing) {
    return DIPL_ERROR;
  }
  return DIPL_OK;
}

/***************************************************************
  Check if pplayer has an embassy with pplayer2.  We always have
  an embassy with ourselves.
***************************************************************/
bool player_has_embassy(const struct player *pplayer,
                        const struct player *pplayer2)
{
  return (pplayer == pplayer2
          || player_has_real_embassy(pplayer, pplayer2)
          || player_has_embassy_from_effect(pplayer, pplayer2));
}

/***************************************************************
  Returns whether pplayer has a real embassy with pplayer2,
  established from a diplomat, or through diplomatic meeting.
***************************************************************/
bool player_has_real_embassy(const struct player *pplayer,
                             const struct player *pplayer2)
{
  return BV_ISSET(pplayer->real_embassy, player_index(pplayer2));
}

/***************************************************************
  Returns whether pplayer has got embassy with pplayer2 thanks
  to an effect (e.g. Macro Polo Embassy).
***************************************************************/
bool player_has_embassy_from_effect(const struct player *pplayer,
                                    const struct player *pplayer2)
{
  return (get_player_bonus(pplayer, EFT_HAVE_EMBASSIES) > 0
          && !is_barbarian(pplayer2));
}

/****************************************************************************
  Return TRUE iff the given player owns the city.
****************************************************************************/
bool player_owns_city(const struct player *pplayer, const struct city *pcity)
{
  return (pcity && pplayer && city_owner(pcity) == pplayer);
}

/****************************************************************************
  Return TRUE iff the player can invade a particular tile (linked with
  borders and diplomatic states).
****************************************************************************/
bool player_can_invade_tile(const struct player *pplayer,
                            const struct tile *ptile)
{
  const struct player *ptile_owner = tile_owner(ptile);

  return (!ptile_owner
          || ptile_owner == pplayer
          || !players_non_invade(pplayer, ptile_owner));
}

/****************************************************************************
  ...
****************************************************************************/
static void player_diplstate_new(const struct player *plr1,
                                 const struct player *plr2)
{
  struct player_diplstate *diplstate;

  fc_assert_ret(plr1 != NULL);
  fc_assert_ret(plr2 != NULL);

  const struct player_diplstate **diplstate_slot
    = plr1->diplstates + player_index(plr2);

  fc_assert_ret(*diplstate_slot == NULL);

  diplstate = fc_calloc(1, sizeof(*diplstate));
  *diplstate_slot = diplstate;
}

/****************************************************************************
  ...
****************************************************************************/
static void player_diplstate_defaults(const struct player *plr1,
                                      const struct player *plr2)
{
  struct player_diplstate *diplstate = player_diplstate_get(plr1, plr2);

  fc_assert_ret(diplstate != NULL);

  diplstate->type                 = DS_NO_CONTACT;
  diplstate->max_state            = DS_NO_CONTACT;
  diplstate->first_contact_turn   = 0;
  diplstate->turns_left           = 0;
  diplstate->has_reason_to_cancel = 0;
  diplstate->contact_turns_left   = 0;
}


/***************************************************************
  Returns diplomatic state type between two players
***************************************************************/
struct player_diplstate *player_diplstate_get(const struct player *plr1,
                                              const struct player *plr2)
{
  fc_assert_ret_val(plr1 != NULL, NULL);
  fc_assert_ret_val(plr2 != NULL, NULL);

  const struct player_diplstate **diplstate_slot
    = plr1->diplstates + player_index(plr2);

  fc_assert_ret_val(*diplstate_slot != NULL, NULL);

  return (struct player_diplstate *) *diplstate_slot;
}

/****************************************************************************
  ...
****************************************************************************/
static void player_diplstate_destroy(const struct player *plr1,
                                     const struct player *plr2)
{
  fc_assert_ret(plr1 != NULL);
  fc_assert_ret(plr2 != NULL);

  const struct player_diplstate **diplstate_slot
    = plr1->diplstates + player_index(plr2);

  if (*diplstate_slot != NULL) {
    free(player_diplstate_get(plr1, plr2));
  }

  *diplstate_slot = NULL;
}

/***************************************************************
  Initialise all player slots (= pointer to player pointers).
***************************************************************/
void player_slots_init(void)
{
  int i;

  /* Init player slots. */
  player_slots.slots = fc_calloc(player_slot_count(),
                                 sizeof(*player_slots.slots));
  /* Can't use the defined functions as the needed data will be
   * defined here. */
  for (i = 0; i < player_slot_count(); i++) {
    player_slots.slots[i].player = NULL;
  }
  player_slots.used_slots = 0;
}

/***************************************************************
  ...
***************************************************************/
bool player_slots_initialised(void)
{
  return (player_slots.slots != NULL);
}

/***************************************************************
  Remove all player slots.
***************************************************************/
void player_slots_free(void)
{
  players_iterate(pplayer) {
    player_destroy(pplayer);
  } players_iterate_end;
  free(player_slots.slots);
  player_slots.slots = NULL;
  player_slots.used_slots = 0;
}

/****************************************************************************
  Returns the first player slot.
****************************************************************************/
struct player_slot *player_slot_first(void)
{
  return player_slots.slots;
}

/****************************************************************************
  Returns the next slot.
****************************************************************************/
struct player_slot *player_slot_next(struct player_slot *pslot)
{
  pslot++;
  return (pslot < player_slots.slots + player_slot_count() ? pslot : NULL);
}

/***************************************************************
  Returns the total number of player slots, i.e. the maximum
  number of players (including barbarians, etc.) that could ever
  exist at once.
***************************************************************/
int player_slot_count(void)
{
  return (MAX_NUM_PLAYER_SLOTS);
}

/****************************************************************************
  Returns the index of the player slot.
****************************************************************************/
int player_slot_index(const struct player_slot *pslot)
{
  fc_assert_ret_val(NULL != pslot, -1);

  return pslot - player_slots.slots;
}

/****************************************************************************
  Returns the team corresponding to the slot. If the slot is not used, it
  will return NULL. See also player_slot_is_used().
****************************************************************************/
struct player *player_slot_get_player(const struct player_slot *pslot)
{
  fc_assert_ret_val(NULL != pslot, NULL);

  return pslot->player;
}

/****************************************************************************
  Returns TRUE is this slot is "used" i.e. corresponds to a valid,
  initialized player that exists in the game.
****************************************************************************/
bool player_slot_is_used(const struct player_slot *pslot)
{
  fc_assert_ret_val(NULL != pslot, FALSE);

  /* No player slot available, if the game is not initialised. */
  if (!player_slots_initialised()) {
    return FALSE;
  }

  return NULL != pslot->player;
}

/****************************************************************************
  Return the possibly unused and uninitialized player slot.
****************************************************************************/
struct player_slot *player_slot_by_number(int player_id)
{
  if (!player_slots_initialised()
      || !(0 <= player_id && player_id < player_slot_count())) {
    return NULL;
  }

  return player_slots.slots + player_id;
}

/****************************************************************************
  Return the highest used player slot index.
****************************************************************************/
int player_slot_max_used_number(void)
{
  int max_pslot = 0;

  player_slots_iterate(pslot) {
    if (player_slot_is_used(pslot)) {
      max_pslot = player_slot_index(pslot);
    }
  } player_slots_iterate_end;

  return max_pslot;
}

/****************************************************************************
  Creates a new player for the slot. If slot is NULL, it will lookup to a
  free slot. If the slot already used, then just return the player.
****************************************************************************/
struct player *player_new(struct player_slot *pslot)
{
  struct player *pplayer;

  fc_assert_ret_val(player_slots_initialised(), NULL);

  if (NULL == pslot) {
    player_slots_iterate(aslot) {
      if (!player_slot_is_used(aslot)) {
        pslot = aslot;
        break;
      }
    } player_slots_iterate_end;

    fc_assert_ret_val(NULL != pslot, NULL);
  } else if (NULL != pslot->player) {
    return pslot->player;
  }

  /* Now create the player. */
  log_debug("Create player for slot %d.", player_slot_index(pslot));
  pplayer = fc_calloc(1, sizeof(*pplayer));
  pplayer->slot = pslot;
  pslot->player = pplayer;

  pplayer->diplstates = fc_calloc(player_slot_count(),
                                  sizeof(*pplayer->diplstates));
  player_slots_iterate(pslot) {
    const struct player_diplstate **diplstate_slot
      = pplayer->diplstates + player_slot_index(pslot);
    *diplstate_slot = NULL;
  } player_slots_iterate_end;

  players_iterate(aplayer) {
    /* Create diplomatic states for all other players. */
    player_diplstate_new(pplayer, aplayer);
    /* Create diplomatic state of this player. */
    if (aplayer != pplayer) {
      player_diplstate_new(aplayer, pplayer);
    }
  } players_iterate_end;

  /* Set default values. */
  player_defaults(pplayer);

  /* Increase number of players. */
  player_slots.used_slots++;

  return pplayer;
}

/****************************************************************************
  ...
****************************************************************************/
static void player_defaults(struct player *pplayer)
{
  int i;

  sz_strlcpy(pplayer->name, ANON_PLAYER_NAME);
  sz_strlcpy(pplayer->username, ANON_USER_NAME);
  sz_strlcpy(pplayer->ranked_username, ANON_USER_NAME);
  pplayer->user_turns = 0;
  pplayer->is_male = TRUE;
  pplayer->government = NULL;
  pplayer->target_government = NULL;
  pplayer->nation = NO_NATION_SELECTED;
  pplayer->team = NULL;
  pplayer->is_ready = FALSE;
  pplayer->nturns_idle = 0;
  pplayer->is_alive = TRUE;

  pplayer->revolution_finishes = -1;

  BV_CLR_ALL(pplayer->real_embassy);
  players_iterate(aplayer) {
    /* create diplomatic states for all other players */
    player_diplstate_defaults(pplayer, aplayer);
    /* create diplomatic state of this player */
    if (aplayer != pplayer) {
      player_diplstate_defaults(aplayer, pplayer);
    }
  } players_iterate_end;

  pplayer->city_style = 0;            /* should be first basic style */
  pplayer->cities = city_list_new();
  pplayer->units = unit_list_new();

  pplayer->economic.gold    = 0;
  pplayer->economic.tax     = PLAYER_DEFAULT_TAX_RATE;
  pplayer->economic.science = PLAYER_DEFAULT_SCIENCE_RATE;
  pplayer->economic.luxury  = PLAYER_DEFAULT_LUXURY_RATE;

  spaceship_init(&pplayer->spaceship);

  pplayer->ai_controlled = FALSE;
  BV_CLR_ALL(pplayer->ai_common.handicaps);
  pplayer->ai_common.skill_level = 0;
  pplayer->ai_common.fuzzy = 0;
  pplayer->ai_common.expand = 100;
  pplayer->ai_common.barbarian_type = NOT_A_BARBARIAN;
  player_slots_iterate(pslot) {
    pplayer->ai_common.love[player_slot_index(pslot)] = 1;
  } player_slots_iterate_end;

  pplayer->ai = NULL;
  pplayer->was_created = FALSE;
  pplayer->is_connected = FALSE;
  pplayer->current_conn = NULL;
  pplayer->connections = conn_list_new();
  BV_CLR_ALL(pplayer->gives_shared_vision);
  for (i = 0; i < B_LAST; i++) {
    pplayer->wonders[i] = WONDER_NOT_BUILT;
  }

  pplayer->attribute_block.data = NULL;
  pplayer->attribute_block.length = 0;
  pplayer->attribute_block_buffer.data = NULL;
  pplayer->attribute_block_buffer.length = 0;

  pplayer->tile_known.vec = NULL;
  pplayer->tile_known.bits = 0;

  /* pplayer->server is initialised in
      ./server/plrhand.c:server_player_init()
     and pplayer->client in
      ./client/climisc.c:client_player_init() */
}

/****************************************************************************
  Clear all player data. If full is set, then the nation and the team will
  be cleared too.
****************************************************************************/
void player_clear(struct player *pplayer, bool full)
{
  if (pplayer == NULL) {
    return;
  }

  /* Clears the attribute blocks. */
  if (pplayer->attribute_block.data) {
    free(pplayer->attribute_block.data);
    pplayer->attribute_block.data = NULL;
  }
  pplayer->attribute_block.length = 0;

  if (pplayer->attribute_block_buffer.data) {
    free(pplayer->attribute_block_buffer.data);
    pplayer->attribute_block_buffer.data = NULL;
  }
  pplayer->attribute_block_buffer.length = 0;

  /* Clears units and cities. */
  unit_list_iterate(pplayer->units, punit) {
    game_remove_unit(punit);
  } unit_list_iterate_end;

  city_list_iterate(pplayer->cities, pcity) {
    game_remove_city(pcity);
  } city_list_iterate_end;

  if (full) {
    team_remove_player(pplayer);

    /* This comes last because log calls in the above functions
     * may use it. */
    if (pplayer->nation != NULL) {
      player_set_nation(pplayer, NULL);
    }
  }
}

/****************************************************************************
  Destroys and remove a player from the game.
****************************************************************************/
void player_destroy(struct player *pplayer)
{
  struct player_slot *pslot;

  fc_assert_ret(NULL != pplayer);

  pslot = pplayer->slot;
  fc_assert(pslot->player == pplayer);

  /* Remove all that is game-dependent in the player structure. */
  player_clear(pplayer, TRUE);

  fc_assert(0 == unit_list_size(pplayer->units));
  unit_list_destroy(pplayer->units);
  fc_assert(0 == city_list_size(pplayer->cities));
  city_list_destroy(pplayer->cities);

  fc_assert(conn_list_size(pplayer->connections) == 0);
  conn_list_destroy(pplayer->connections);

  players_iterate(aplayer) {
    /* destroy the diplomatics states of this player with others ... */
    player_diplstate_destroy(pplayer, aplayer);
    /* and of others with this player. */
    if (aplayer != pplayer) {
      player_diplstate_destroy(aplayer, pplayer);
    }
  } players_iterate_end;
  free(pplayer->diplstates);

  dbv_free(&pplayer->tile_known);

  free(pplayer);
  pslot->player = NULL;
  player_slots.used_slots--;
}

/**************************************************************************
  Return the number of players.
**************************************************************************/
int player_count(void)
{
  return player_slots.used_slots;
}

/**************************************************************************
  Return the player index.

  Currently same as player_number(), paired with player_count()
  indicates use as an array index.
**************************************************************************/
int player_index(const struct player *pplayer)
{
  return player_number(pplayer);
}

/**************************************************************************
  Return the player index/number/id.
**************************************************************************/
int player_number(const struct player *pplayer)
{
  fc_assert_ret_val(NULL != pplayer, -1);
  return player_slot_index(pplayer->slot);
}

/**************************************************************************
  Return struct player pointer for the given player index.

  You can retrieve players that are not in the game (with IDs larger than
  player_count).  An out-of-range player request will return NULL.
**************************************************************************/
struct player *player_by_number(const int player_id)
{
  struct player_slot *pslot = player_slot_by_number(player_id);

  return (NULL != pslot ? player_slot_get_player(pslot) : NULL);
}

/****************************************************************************
  Set the player's nation to the given nation (may be NULL).  Returns TRUE
  iff there was a change.
****************************************************************************/
bool player_set_nation(struct player *pplayer, struct nation_type *pnation)
{
  if (pplayer->nation != pnation) {
    if (pplayer->nation) {
      fc_assert(pplayer->nation->player == pplayer);
      pplayer->nation->player = NULL;
    }
    if (pnation) {
      fc_assert(pnation->player == NULL);
      pnation->player = pplayer;
    }
    pplayer->nation = pnation;
    return TRUE;
  }
  return FALSE;
}

/***************************************************************
...
***************************************************************/
struct player *player_by_name(const char *name)
{
  players_iterate(pplayer) {
    if (fc_strcasecmp(name, pplayer->name) == 0) {
      return pplayer;
    }
  } players_iterate_end;

  return NULL;
}

/**************************************************************************
  Return the leader name of the player.
**************************************************************************/
const char *player_name(const struct player *pplayer)
{
  if (!pplayer) {
    return NULL;
  }
  return pplayer->name;
}

/***************************************************************
  Find player by name, allowing unambigous prefix (ie abbreviation).
  Returns NULL if could not match, or if ambiguous or other
  problem, and fills *result with characterisation of match/non-match
  (see shared.[ch])
***************************************************************/
static const char *player_name_by_number(int i)
{
  struct player *pplayer;

  pplayer = player_by_number(i);
  return player_name(pplayer);
}

/***************************************************************
Find player by its name prefix
***************************************************************/
struct player *player_by_name_prefix(const char *name,
                                     enum m_pre_result *result)
{
  int ind;

  *result = match_prefix(player_name_by_number,
                         player_slot_count(), MAX_LEN_NAME - 1,
                         fc_strncasequotecmp, effectivestrlenquote,
                         name, &ind);

  if (*result < M_PRE_AMBIGUOUS) {
    return player_by_number(ind);
  } else {
    return NULL;
  }
}

/***************************************************************
Find player by its user name (not player/leader name)
***************************************************************/
struct player *player_by_user(const char *name)
{
  players_iterate(pplayer) {
    if (fc_strcasecmp(name, pplayer->username) == 0) {
      return pplayer;
    }
  } players_iterate_end;

  return NULL;
}

/****************************************************************************
  Checks if a unit can be seen by pplayer at (x,y).
  A player can see a unit if he:
  (a) can see the tile AND
  (b) can see the unit at the tile (i.e. unit not invisible at this tile) AND
  (c) the unit is outside a city OR in an allied city AND
  (d) the unit isn't in a transporter, or we are allied AND
  (e) the unit isn't in a transporter, or we can see the transporter
****************************************************************************/
bool can_player_see_unit_at(const struct player *pplayer,
			    const struct unit *punit,
			    const struct tile *ptile)
{
  struct city *pcity;

  /* If the player can't even see the tile... */
  if (TILE_KNOWN_SEEN != tile_get_known(ptile, pplayer)) {
    return FALSE;
  }

  /* Don't show non-allied units that are in transports.  This is logical
   * because allied transports can also contain our units.  Shared vision
   * isn't taken into account. */
  if (punit->transported_by != -1 && unit_owner(punit) != pplayer
      && !pplayers_allied(pplayer, unit_owner(punit))) {
    return FALSE;
  }

  /* Units in cities may be hidden. */
  pcity = tile_city(ptile);
  if (pcity && !can_player_see_units_in_city(pplayer, pcity)) {
    return FALSE;
  }

  /* Allied or non-hiding units are always seen. */
  if (pplayers_allied(unit_owner(punit), pplayer)
      || !is_hiding_unit(punit)) {
    return TRUE;
  }

  /* Hiding units are only seen by the V_INVIS fog layer. */
  return fc_funcs->player_tile_vision_get(ptile, pplayer, V_INVIS);

  return FALSE;
}


/****************************************************************************
  Checks if a unit can be seen by pplayer at its current location.

  See can_player_see_unit_at.
****************************************************************************/
bool can_player_see_unit(const struct player *pplayer,
			 const struct unit *punit)
{
  return can_player_see_unit_at(pplayer, punit, punit->tile);
}

/****************************************************************************
  Return TRUE iff the player can see units in the city.  Either they
  can see all units or none.

  If the player can see units in the city, then the server sends the
  unit info for units in the city to the client.  The client uses the
  tile's unitlist to determine whether to show the city occupied flag.  Of
  course the units will be visible to the player as well, if he clicks on
  them.

  If the player can't see units in the city, then the server doesn't send
  the unit info for these units.  The client therefore uses the "occupied"
  flag sent in the short city packet to determine whether to show the city
  occupied flag.

  Note that can_player_see_city_internals => can_player_see_units_in_city.
  Otherwise the player would not know anything about the city's units at
  all, since the full city packet has no "occupied" flag.

  Returns TRUE if given a NULL player.  This is used by the client when in
  observer mode.
****************************************************************************/
bool can_player_see_units_in_city(const struct player *pplayer,
				  const struct city *pcity)
{
  return (!pplayer
	  || can_player_see_city_internals(pplayer, pcity)
	  || pplayers_allied(pplayer, city_owner(pcity)));
}

/****************************************************************************
  Return TRUE iff the player can see the city's internals.  This means the
  full city packet is sent to the client, who should then be able to popup
  a dialog for it.

  Returns TRUE if given a NULL player.  This is used by the client when in
  observer mode.
****************************************************************************/
bool can_player_see_city_internals(const struct player *pplayer,
				   const struct city *pcity)
{
  return (!pplayer || pplayer == city_owner(pcity));
}

/***************************************************************
 If the specified player owns the city with the specified id,
 return pointer to the city struct.  Else return NULL.
 Now always uses fast idex_lookup_city.

 pplayer may be NULL in which case all cities registered to
 hash are considered - even those not currently owned by any
 player. Callers expect this behavior.
***************************************************************/
struct city *player_city_by_number(const struct player *pplayer, int city_id)
{
  /* We call idex directly. Should use game_city_by_number() instead? */
  struct city *pcity = idex_lookup_city(city_id);

  if (!pcity) {
    return NULL;
  }

  if (!pplayer || (city_owner(pcity) == pplayer)) {
    /* Correct owner */
    return pcity;
  }

  return NULL;
}

/***************************************************************
 If the specified player owns the unit with the specified id,
 return pointer to the unit struct.  Else return NULL.
 Uses fast idex_lookup_city.

 pplayer may be NULL in which case all units registered to
 hash are considered - even those not currently owned by any
 player. Callers expect this behavior.
***************************************************************/
struct unit *player_unit_by_number(const struct player *pplayer, int unit_id)
{
  /* We call idex directly. Should use game_unit_by_number() instead? */
  struct unit *punit = idex_lookup_unit(unit_id);

  if (!punit) {
    return NULL;
  }

  if (!pplayer || (unit_owner(punit) == pplayer)) {
    /* Correct owner */
    return punit;
  }

  return NULL;
}

/*************************************************************************
  Return true iff x,y is inside any of the player's city map.
**************************************************************************/
bool player_in_city_map(const struct player *pplayer,
                        const struct tile *ptile)
{
  city_tile_iterate(CITY_MAP_MAX_RADIUS_SQ, ptile, ptile1) {
    struct city *pcity = tile_city(ptile1);

    if (pcity && city_owner(pcity) == pplayer
        && city_map_radius_sq_get(pcity) >= sq_map_distance(ptile,
                                                            ptile1)) {
      return TRUE;
    }
  } city_tile_iterate_end;

  return FALSE;
}

/**************************************************************************
 Returns the number of techs the player has researched which has this
 flag. Needs to be optimized later (e.g. int tech_flags[TF_COUNT] in
 struct player)
**************************************************************************/
int num_known_tech_with_flag(const struct player *pplayer,
                             enum tech_flag_id flag)
{
  return player_research_get(pplayer)->num_known_tech_with_flag[flag];
}

/**************************************************************************
  Return the expected net income of the player this turn.  This includes
  tax revenue and upkeep, but not one-time purchases or found gold.

  This function depends on pcity->prod[O_GOLD] being set for all cities, so
  make sure the player's cities have been refreshed.
**************************************************************************/
int player_get_expected_income(const struct player *pplayer)
{
  int income = 0;

  /* City income/expenses. */
  city_list_iterate(pplayer->cities, pcity) {
    /* Gold suplus accounts for imcome plus building and unit upkeep. */
    income += pcity->surplus[O_GOLD];

    /* Gold upkeep for buildings and units is defined by the setting
     * 'game.info.gold_upkeep_style':
     * 0: cities pay for buildings and units (this is included in
     *    pcity->surplus[O_GOLD])
     * 1: cities pay only for buildings; the nation pays for units
     * 2: the nation pays for buildings and units */
    if (game.info.gold_upkeep_style > 0) {
      switch (game.info.gold_upkeep_style) {
        case 2:
          /* nation pays for buildings (and units) */
          income -= city_total_impr_gold_upkeep(pcity);
          /* no break */
        case 1:
          /* nation pays for units */
          income -= city_total_unit_gold_upkeep(pcity);
          break;
        default:
          /* fallthru */
          break;
      }
    }

    /* Capitalization income. */
    if (city_production_has_flag(pcity, IF_GOLD)) {
      income += pcity->shield_stock + pcity->surplus[O_SHIELD];
    }
  } city_list_iterate_end;

  return income;
}

/**************************************************************************
 Returns TRUE iff the player knows at least one tech which has the
 given flag.
**************************************************************************/
bool player_knows_techs_with_flag(const struct player *pplayer,
				  enum tech_flag_id flag)
{
  return num_known_tech_with_flag(pplayer, flag) > 0;
}

/**************************************************************************
Locate the city where the players palace is located, (NULL Otherwise) 
**************************************************************************/
struct city *player_palace(const struct player *pplayer)
{
  if (!pplayer) {
    /* The client depends on this behavior in some places. */
    return NULL;
  }
  city_list_iterate(pplayer->cities, pcity) {
    if (is_capital(pcity)) {
      return pcity;
    }
  } city_list_iterate_end;
  return NULL;
}

/**************************************************************************
  AI players may have handicaps - allowing them to cheat or preventing
  them from using certain algorithms.  This function returns whether the
  player has the given handicap.  Human players are assumed to have no
  handicaps.
**************************************************************************/
bool ai_handicap(const struct player *pplayer, enum handicap_type htype)
{
  if (!pplayer->ai_controlled) {
    return TRUE;
  }
  return BV_ISSET(pplayer->ai_common.handicaps, htype);
}

/**************************************************************************
Return the value normal_decision (a boolean), except if the AI is fuzzy,
then sometimes flip the value.  The intention of this is that instead of
    if (condition) { action }
you can use
    if (ai_fuzzy(pplayer, condition)) { action }
to sometimes flip a decision, to simulate an AI with some confusion,
indecisiveness, forgetfulness etc. In practice its often safer to use
    if (condition && ai_fuzzy(pplayer,1)) { action }
for an action which only makes sense if condition holds, but which a
fuzzy AI can safely "forget".  Note that for a non-fuzzy AI, or for a
human player being helped by the AI (eg, autosettlers), you can ignore
the "ai_fuzzy(pplayer," part, and read the previous example as:
    if (condition && 1) { action }
--dwp
**************************************************************************/
bool ai_fuzzy(const struct player *pplayer, bool normal_decision)
{
  if (!pplayer->ai_controlled || pplayer->ai_common.fuzzy == 0) {
    return normal_decision;
  }
  if (fc_rand(1000) >= pplayer->ai_common.fuzzy) {
    return normal_decision;
  }
  return !normal_decision;
}

/**************************************************************************
  Return a text describing an AI's love for you.  (Oooh, kinky!!)
  These words should be adjectives which can fit in the sentence
  "The x are y towards us"
  "The Babylonians are respectful towards us"
**************************************************************************/
const char *love_text(const int love)
{
  if (love <= - MAX_AI_LOVE * 90 / 100) {
    return Q_("?attitude:Genocidal");
  } else if (love <= - MAX_AI_LOVE * 70 / 100) {
    return Q_("?attitude:Belligerent");
  } else if (love <= - MAX_AI_LOVE * 50 / 100) {
    return Q_("?attitude:Hostile");
  } else if (love <= - MAX_AI_LOVE * 25 / 100) {
    return Q_("?attitude:Uncooperative");
  } else if (love <= - MAX_AI_LOVE * 10 / 100) {
    return Q_("?attitude:Uneasy");
  } else if (love <= MAX_AI_LOVE * 10 / 100) {
    return Q_("?attitude:Neutral");
  } else if (love <= MAX_AI_LOVE * 25 / 100) {
    return Q_("?attitude:Respectful");
  } else if (love <= MAX_AI_LOVE * 50 / 100) {
    return Q_("?attitude:Helpful");
  } else if (love <= MAX_AI_LOVE * 70 / 100) {
    return Q_("?attitude:Enthusiastic");
  } else if (love <= MAX_AI_LOVE * 90 / 100) {
    return Q_("?attitude:Admiring");
  } else {
    fc_assert(love > MAX_AI_LOVE * 90 / 100);
    return Q_("?attitude:Worshipful");
  }
}

/**************************************************************************
  Return a diplomatic state as a human-readable string
**************************************************************************/
const char *diplstate_text(const enum diplstate_type type)
{
  static const char *ds_names[DS_LAST] = 
  {
    N_("?diplomatic_state:Armistice"),
    N_("?diplomatic_state:War"), 
    N_("?diplomatic_state:Cease-fire"),
    N_("?diplomatic_state:Peace"),
    N_("?diplomatic_state:Alliance"),
    N_("?diplomatic_state:Never met"),
    N_("?diplomatic_state:Team")
  };

  fc_assert_ret_val_msg(0 <= type && type < DS_LAST, NULL,
                        "Bad diplstate_type: %d.", type);
  return Q_(ds_names[type]);
}

/***************************************************************
  Returns true iff players can attack each other.
***************************************************************/
bool pplayers_at_war(const struct player *pplayer,
                     const struct player *pplayer2)
{
  enum diplstate_type ds = player_diplstate_get(pplayer, pplayer2)->type;
  if (pplayer == pplayer2) {
    return FALSE;
  }
  if (is_barbarian(pplayer) || is_barbarian(pplayer2)) {
    return TRUE;
  }
  return ds == DS_WAR || ds == DS_NO_CONTACT;
}

/***************************************************************
  Returns true iff players are allied.
***************************************************************/
bool pplayers_allied(const struct player *pplayer,
                     const struct player *pplayer2)
{
  enum diplstate_type ds;

  if (!pplayer || !pplayer2) {
    return FALSE;
  }

  if (pplayer == pplayer2) {
    return TRUE;
  }

  if (is_barbarian(pplayer) || is_barbarian(pplayer2)) {
    return FALSE;
  }

  ds = player_diplstate_get(pplayer, pplayer2)->type;

  return (ds == DS_ALLIANCE || ds == DS_TEAM);
}

/***************************************************************
  Returns true iff players are allied or at peace.
***************************************************************/
bool pplayers_in_peace(const struct player *pplayer,
                       const struct player *pplayer2)
{
  enum diplstate_type ds = player_diplstate_get(pplayer, pplayer2)->type;

  if (pplayer == pplayer2) {
    return TRUE;
  }
  if (is_barbarian(pplayer) || is_barbarian(pplayer2)) {
    return FALSE;
  }
  return (ds == DS_PEACE || ds == DS_ALLIANCE || ds == DS_TEAM);
}

/****************************************************************************
  Returns TRUE if players can't enter each others' territory.
****************************************************************************/
bool players_non_invade(const struct player *pplayer1,
			const struct player *pplayer2)
{
  if (pplayer1 == pplayer2 || !pplayer1 || !pplayer2) {
    return FALSE;
  }
  if (is_barbarian(pplayer1) || is_barbarian(pplayer2)) {
    /* Likely an unnecessary test. */
    return FALSE;
  }
  return player_diplstate_get(pplayer1, pplayer2)->type == DS_PEACE;
}

/***************************************************************
  Returns true iff players have peace or cease-fire.
***************************************************************/
bool pplayers_non_attack(const struct player *pplayer,
                         const struct player *pplayer2)
{
  enum diplstate_type ds = player_diplstate_get(pplayer, pplayer2)->type;
  if (pplayer == pplayer2) {
    return FALSE;
  }
  if (is_barbarian(pplayer) || is_barbarian(pplayer2)) {
    return FALSE;
  }
  return (ds == DS_PEACE || ds == DS_CEASEFIRE || ds == DS_ARMISTICE);
}

/**************************************************************************
  Return TRUE if players are in the same team
**************************************************************************/
bool players_on_same_team(const struct player *pplayer1,
                          const struct player *pplayer2)
{
  return (pplayer1->team && pplayer1->team == pplayer2->team);
}

bool is_barbarian(const struct player *pplayer)
{
  return pplayer->ai_common.barbarian_type != NOT_A_BARBARIAN;
}

/**************************************************************************
  Return TRUE iff the player me gives shared vision to player them.
**************************************************************************/
bool gives_shared_vision(const struct player *me, const struct player *them)
{
  return BV_ISSET(me->gives_shared_vision, player_index(them));
}

/**************************************************************************
  Return TRUE iff the two diplstates are equal.
**************************************************************************/
bool are_diplstates_equal(const struct player_diplstate *pds1,
			  const struct player_diplstate *pds2)
{
  return (pds1->type == pds2->type && pds1->turns_left == pds2->turns_left
	  && pds1->has_reason_to_cancel == pds2->has_reason_to_cancel
	  && pds1->contact_turns_left == pds2->contact_turns_left);
}

/***************************************************************************
  Return the number of pplayer2's visible units in pplayer's territory,
  from the point of view of pplayer.  Units that cannot be seen by pplayer
  will not be found (this function doesn't cheat).
***************************************************************************/
int player_in_territory(const struct player *pplayer,
			const struct player *pplayer2)
{
  int in_territory = 0;

  /* This algorithm should work at server or client.  It only returns the
   * number of visible units (a unit can potentially hide inside the
   * transport of a different player).
   *
   * Note this may be quite slow.  An even slower alternative is to iterate
   * over the entire map, checking all units inside the player's territory
   * to see if they're owned by the enemy. */
  unit_list_iterate(pplayer2->units, punit) {
    /* Get the owner of the tile/territory. */
    struct player *owner = tile_owner(punit->tile);

    if (owner == pplayer && can_player_see_unit(pplayer, punit)) {
      /* Found one! */
      in_territory += 1;
    }
  } unit_list_iterate_end;

  return in_territory;
}

/****************************************************************************
  Returns whether this is a valid username.  This is used by the server to
  validate usernames and should be used by the client to avoid invalid
  ones.
****************************************************************************/
bool is_valid_username(const char *name)
{
  return (strlen(name) > 0
          && !fc_isdigit(name[0])
          && is_ascii_name(name)
          && fc_strcasecmp(name, ANON_USER_NAME) != 0);
}

/****************************************************************************
  Returns AI level associated with level name
****************************************************************************/
enum ai_level ai_level_by_name(const char *name)
{
  enum ai_level level;

  for (level = 0; level < AI_LEVEL_LAST; level++) {
    if (ai_level_names[level] != NULL) {
      /* Only consider levels that really have names */
      if (fc_strcasecmp(ai_level_names[level], name) == 0) {
        return level;
      }
    }
  }

  /* No level matches name */
  return AI_LEVEL_LAST;
}

/***************************************************************
  Return localized name of the AI level
***************************************************************/
const char *ai_level_name(enum ai_level level)
{
  fc_assert_ret_val(level >= 0 && level < AI_LEVEL_LAST, NULL);

  if (ai_level_names[level] == NULL) {
    return NULL;
  }

  return _(ai_level_names[level]);
}

/***************************************************************
  Return cmd that sets given ai level
***************************************************************/
const char *ai_level_cmd(enum ai_level level)
{
  fc_assert_ret_val(level >= 0 && level < AI_LEVEL_LAST, NULL);

  if (ai_level_names[level] == NULL) {
    return NULL;
  }

  return ai_level_names[level];
}

/***************************************************************
  Return is AI can be set to given level
***************************************************************/
bool is_settable_ai_level(enum ai_level level)
{
  if (level == AI_LEVEL_AWAY) {
    /* Cannot set away level for AI */
    return FALSE;
  }

  /* It's usable if it has name */
  return ai_level_cmd(level) != NULL;
}

/***************************************************************
  Return number of AI levels in game
***************************************************************/
int number_of_ai_levels(void)
{
  /* We determine this runtime instead of hardcoding correct answer.
   * But as this is constant, we determine it only once. */
  static int count = 0;
  enum ai_level level;

  if (count) {
    /* Answer already known */
    return count;
  }

  /* Determine how many levels are actually usable */
  for (level = 0; level < AI_LEVEL_LAST; level++) {
    if (is_settable_ai_level(level)) {
      count++;
    }
  }

  return count;
}
