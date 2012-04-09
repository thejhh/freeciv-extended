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

// FIXME: Add this to config.h
#ifndef HAVE_CLIENT_MYSQL
  #define HAVE_CLIENT_MYSQL 1
#endif

#include <string.h>
#include <time.h>

#ifdef HAVE_CLIENT_MYSQL
#include <mysql/mysql.h>
#endif /* HAVE_CLIENT_MYSQL */

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

#ifdef HAVE_CLIENT_MYSQL
  #define CLIENT_MYSQL_LOG_TABLE     "fc_log"
  #define CLIENT_MYSQL_UNITLOG_TABLE "fc_unitlog"

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
    unit_x           INT UNSIGNED,
    unit_y           INT UNSIGNED,
    player_name      VARCHAR(255),
    unit_hp          INT UNSIGNED,
    unit_veteran     INT UNSIGNED,
    unit_id          INT UNSIGNED,
    PRIMARY KEY(unitlog_id)) CHARACTER SET utf8 ENGINE=InnoDB;


************************************************************************/

#endif /* HAVE_CLIENT_MYSQL */

/****************************************************************************
  Connect to MySQL server JIT-style and reuse static connection. 

  If connection was already initialized this function will ping the 
  connection to test it's working.

  Reads configurations from ~/.my.cnf from section [freeciv_client].

  Note: This reconnect does not work before MySQL 5.1.6 because of bug.
****************************************************************************/
#ifdef HAVE_CLIENT_MYSQL
static MYSQL* client_mysql_connect() {
	static bool initilized = false;
	static MYSQL mysql;
	if(initilized) return (mysql_ping(&mysql) == 0) ? &mysql : NULL;
	initilized = true;
	mysql_init(&mysql);
	mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP, "freeciv_client");
	mysql_options(&mysql, MYSQL_OPT_RECONNECT, true);
	return mysql_real_connect(&mysql, NULL, NULL, NULL, NULL, 0, NULL, 0);
}
#endif /* HAVE_CLIENT_MYSQL */

/****************************************************************************
  Execute generic MySQL query in printf style.

  Format:
    %s - Escaped string
    %d - Integer
    %% - A single %

  TODO (not implemented):
    %S - unescaped string
    %f - floats

  Returns nothing.

****************************************************************************/
void fc_mysql_query(char *query_fmt, ...) {
#ifdef HAVE_CLIENT_MYSQL
	MYSQL *mysql = client_mysql_connect();
	char* query_fmt_p = NULL;
	char query[1024] = "";
	const int query_size = sizeof(query);
	char* query_p = NULL;
	int query_n = 0;
	int buffer_result = 0;
	bool error = false;
	va_list ap;

	if(mysql == NULL) {
		log_error("Connect failed for MySQL query log");
		return;
	}

	va_start(ap, query_fmt);
	query_p = query;
	query_fmt_p = query_fmt;
	while( (*query_fmt_p != 0) && (query_n < query_size) ) {

		// If part of static query...
		if(*query_fmt_p != '%') {
			*query_p = *query_fmt_p;
			query_fmt_p++;
			query_p++;
			query_n++;
			continue;
		}

		// ...or placeholder for variable
		query_fmt_p++;
		if(*query_fmt_p == 0) {
			break;
		}

		// digit
		if( (*query_fmt_p == 'd') && (query_size-query_n >= 0) ) {
			int digit = va_arg( ap, int );
			buffer_result = fc_snprintf(query_p, query_size-query_n, "%d", digit);
			if( (buffer_result > 0) && (buffer_result < query_size-query_n) ) {
				query_p += buffer_result;
				query_n += buffer_result;
			}
			query_fmt_p++;
			continue;
		}

		// string
		if(*query_fmt_p == 's') {
			char* str = va_arg( ap, char* );
			int str_len = strlen(str);
			unsigned long res = 0;

			if(query_size-query_n < str_len*2+1+2) {
				log_error("No enough memory to escape MySQL query param (%d/%d)", query_size-query_n, str_len*2+1+2);
				error = true;
				break;
			}

			*query_p = '\'';
			query_p++;
			query_n++;

			res = mysql_real_escape_string(mysql, query_p, str, str_len);
			if( res > 0 ) {
				query_p += res;
				query_n += res;
			}

			*query_p = '\'';
			query_p++;
			query_n++;

			query_fmt_p++;
			continue;
		}

		// Escaped placeholder
		if(*query_fmt_p == '%') {
			*query_p = '%';
			query_p++;
			query_n++;
			query_fmt_p++;
			continue;
		}

		// unknown type
		*query_p = '%';
		query_p++;
		query_n++;
	}
	va_end(ap);

	if(error) return;

	if(query_n >= query_size) {
		log_error("No enough memory to prepare MySQL query (%d/%d)", query_n, query_size);
		return;
	}

	*query_p = 0;
	query_p++;
	query_n++;

	if (mysql_query(mysql, query)) {
		log_error("Query to MySQL failed (%s)", mysql_error(mysql));
	}
#endif /* HAVE_CLIENT_MYSQL */
}
