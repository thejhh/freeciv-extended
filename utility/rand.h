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
#ifndef FC__RAND_H
#define FC__RAND_H

#include <stdint.h>

#include "support.h"            /* bool type */

/* This is duplicated in shared.h to avoid extra includes: */
#define MAX_UINT32 0xFFFFFFFF

typedef uint32_t RANDOM_TYPE;

typedef struct {
  RANDOM_TYPE v[56];
  int j, k, x;
  bool is_init;			/* initially 0 for static storage */
} RANDOM_STATE;

#define fc_rand(_size) \
  fc_rand_debug((_size), "fc_rand", __LINE__, __FILE__)

RANDOM_TYPE fc_rand_debug(RANDOM_TYPE size, const char *called_as,
                          int line, const char *file);

void fc_srand(RANDOM_TYPE seed);

bool fc_rand_is_init(void);
RANDOM_STATE fc_rand_state(void);
void fc_rand_set_state(RANDOM_STATE state);

void test_random1(int n);

/*===*/

#define fc_randomly(_seed, _size) \
  fc_randomly_debug((_seed), (_size), "fc_randomly", __LINE__, __FILE__)

RANDOM_TYPE fc_randomly_debug(RANDOM_TYPE seed, RANDOM_TYPE size,
                              const char *called_as,
                              int line, const char *file);

#endif  /* FC__RAND_H */
