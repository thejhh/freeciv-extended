#!/usr/bin/env python

#
# Freeciv - Copyright (C) 2009
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2, or (at your option)
#   any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#

# The maximum number of enumerators.
max_enum_values=125

# The target file.
target="utility/specenum_gen.h"

# Here are push all defined macros.
macros=[]

import os, sys

def make_header(file):
    file.write('''
 /**************************************************************************
 *                       THIS FILE WAS GENERATED                           *
 * Script: utility/generate_specenum.py                                    *
 *                       DO NOT CHANGE THIS FILE                           *
 **************************************************************************/

/********************************************************************** 
 Freeciv - Copyright (C) 2009
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/
''')

def make_documentation(file):
    file.write('''
/*
 * Include this file to define tools to manage enumerators.  First of all,
 * before including this file, you *MUST* define the following macros:
 * - SPECENUM_NAME: is the name of the enumeration (e.g. 'foo' for defining
 * 'enum foo').
 * - SPECENUM_VALUE%d: define like this all values of your enumeration type
 * (e.g. '#define SPECENUM_VALUE0 FOO_FIRST').
 *
 * The following macros *CAN* be defined:
 * - SPECENUM_INVALID: specifies a value that your 'foo_invalid()' function
 * will return.  Note it cannot be a declared value with SPECENUM_VALUE%d.
 * - SPECENUM_BITWISE: defines if the enumeration should be like
 * [1, 2, 4, 8, etc...] instead of the default of [0, 1, 2, 3, etc...].
 * - SPECENUM_ZERO: can be defined only if SPECENUM_BITWISE was also defined.
 * It defines a 0 value.  Note that if you don't declare this value, 0 passed
 * to the 'foo_is_valid()' function will return 0.
 * - SPECENUM_COUNT: The number of elements in the enum for use in static
 * structs. It can not be used in combination with SPECENUM_BITWISE.
 * SPECENUM_is_valid() will return the invalid element for it.
 *
 * SPECENUM_VALUE%dNAME, SPECENUM_ZERONAME, SPECENUM_COUNTNAME: Can be used
 * to bind the name of the particular enumerator.  If not defined, the
 * default name for 'FOO_FIRST' is '"FOO_FIRST"'.
 *
 * Assuming SPECENUM_NAME were 'foo', including this file would provide
 * the definition for the enumeration type 'enum foo', and prototypes for
 * the following functions:
 *   bool foo_is_bitwise(void);
 *   enum foo foo_min(void);
 *   enum foo foo_max(void);
 *   enum foo foo_invalid(void);
 *   bool foo_is_valid(enum foo);
 *
 *   enum foo foo_begin(void);
 *   enum foo foo_end(void);
 *   enum foo foo_next(enum foo);
 *
 *   const char *foo_name(enum foo);
 *   enum foo foo_by_name(const char *name,
 *                        int (*strcmp_func)(const char *, const char *));
 *
 * Example:
 *   #define SPECENUM_NAME test
 *   #define SPECENUM_BITWISE
 *   #define SPECENUM_VALUE0 TEST0
 *   #define SPECENUM_VALUE1 TEST1
 *   #define SPECENUM_VALUE3 TEST3
 *   #include "specenum_gen.h"
 *
 *  {
 *    static const char *strings[] = {
 *      "TEST1", "test3", "fghdf", NULL
 *    };
 *    enum test e;
 *    int i;
 *
 *    log_verbose("enum test [%d; %d]%s",
 *                test_min(), test_max(), test_bitwise ? " bitwise" : "");
 *
 *    for (e = test_begin(); e != test_end(); e = test_next(e)) {
 *      log_verbose("Value %d is %s", e, test_name(e));
 *    }
 *
 *    for (i = 0; strings[i]; i++) {
 *      e = test_by_name(strings[i], mystrcasecmp);
 *      if (test_is_valid(e)) {
 *        log_verbose("Value is %d for %s", e, strings[i]);
 *      } else {
 *        log_verbose("%s is not a valid name", strings[i]);
 *      }
 *    }
 *  }
 *
 * Will output:
 *   enum test [1, 8] bitwise
 *   Value 1 is TEST0
 *   Value 2 is TEST1
 *   Value 8 is TEST3
 *   Value is 2 for TEST1
 *   Value is 8 for test3
 *   fghdf is not a valid name
 */
''')

