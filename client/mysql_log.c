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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>

/* utility */
#include "bitvector.h"
#include "capability.h"
#include "fcintl.h"
#include "log.h"
#include "mem.h"
#include "string_vector.h"
#include "support.h"

/* common */
#include "capstr.h"
#include "events.h"
#include "game.h"
#include "government.h"
#include "idex.h"
#include "map.h"
#include "name_translation.h"
#include "nation.h"
#include "packets.h"
#include "player.h"
#include "research.h"
#include "spaceship.h"
#include "specialist.h"
#include "unit.h"
#include "unitlist.h"
#include "worklist.h"

/* include */
#include "chatline_g.h"
#include "citydlg_g.h"
#include "cityrep_g.h"
#include "connectdlg_g.h"
#include "dialogs_g.h"
#include "editgui_g.h"
#include "gui_main_g.h"
#include "inteldlg_g.h"
#include "mapctrl_g.h"          /* popup_newcity_dialog() */
#include "mapview_g.h"
#include "menu_g.h"
#include "messagewin_g.h"
#include "pages_g.h"
#include "plrdlg_g.h"
#include "repodlgs_g.h"
#include "spaceshipdlg_g.h"
#include "voteinfo_bar_g.h"
#include "wldlg_g.h"

/* client */
#include "agents.h"
#include "attribute.h"
#include "audio.h"
#include "client_main.h"
#include "climap.h"
#include "climisc.h"
#include "connectdlg_common.h"
#include "control.h"
#include "editor.h"
#include "ggzclient.h"
#include "goto.h"               /* client_goto_init() */
#include "helpdata.h"           /* boot_help_texts() */
#include "mapview_common.h"
#include "options.h"
#include "overview_common.h"
#include "tilespec.h"
#include "update_queue.h"
#include "voteinfo.h"
#include "packhand.h"

#include "mysql.h"
#include "mysql_log.h"

#define FC__MYSQL_LOG_TABLE     "fc_log"
#define FC__MYSQL_UNITLOG_TABLE "fc_unitlog"
#define FC__MYSQL_CITYLOG_TABLE "fc_citylog"
#define FC__MYSQL_NUKELOG_TABLE "fc_nukelog"
#define FC__MYSQL_CHATLOG_TABLE "fc_chatlog"
#define FC__MYSQL_COMBATLOG_TABLE "fc_combatlog"
#define FC__MYSQL_TCLOG_TABLE "fc_tclog"

/***********************************************************************

CREATE TABLE `fc_log` (
    log_id           BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    created          TIMESTAMP NOT NULL DEFAULT 0,
    x                INT UNSIGNED,
    y                INT UNSIGNED,
    msg              VARCHAR(255) NOT NULL DEFAULT '',
    PRIMARY KEY(log_id)) CHARACTER SET utf8 ENGINE=InnoDB;

CREATE TABLE `fc_unitlog` (
    unitlog_id               BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    created          TIMESTAMP NOT NULL DEFAULT 0,
    unit_name        VARCHAR(255),
    tile_x           INT UNSIGNED,
    tile_y           INT UNSIGNED,
    owner_name      VARCHAR(255),
    unit_hp          INT UNSIGNED,
    unit_veteran     INT UNSIGNED,
    unit_id          INT UNSIGNED,
    removed          BOOL NOT NULL DEFAULT 0,
    PRIMARY KEY(unitlog_id)) CHARACTER SET utf8 ENGINE=InnoDB;

CREATE TABLE `fc_citylog` (
    citylog_id       BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    created          TIMESTAMP NOT NULL DEFAULT 0,
    city_id          INT UNSIGNED,
    city_name        VARCHAR(255),
    tile_x           INT UNSIGNED,
    tile_y           INT UNSIGNED,
    owner_name       VARCHAR(255),
    city_size             INT,
    city_food_stock       INT,
    city_shield_stock     INT,
    removed          BOOL NOT NULL DEFAULT 0,
    PRIMARY KEY(citylog_id)) CHARACTER SET utf8 ENGINE=InnoDB;

CREATE TABLE `fc_nukelog` (
    nukelog_id       BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    created          TIMESTAMP NOT NULL DEFAULT 0,
    tile_x           INT UNSIGNED,
    tile_y           INT UNSIGNED,
    PRIMARY KEY(nukelog_id)) CHARACTER SET utf8 ENGINE=InnoDB;

************************************************************************/

