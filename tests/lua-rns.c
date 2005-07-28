/* 
 * Lua stub to link lua to ldns
 *
 * This also exports functions for lua use
 * partely based upon:
 * http://tonyandpaige.com/tutorials/lua3.html
 *
 * (c) R. Gieben, NLnet Labs
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>

#include <stdint.h>

/* lua includes */
#include "lua50/lua.h"
#include "lua50/lualib.h"
#include "lua50/lauxlib.h"

/* ldns include */
#include <ldns/dns.h>

/* the Lua interpreter */
lua_State* L;

char *VERSION = "lua-rns 0.1";

#define FOO "Foo" 	/* this is something stupid */

void
usage(FILE *f, char *progname)
{
	fprintf(f, "Synopsis: %s lua-file\n", progname);
	fprintf(f, "   Useless bunch of other options\n");
}

void
version(FILE *f, char *progname)
{
	fprintf(f, "%s version %s\n", progname, VERSION);
}

/*
=====================================================
 Lua bindings for ldns
=====================================================

the l_ prefix stands for lua_ldns_, but that was way
to long to type and the lua_ prefix is already claimed
by lua.

*/

static int
l_rr_new_frm_str(lua_State *L)
{
	/* pop string from stack, make new rr, push rr to
	 * stack and return 1 - to signal the new pointer
	 */
	char *str = strdup((char*)luaL_checkstring(L, 1));

	printf("string retrieved from stack %s\n", str);

	ldns_rr *new_rr = ldns_rr_new_frm_str(str);

	if (new_rr) {
		printf("yeah it worked\n");
	} else {
		printf("uh oh\n");
	}

	lua_pushlightuserdata(L, new_rr);
	return 1;
}

static int
l_rr_print(lua_State *L)
{
	/* we always print to stdout */
/*
 luaL_checktype(lua,1,LUA_TLIGHTUSERDATA);
 QCanvasLine *line = static_cast<QCanvasLine*>(lua_touserdata(lua,1));
*/
	ldns_rr *toprint = (ldns_rr*)lua_touserdata(L, 1); /* pop from the stack */

	ldns_rr_print(stdout, toprint);
	return 0;
}

/* Test function which doesn't call ldns stuff yet */
static int 
l_average(lua_State *L)
{
	int n = lua_gettop(L);
	double sum = 0;
	int i;

	/* loop through each argument */
	for (i = 1; i <= n; i++)
	{
		/* total the arguments */
		sum += lua_tonumber(L, i);
	}

	/* push the average */
	lua_pushnumber(L, sum / n);

	/* push the sum */
	lua_pushnumber(L, sum);

	/* return the number of results */
	return 2;
}

/*
=====================================================
 Lua bindings for ldns
=====================================================
*/

void
register_ldns_functions(void)
{
        /* register our functions */
        lua_register(L, "l_average", l_average);
	lua_register(L, "l_rr_new_frm_str", l_rr_new_frm_str);
	lua_register(L, "l_rr_print", l_rr_print);
}

int
main(int argc, char *argv[])
{
	if (argc != 2) {
		usage(stderr, argv[0]);
		exit(EXIT_FAILURE);
	}

	if (access(argv[1], R_OK)) {
		fprintf(stderr, "File %s is unavailable.\n", argv[1]);
		exit(EXIT_FAILURE);
	}
	
        L = lua_open();
        lua_baselibopen(L);

	register_ldns_functions();

        /* run the script */
        lua_dofile(L, argv[1]);

        lua_close(L);
        exit(EXIT_SUCCESS);
}