def make_macros(file):
    file.write('''
#include "log.h"        /* fc_assert. */
#include "support.h"    /* bool type. */

#ifndef SPECENUM_NAME
#error Must define a SPECENUM_NAME to use this header
#endif

#define SPECENUM_PASTE_(x, y) x ## y
#define SPECENUM_PASTE(x, y) SPECENUM_PASTE_(x, y)

#define SPECENUM_STRING_(x) #x
#define SPECENUM_STRING(x) SPECENUM_STRING_(x)

#define SPECENUM_FOO(suffix) SPECENUM_PASTE(SPECENUM_NAME, suffix)

#ifndef SPECENUM_INVALID
#define SPECENUM_INVALID (-1)
#endif

#ifdef SPECENUM_BITWISE
#ifdef SPECENUM_COUNT
#error Cannot define SPECENUM_COUNT when SPECENUM_BITWISE is defined.
#endif
#define SPECENUM_VALUE(value) (1 << value)
#else /* SPECENUM_BITWISE */
#ifdef SPECENUM_ZERO
#error Cannot define SPECENUM_ZERO when SPECENUM_BITWISE is not defined.
#endif
#define SPECENUM_VALUE(value) (value)
#endif /* SPECENUM_BITWISE */

#undef SPECENUM_MIN_VALUE
#undef SPECENUM_MAX_VALUE
''')
    macros.append("SPECENUM_NAME")
    macros.append("SPECENUM_PASTE_")
    macros.append("SPECENUM_PASTE")
    macros.append("SPECENUM_STRING_")
    macros.append("SPECENUM_STRING")
    macros.append("SPECENUM_FOO")
    macros.append("SPECENUM_INVALID")
    macros.append("SPECENUM_BITWISE")
    macros.append("SPECENUM_VALUE")
    macros.append("SPECENUM_ZERO")
    macros.append("SPECENUM_MIN_VALUE")
    macros.append("SPECENUM_MAX_VALUE")

def make_enum(file):
    file.write('''
/* Enumeration definition. */
enum SPECENUM_NAME {
#ifdef SPECENUM_ZERO
  SPECENUM_ZERO = 0,
#endif
''')

    for i in range(max_enum_values):
        file.write('''
#ifdef SPECENUM_VALUE%d
  SPECENUM_VALUE%d = SPECENUM_VALUE(%d),
#ifndef SPECENUM_MIN_VALUE
#define SPECENUM_MIN_VALUE SPECENUM_VALUE%d
#endif
#ifdef SPECENUM_MAX_VALUE
#undef SPECENUM_MAX_VALUE
#endif
#define SPECENUM_MAX_VALUE SPECENUM_VALUE%d
#endif /* SPECENUM_VALUE%d */
'''%(i,i,i,i,i,i))

    file.write('''
#ifdef SPECENUM_COUNT
  SPECENUM_COUNT = (SPECENUM_MAX_VALUE + 1),
#endif /* SPECENUM_COUNT */
};
''')

    macros.append("SPECENUM_COUNT")
    for i in range(max_enum_values):
        macros.append("SPECENUM_VALUE%d"%i)

def make_is_bitwise(file):
    file.write('''
/**************************************************************************
  Returns TRUE if this enumeration is in bitwise mode.
**************************************************************************/
static inline bool SPECENUM_FOO(_is_bitwise)(void)
{
#ifdef SPECENUM_BITWISE
  return TRUE;
#else
  return FALSE;
#endif
}
''')

def make_min(file):
    file.write('''
/**************************************************************************
  Returns the value of the minimal enumerator.
**************************************************************************/
static inline enum SPECENUM_NAME SPECENUM_FOO(_min)(void)
{
  return SPECENUM_MIN_VALUE;
}
''')

def make_max(file):
    file.write('''
/**************************************************************************
  Returns the value of the maximal enumerator.
**************************************************************************/
static inline enum SPECENUM_NAME SPECENUM_FOO(_max)(void)
{
  return SPECENUM_MAX_VALUE;
}
''')

def make_is_valid(file):
    file.write('''
/**************************************************************************
  Returns TRUE if this enumerator was defined.
**************************************************************************/
static inline bool SPECENUM_FOO(_is_valid)(enum SPECENUM_NAME enumerator)
{
  switch (enumerator) {
#ifdef SPECENUM_ZERO
  case SPECENUM_ZERO:
#endif
''')

    for i in range(max_enum_values):
        file.write('''
#ifdef SPECENUM_VALUE%d
  case SPECENUM_VALUE%d:
#endif
'''%(i,i))

    file.write('''
    return TRUE;
#ifdef SPECENUM_COUNT
  case SPECENUM_COUNT:
    return FALSE;
#endif /* SPECENUM_COUNT */
  }

  return FALSE;
}
''')