void fc_mysql_log_unit(const struct unit* punit) {
    fc_mysql_query(
	"INSERT INTO `" FC__MYSQL_UNITLOG_TABLE "` "
	"(created,unit_name,unit_x,unit_y,owner_name,unit_hp,unit_veteran,unit_id) "
	"VALUES (NOW(),%s,%d,%d,%s,%d,%d,%d)",
            unit_rule_name(punit),
            TILE_XY(punit->tile),
            punit->owner->name,
            punit->hp,
            punit->veteran,
            punit->id
	);
}

void fc_mysql_log_unit_removed(const struct unit* punit) {
    fc_mysql_query(
	"INSERT INTO `" FC__MYSQL_UNITLOG_TABLE "` "
	"(created,unit_name,unit_x,unit_y,owner_name,unit_hp,unit_veteran,unit_id,removed) "
	"VALUES (NOW(),%s,%d,%d,%s,%d,%d,%d,1)",
            unit_rule_name(punit),
            TILE_XY(punit->tile),
            punit->owner->name,
            punit->hp,
            punit->veteran,
            punit->id
	);
}

void fc_mysql_log_city(const struct city* pcity) {
  fc_mysql_query(
        "INSERT INTO `" FC__MYSQL_CITYLOG_TABLE "` "
        "(created,city_id,city_name,tile_x,tile_y,owner_name,city_size,removed,city_food_stock,city_shield_stock) "
        "VALUES (NOW(),%d,%s,%d,%d,%s,%d,%d,%d)",
            pcity->id,
            pcity->name,
            TILE_XY(pcity->tile),
            pcity->owner->name,
            pcity->size,
            pcity->food_stock,
            pcity->shield_stock,
	    (removed ? 1 : 0)
	);
}

void fc_mysql_log_city_removed(const struct city* pcity) {
  fc_mysql_query(
        "INSERT INTO `" FC__MYSQL_CITYLOG_TABLE "` "
        "(created,city_id,city_name,tile_x,tile_y,owner_name,city_size,city_food_stock,city_shield_stock) "
        "VALUES (NOW(),%d,%s,%d,%d,%s,%d,%d,%d,1)",
            pcity->id,
            pcity->name,
            TILE_XY(pcity->tile),
            pcity->owner->name,
            pcity->size,
            pcity->food_stock,
            pcity->shield_stock
	);
}

void fc_mysql_log_nuke(const struct tile* ptile) {
	fc_mysql_query(
	        "INSERT INTO `" FC__MYSQL_NUKELOG_TABLE "` "
	        "(created,tile_x,tile_y) "
	        "VALUES (NOW(),%d,%d)",
	            TILE_XY(ptile)
		);
}

void fc_mysql_log_combat(const struct unit* punit0, const struct unit* punit1) {
    fc_mysql_query(
	"INSERT INTO `" FC__MYSQL_COMBATLOG_TABLE "` "
	"(created,tile_x,tile_y,attacker_unit_id,defender_unit_id) "
	"VALUES (NOW(),%d,%d,%d,%d)",
            TILE_XY(punit1->tile),
            punit0->id,
            punit1->id
	);
    fc_mysql_log_unit(punit0);
    fc_mysql_log_unit(punit1);
}

void fc_mysql_log_tc(int year, int turn) {
    fc_mysql_query(
	"INSERT INTO `" FC__MYSQL_TCLOG_TABLE "` "
	"(created,year,turn) "
	"VALUES (NOW(),%d,%d)",
		year,
		turn
	);
}

void fc_mysql_log_chat(const char *message, const struct tile* tile, enum event_type event, int conn_id) {
    fc_mysql_query(
	"INSERT INTO `" FC__MYSQL_CHATLOG_TABLE "` "
	"(created,tile_x,tile_y,message,event) "
	"VALUES (NOW(),%d,%d,%s,%s)",
		TILE_XY(ptile),
		message,
		get_event_message_text(event)
	);
}
