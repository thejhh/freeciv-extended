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
#include "bitvector.h"
#include "log.h"

/* common */
#include "city.h"
#include "game.h"
#include "government.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "specialist.h"
#include "terrain.h"
#include "unit.h"
#include "unitlist.h"

/* server */
#include "citytools.h"
#include "cityturn.h"           /* city_repair_size() */
#include "maphand.h"
#include "srv_main.h"
#include "unittools.h"

#include "sanitycheck.h"


#ifdef SANITY_CHECKING

#define SANITY_(format, ...) \
  fc_assert_fail(file, function, line, NULL, format, ## __VA_ARGS__)

#define SANITY_CHECK(check) \
  fc_assert_full(file, function, line, check, , NULL)

#define SANITY_CITY(_city, check)                                           \
  fc_assert_full(file, function, line, check, ,                             \
                 "(%4d, %4d) in \"%s\"[%d]", TILE_XY((_city)->tile),        \
                 city_name(_city), (_city)->size)

#define SANITY_TERRAIN(_tile, check)                                        \
  fc_assert_full(file, function, line, check, ,                             \
                 "(%4d, %4d) at \"%s\"", TILE_XY(_tile),                    \
                 terrain_rule_name(tile_terrain(_tile)))

#define SANITY_TILE(_tile, check)                                           \
  do {                                                                      \
    struct city *_tile##_city = tile_city(_tile);                           \
    if (NULL != _tile##_city) {                                             \
      SANITY_CITY(_tile##_city, check);                                     \
    } else {                                                                \
      SANITY_TERRAIN(_tile, check);                                         \
    }                                                                       \
  } while(0)


/**************************************************************************
  Sanity checking on map (tile) specials.
**************************************************************************/
static void check_specials(const char *file, const char *function, int line)
{
  whole_map_iterate(ptile) {
    const struct terrain *pterrain = tile_terrain(ptile);
    bv_special special = tile_specials(ptile);

    if (contains_special(special, S_RAILROAD))
      SANITY_TILE(ptile, contains_special(special, S_ROAD));
    if (contains_special(special, S_FARMLAND))
      SANITY_TILE(ptile, contains_special(special, S_IRRIGATION));

    if (contains_special(special, S_MINE)) {
      SANITY_TILE(ptile, pterrain->mining_result == pterrain);
    }
    if (contains_special(special, S_IRRIGATION)) {
      SANITY_TILE(ptile, pterrain->irrigation_result == pterrain);
    }

    SANITY_TILE(ptile, terrain_index(pterrain) >= T_FIRST 
                       && terrain_index(pterrain) < terrain_count());
  } whole_map_iterate_end;
}

/**************************************************************************
  Sanity checking on fog-of-war (visibility, shared vision, etc.).
**************************************************************************/
static void check_fow(const char *file, const char *function, int line)
{
  if (!game_was_started()) {
    /* The private map of the players is only allocated at game start. */
    return;
  }

  whole_map_iterate(ptile) {
    players_iterate(pplayer) {
      struct player_tile *plr_tile = map_get_player_tile(ptile, pplayer);

      vision_layer_iterate(v) {
        /* underflow of unsigned int */
        SANITY_TILE(ptile, plr_tile->seen_count[v] < 30000);
        SANITY_TILE(ptile, plr_tile->own_seen[v] < 30000);
        SANITY_TILE(ptile, plr_tile->own_seen[v] <= plr_tile->seen_count[v]);
      } vision_layer_iterate_end;

      /* Lots of server bits depend on this. */
      SANITY_TILE(ptile, plr_tile->seen_count[V_INVIS]
		   <= plr_tile->seen_count[V_MAIN]);
      SANITY_TILE(ptile, plr_tile->own_seen[V_INVIS]
		   <= plr_tile->own_seen[V_MAIN]);
    } players_iterate_end;
  } whole_map_iterate_end;

  SANITY_CHECK(game.government_during_revolution != NULL);
  SANITY_CHECK(game.government_during_revolution
	       == government_by_number(game.info.government_during_revolution_id));
}

/**************************************************************************
  Miscellaneous sanity checks.
**************************************************************************/
static void check_misc(const char *file, const char *function, int line)
{
  int nplayers = 0, nbarbs = 0;

  /* Do not use player_slots_iterate as we want to check the index! */
  player_slots_iterate(pslot) {
    if (player_slot_is_used(pslot)) {
      if (is_barbarian(player_slot_get_player(pslot))) {
        nbarbs++;
      }
      nplayers++;
    }
  } player_slots_iterate_end;

  SANITY_CHECK(nplayers == player_count());
  SANITY_CHECK(nbarbs == server.nbarbarians);

  SANITY_CHECK(player_count() <= player_slot_count());
  SANITY_CHECK(team_count() <= MAX_NUM_TEAM_SLOTS);
}

/**************************************************************************
  Sanity checks on the map itself.  See also check_specials.
**************************************************************************/
static void check_map(const char *file, const char *function, int line)
{
  whole_map_iterate(ptile) {
    struct city *pcity = tile_city(ptile);
    int cont = tile_continent(ptile), x, y;

    CHECK_INDEX(tile_index(ptile));
    CHECK_MAP_POS(ptile->x, ptile->y);
    CHECK_NATIVE_POS(ptile->nat_x, ptile->nat_y);

    if (NULL != pcity) {
      SANITY_TILE(ptile, same_pos(pcity->tile, ptile));
      if (BORDERS_DISABLED != game.info.borders) {
        SANITY_TILE(ptile, tile_owner(ptile) != NULL);
      }
    }

    index_to_map_pos(&x, &y, tile_index(ptile));
    SANITY_TILE(ptile, x == ptile->x && y == ptile->y);

    index_to_native_pos(&x, &y, tile_index(ptile));
    SANITY_TILE(ptile, x == ptile->nat_x && y == ptile->nat_y);

    if (is_ocean_tile(ptile)) {
      SANITY_TILE(ptile, cont < 0);
      adjc_iterate(ptile, tile1) {
	if (is_ocean_tile(tile1)) {
	  SANITY_TILE(ptile, tile_continent(tile1) == cont);
	}
      } adjc_iterate_end;
    } else {
      SANITY_TILE(ptile, cont > 0);
      adjc_iterate(ptile, tile1) {
	if (!is_ocean_tile(tile1)) {
	  SANITY_TILE(ptile, tile_continent(tile1) == cont);
	}
      } adjc_iterate_end;
    }

    unit_list_iterate(ptile->units, punit) {
      SANITY_TILE(ptile, same_pos(punit->tile, ptile));

      /* Check diplomatic status of stacked units. */
      unit_list_iterate(ptile->units, punit2) {
	SANITY_TILE(ptile, pplayers_allied(unit_owner(punit), 
                                           unit_owner(punit2)));
      } unit_list_iterate_end;
      if (pcity) {
	SANITY_TILE(ptile, pplayers_allied(unit_owner(punit), 
                                           city_owner(pcity)));
      }
    } unit_list_iterate_end;
  } whole_map_iterate_end;
}

/**************************************************************************
  Verify that the city itself has sane values.
**************************************************************************/
static bool check_city_good(struct city *pcity, const char *file,
                            const char *function, int line)
{
  struct player *pplayer = city_owner(pcity);
  struct tile *pcenter = city_tile(pcity);

  if (NULL == pcenter) {
    /* Editor! */
    SANITY_("(----,----) city has no tile (skipping remaining tests), "
            "at %s \"%s\"[%d]%s",
            nation_rule_name(nation_of_player(pplayer)),
            city_name(pcity), pcity->size,
            "{city center}");
    return FALSE;
  }

  SANITY_CITY(pcity, !terrain_has_flag(tile_terrain(pcenter), TER_NO_CITIES));

  if (BORDERS_DISABLED != game.info.borders) {
    SANITY_CITY(pcity, NULL != tile_owner(pcenter));
  }

  if (NULL != tile_owner(pcenter)) {
    if (tile_owner(pcenter) != pplayer) {
      SANITY_("(%4d,%4d) tile owned by %s, "
              "at %s \"%s\"[%d]%s",
              TILE_XY(pcenter),
              nation_rule_name(nation_of_player(tile_owner(pcenter))),
              nation_rule_name(nation_of_player(pplayer)),
              city_name(pcity), pcity->size,
              "{city center}");
    }
  }

  unit_list_iterate(pcity->units_supported, punit) {
    SANITY_CITY(pcity, punit->homecity == pcity->id);
    SANITY_CITY(pcity, unit_owner(punit) == pplayer);
  } unit_list_iterate_end;

  city_built_iterate(pcity, pimprove) {
    if (is_small_wonder(pimprove)) {
      SANITY_CITY(pcity, city_from_small_wonder(pplayer, pimprove) == pcity);
    } else if (is_great_wonder(pimprove)) {
      SANITY_CITY(pcity, city_from_great_wonder(pimprove) == pcity);
    }
  } city_built_iterate_end;

  return TRUE;
}

/**************************************************************************
  Sanity check city size versus worker and specialist counts.
**************************************************************************/
static void check_city_size(struct city *pcity, const char *file,
                            const char *function, int line)
{
  int delta;
  int citizens = 0;
  struct tile *pcenter = city_tile(pcity);

  SANITY_CITY(pcity, pcity->size >= 1);

  city_tile_iterate_skip_free_worked(city_map_radius_sq_get(pcity), pcenter,
                                  ptile, _index, _x, _y) {
    if (tile_worked(ptile) == pcity) {
      citizens++;
    }
  } city_tile_iterate_skip_free_worked_end;

  citizens += city_specialists(pcity);
  delta = pcity->size - citizens;
  if (0 != delta) {
    SANITY_("(%4d,%4d) %d citizens not equal [size], "
            "repairing \"%s\"[%d]",
            TILE_XY(pcity->tile),
            citizens,
            city_name(pcity), pcity->size);

    citylog_map_workers(LOG_DEBUG, pcity);
    log_debug("[%s (%d)] specialists: %d", city_name(pcity), pcity->id,
              city_specialists(pcity));

    city_repair_size(pcity, delta);
    city_refresh_from_main_map(pcity, NULL);
  }
}

/**************************************************************************
  Verify that the number of people with feelings + specialists equal
  city size.
**************************************************************************/
void real_sanity_check_feelings(const struct city *pcity,
                                const char *file,
                                const char *function, int line)
{
  int ccategory;
  int spe = city_specialists(pcity);

  for (ccategory = CITIZEN_HAPPY; ccategory < CITIZEN_LAST; ccategory++) {
    int sum = 0;
    int feel;

    for (feel = FEELING_BASE; feel < FEELING_LAST; feel++) {
      sum += pcity->feel[ccategory][feel];
    }

    SANITY_CITY(pcity, pcity->size - spe == sum);
  }
}

/**************************************************************************
  Verify that the city has sane values.
**************************************************************************/
void real_sanity_check_city_all(struct city *pcity, const char *file,
                                const char *function, int line)
{
  if (check_city_good(pcity, file, function, line)) {
    check_city_size(pcity, file, function, line);
    /* TODO: check_feelings. Currently fails far too often to be included. */
  }
}

/**************************************************************************
  Verify that the city has sane values.
**************************************************************************/
void real_sanity_check_city(struct city *pcity, const char *file,
                            const char *function, int line)
{
  if (check_city_good(pcity, file, function, line)) {
    check_city_size(pcity, file, function, line);
  }
}

/**************************************************************************
  Sanity checks on all cities in the world.
**************************************************************************/
static void check_cities(const char *file, const char *function, int line)
{
  players_iterate(pplayer) {
    city_list_iterate(pplayer->cities, pcity) {
      SANITY_CITY(pcity, city_owner(pcity) == pplayer);

      real_sanity_check_city(pcity, file, function, line);
    } city_list_iterate_end;
  } players_iterate_end;
}

/**************************************************************************
  Sanity checks on all units in the world.
**************************************************************************/
static void check_units(const char *file, const char *function, int line)
{
  players_iterate(pplayer) {
    unit_list_iterate(pplayer->units, punit) {
      struct tile *ptile = punit->tile;
      struct city *pcity;
      struct city *phome;
      struct unit *transporter = NULL, *transporter2 = NULL;

      SANITY_CHECK(unit_owner(punit) == pplayer);

      if (IDENTITY_NUMBER_ZERO != punit->homecity) {
        SANITY_CHECK(phome = player_city_by_number(pplayer,
                                                   punit->homecity));
	if (phome) {
	  SANITY_CHECK(city_owner(phome) == pplayer);
	}
      }

      if (!can_unit_continue_current_activity(punit)) {
	SANITY_("(%4d,%4d) %s has activity %s, "
		"but it can't continue at %s",
		TILE_XY(ptile),
		unit_rule_name(punit),
		get_activity_text(punit->activity),
		tile_get_info_text(ptile, 0));
      }

      pcity = tile_city(ptile);
      if (pcity) {
	SANITY_CHECK(pplayers_allied(city_owner(pcity), pplayer));
      }

      SANITY_CHECK(punit->moves_left >= 0);
      SANITY_CHECK(punit->hp > 0);

      if (punit->transported_by != -1) {
        transporter = game_unit_by_number(punit->transported_by);
        SANITY_CHECK(transporter != NULL);

	/* Make sure the transporter is on the tile. */
	unit_list_iterate(punit->tile->units, tile_unit) {
	  if (tile_unit == transporter) {
	    transporter2 = tile_unit;
	  }
	} unit_list_iterate_end;
	SANITY_CHECK(transporter2 != NULL);

        /* Also in the list of owner? */
        if (NULL != transporter) {
          SANITY_CHECK(player_unit_by_number(unit_owner(transporter),
                                             punit->transported_by) != NULL);
          SANITY_CHECK(same_pos(ptile, transporter->tile));
        }

        /* Transporter capacity will be checked when transporter itself
	 * is checked */
      }

      /* Check for ground units in the ocean. */
      if (!can_unit_exist_at_tile(punit, ptile)) {
        SANITY_CHECK(punit->transported_by != -1);
        SANITY_CHECK(transporter != NULL
                     && can_unit_transport(transporter, punit));
      }

      /* Check for over-full transports. */
      SANITY_CHECK(get_transporter_occupancy(punit)
	     <= get_transporter_capacity(punit));
    } unit_list_iterate_end;
  } players_iterate_end;
}

/**************************************************************************
  Sanity checks on all players.
**************************************************************************/
static void check_players(const char *file, const char *function, int line)
{
  players_iterate(pplayer) {
    int found_palace = 0;

    if (!pplayer->is_alive) {
      /* Dead players' units and cities are disbanded in kill_player(). */
      SANITY_CHECK(unit_list_size(pplayer->units) == 0);
      SANITY_CHECK(city_list_size(pplayer->cities) == 0);

      continue;
    }

    SANITY_CHECK(pplayer->server.aidata != NULL);
    SANITY_CHECK(!pplayer->nation || pplayer->nation->player == pplayer);
    SANITY_CHECK(player_list_search(team_members(pplayer->team), pplayer));

    city_list_iterate(pplayer->cities, pcity) {
      if (is_capital(pcity)) {
	found_palace++;
      }
      SANITY_CITY(pcity, found_palace <= 1);
    } city_list_iterate_end;

    players_iterate(pplayer2) {
      SANITY_CHECK(player_diplstate_get(pplayer, pplayer2)->type
                   == player_diplstate_get(pplayer2, pplayer)->type);
      if (player_diplstate_get(pplayer, pplayer2)->type == DS_CEASEFIRE) {
        SANITY_CHECK(player_diplstate_get(pplayer, pplayer2)->turns_left
                     == player_diplstate_get(pplayer2, pplayer)->turns_left);
      }
      if (pplayer->is_alive
          && pplayer2->is_alive
          && pplayers_allied(pplayer, pplayer2)) {
        enum dipl_reason allied_players_can_be_allied =
          pplayer_can_make_treaty(pplayer, pplayer2, DS_ALLIANCE);
        SANITY_CHECK(allied_players_can_be_allied
                     != DIPL_ALLIANCE_PROBLEM_US);
        SANITY_CHECK(allied_players_can_be_allied
                     != DIPL_ALLIANCE_PROBLEM_THEM);
      }
    } players_iterate_end;

    if (pplayer->revolution_finishes == -1) {
      if (government_of_player(pplayer) == game.government_during_revolution) {
        SANITY_("%s government is anarchy, but does not finish!",
                nation_rule_name(nation_of_player(pplayer)));
      }
      SANITY_CHECK(government_of_player(pplayer) != game.government_during_revolution);
    } else if (pplayer->revolution_finishes > game.info.turn) {
      SANITY_CHECK(government_of_player(pplayer) == game.government_during_revolution);
    } else {
      /* Things may vary in this case depending on when the sanity_check
       * call is made.  No better check is possible. */
    }

    /* Dying players shouldn't be left around.  But they are. */
    SANITY_CHECK(!BV_ISSET(pplayer->server.status, PSTATUS_DYING));
  } players_iterate_end;

  nations_iterate(pnation) {
    SANITY_CHECK(!pnation->player || pnation->player->nation == pnation);
  } nations_iterate_end;

  teams_iterate(pteam) {
    player_list_iterate(team_members(pteam), pplayer) {
      SANITY_CHECK(pplayer->team == pteam);
    } player_list_iterate_end;
  } teams_iterate_end;
}

/****************************************************************************
  Sanity checking on teams.
****************************************************************************/
static void check_teams(const char *file, const char *function, int line)
{
  int count[MAX_NUM_TEAM_SLOTS];

  memset(count, 0, sizeof(count));
  players_iterate(pplayer) {
    /* For the moment, all players have teams. */
    SANITY_CHECK(pplayer->team != NULL);
    if (pplayer->team) {
      count[team_index(pplayer->team)]++;
    }
  } players_iterate_end;

  team_slots_iterate(tslot) {
    if (team_slot_is_used(tslot)) {
      struct team *pteam = team_slot_get_team(tslot);
      fc_assert_exit(pteam);
      SANITY_CHECK(player_list_size(team_members(pteam))
                   == count[team_slot_index(tslot)]);
    }
  } team_slots_iterate_end;
}

/**************************************************************************
  Sanity checking on connections.
**************************************************************************/
static void check_connections(const char *file, const char *function,
                              int line)
{
  /* est_connections is a subset of all_connections */
  SANITY_CHECK(conn_list_size(game.all_connections)
	       >= conn_list_size(game.est_connections));
}

/**************************************************************************
  Do sanity checks on the server state.  Call this once per turn or
  whenever you feel like it.

  But be careful, calling it too much would make the server slow down.  And
  at some times the server isn't supposed to be in a sane state so you
  can't call it in the middle of an operation that is supposed to be
  atomic.
**************************************************************************/
void real_sanity_check(const char *file, const char *function, int line)
{
  if (!map_is_empty()) {
    /* Don't sanity-check the map if it hasn't been created yet (this
     * happens when loading scenarios). */
    check_specials(file, function, line);
    check_map(file, function, line);
    check_cities(file, function, line);
    check_units(file, function, line);
    check_fow(file, function, line);
  }
  check_misc(file, function, line);
  check_players(file, function, line);
  check_teams(file, function, line);
  check_connections(file, function, line);
}

/*****************************************************************************
  Verify that the tile has sane values. This should be called after the
  terrain is changed.
*****************************************************************************/
void real_sanity_check_tile(struct tile *ptile, const char *file,
                            const char *function, int line)
{
  SANITY_CHECK(ptile != NULL);
  SANITY_CHECK(ptile->terrain != NULL);

  unit_list_iterate(ptile->units, punit) {
    /* Check if the units can survive on the tile (terrain). Here only the
     * 'easy' test if the unit is transported is done. A complete check is
     * done by check_units() in real_sanity_check(). */
    if (!can_unit_exist_at_tile(punit, ptile)
        && punit->transported_by != -1) {
      SANITY_("(%4d,%4d) %s can't survive on %s", TILE_XY(ptile),
              unit_rule_name(punit), tile_get_info_text(ptile, 0));
    }
  } unit_list_iterate_end;
}

#endif /* SANITY_CHECKING */