def make_invalid(file):
    file.write('''
/**************************************************************************
  Returns an invalid enumerator value.
**************************************************************************/
static inline enum SPECENUM_NAME SPECENUM_FOO(_invalid)(void)
{
  fc_assert(!SPECENUM_FOO(_is_valid(SPECENUM_INVALID)));
  return SPECENUM_INVALID;
}
''')

def make_begin(file):
    file.write('''
/**************************************************************************
  Beginning of the iteration of the enumerators.
**************************************************************************/
static inline enum SPECENUM_NAME SPECENUM_FOO(_begin)(void)
{
  return SPECENUM_FOO(_min)();
}
''')

def make_end(file):
    file.write('''
/**************************************************************************
  End of the iteration of the enumerators.
**************************************************************************/
static inline enum SPECENUM_NAME SPECENUM_FOO(_end)(void)
{
  return SPECENUM_FOO(_invalid)();
}
''')

def make_next(file):
    file.write('''
/**************************************************************************
  Find the next valid enumerator value.
**************************************************************************/
static inline enum SPECENUM_NAME SPECENUM_FOO(_next)(enum SPECENUM_NAME e)
{
  do {
#ifdef SPECENUM_BITWISE
    e <<= 1;
#else
    e++;
#endif

    if (e > SPECENUM_FOO(_max)()) {
      /* End of the iteration. */
      return SPECENUM_FOO(_invalid)();
    }
  } while (!SPECENUM_FOO(_is_valid)(e));

  return e;
}
''')

def make_name(file):
    file.write('''
/**************************************************************************
  Returns the name of the enumerator.
**************************************************************************/
static inline const char *SPECENUM_FOO(_name)(enum SPECENUM_NAME enumerator)
{
  switch (enumerator) {
#ifdef SPECENUM_ZERO
  case SPECENUM_ZERO:
#ifdef SPECENUM_ZERONAME
    return SPECENUM_ZERONAME;
#else
    return SPECENUM_STRING(SPECENUM_ZERO);
#endif
#endif /* SPECENUM_ZERO */
''')
    macros.append("SPECENUM_ZERONAME")

    for i in range(max_enum_values):
        file.write('''
#ifdef SPECENUM_VALUE%d
  case SPECENUM_VALUE%d:
#ifdef SPECENUM_VALUE%dNAME
    return SPECENUM_VALUE%dNAME;
#else
    return SPECENUM_STRING(SPECENUM_VALUE%d);
#endif
#endif /* SPECENUM_VALUE%d */
'''%(i,i,i,i,i,i))
        macros.append("SPECENUM_VALUE%dNAME"%i)

    file.write('''
#ifdef SPECENUM_COUNT
  case SPECENUM_COUNT:
#ifdef SPECENUM_COUNTNAME
    return SPECENUM_COUNTNAME;
#else
    return SPECENUM_STRING(SPECENUM_COUNT);
#endif
#endif /* SPECENUM_COUNT */
  }

  return NULL;
}
''')

def make_by_name(file):
    file.write('''
/**************************************************************************
  Returns the enumerator for the name or *_invalid() if not found.
**************************************************************************/
static inline enum SPECENUM_NAME SPECENUM_FOO(_by_name)
    (const char *name, int (*strcmp_func)(const char *, const char *))
{
  enum SPECENUM_NAME e;
  const char *enum_name;

  for (e = SPECENUM_FOO(_begin)(); e != SPECENUM_FOO(_end)();
       e = SPECENUM_FOO(_next)(e)) {
    if ((enum_name = SPECENUM_FOO(_name)(e))
        && 0 == strcmp_func(name, enum_name)) {
      return e;
    }
  }

  return SPECENUM_FOO(_invalid)();
}
''')

def make_undef(file):
    for macro in macros:
        file.write('''
#undef %s'''%macro)

    file.write('''
''')

# Main function.
def main():
    src_dir=os.path.dirname(sys.argv[0])
    src_root=src_dir+"/.."
    target_name=src_root+'/'+target

    output=open(target_name,"w")

    make_header(output)
    make_documentation(output)
    make_macros(output)
    make_enum(output)
    make_is_bitwise(output)
    make_min(output)
    make_max(output)
    make_is_valid(output)
    make_invalid(output)
    make_begin(output)
    make_end(output)
    make_next(output)
    make_name(output)
    make_by_name(output)
    make_undef(output)

    output.close()

main()
