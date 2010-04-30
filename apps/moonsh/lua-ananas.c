#include <dirent.h>
#include <errno.h>
#include "lua.h"
#include "lauxlib.h"

/* inspired by section 26.1 of the Programming In Lua book */
static int l_dir(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	DIR* dir;
	int i;

	dir = opendir(path);
	if (dir == NULL) {
		/* no worky - return a single nil */
		lua_pushnil(L);
		return 1;
	}

	lua_newtable(L);
	i = 1;
	struct dirent* entry;
	while((entry = readdir(dir)) != NULL) {
		lua_pushnumber(L, i++);
		lua_pushstring(L, entry->d_name);
		lua_settable(L, -3);
	}
	closedir(dir);
	return 1;
}

static const struct luaL_reg ananas_funcs[] = {
	{ "dir",  l_dir },
	{ NULL, NULL }
};

int luaopen_ananas(lua_State* L)
{
	luaL_openlib(L, "ananas", ananas_funcs, 0);
	return 1;
}
