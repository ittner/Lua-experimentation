/*
** $Id: lua.c,v 1.150 2005/09/06 17:19:33 roberto Exp $
** Lua stand-alone interpreter
** See Copyright Notice in lua.h
*/


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define lua_c

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "luajit.h"



static lua_State *globalL = NULL;

static const char *progname = LUA_PROGNAME;



static void lstop (lua_State *L, lua_Debug *ar) {
  (void)ar;  /* unused arg. */
  lua_sethook(L, NULL, 0, 0);
  luaL_error(L, "interrupted!");
}


static void laction (int i) {
  signal(i, SIG_DFL); /* if another SIGINT happens before lstop,
                              terminate process (default action) */
  lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}


static void print_usage (void) {
  fprintf(stderr,
  "usage: %s [options] [script [args]].\n"
  "Available options are:\n"
  "  -        execute stdin as a file\n"
  "  -e stat  execute string " LUA_QL("stat") "\n"
  "  -i       enter interactive mode after executing " LUA_QL("script") "\n"
  "  -l name  require library " LUA_QL("name") "\n"
  "  -v       show version information\n"
  "  -j cmd   perform LuaJIT control command\n"
  "  -O[lvl]  set LuaJIT optimization level\n"
  "  --       stop handling options\n" ,
  progname);
  fflush(stderr);
}


static void l_message (const char *pname, const char *msg) {
  if (pname) fprintf(stderr, "%s: ", pname);
  fprintf(stderr, "%s\n", msg);
  fflush(stderr);
}


static int report (lua_State *L, int status) {
  if (status && !lua_isnil(L, -1)) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL) msg = "(error object is not a string)";
    l_message(progname, msg);
    lua_pop(L, 1);
  }
  return status;
}


static int traceback (lua_State *L) {
  lua_getfield(L, LUA_GLOBALSINDEX, "debug");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return 1;
  }
  lua_getfield(L, -1, "traceback");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return 1;
  }
  lua_pushvalue(L, 1);  /* pass error message */
  lua_pushinteger(L, 2);  /* skip this function and traceback */
  lua_call(L, 2, 1);  /* call debug.traceback */
  return 1;
}


static int docall (lua_State *L, int narg, int clear) {
  int status;
  int base = lua_gettop(L) - narg;  /* function index */
  lua_pushcfunction(L, traceback);  /* push traceback function */
  lua_insert(L, base);  /* put it under chunk and args */
  signal(SIGINT, laction);
  status = lua_pcall(L, narg, (clear ? 0 : LUA_MULTRET), base);
  signal(SIGINT, SIG_DFL);
  lua_remove(L, base);  /* remove traceback function */
  return status;
}


static void print_version (void) {
  l_message(NULL, LUA_VERSION "  " LUA_COPYRIGHT);
}


static int getargs (lua_State *L, int argc, char **argv, int n) {
  int narg = argc - (n + 1);  /* number of arguments to the script */
  int i;
  luaL_checkstack(L, narg + 3, "too many arguments to script");
  for (i=n+1; i < argc; i++)
    lua_pushstring(L, argv[i]);
  lua_newtable(L);
  for (i=0; i < argc; i++) {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, -2, i - n);
  }
  return narg;
}


static int dofile (lua_State *L, const char *name) {
  int status = luaL_loadfile(L, name) || docall(L, 0, 1);
  return report(L, status);
}


static int dostring (lua_State *L, const char *s, const char *name) {
  int status = luaL_loadbuffer(L, s, strlen(s), name) || docall(L, 0, 1);
  return report(L, status);
}


static int dolibrary (lua_State *L, const char *name) {
  lua_getglobal(L, "require");
  lua_pushstring(L, name);
  return report(L, lua_pcall(L, 1, 0, 0));
}


static const char *get_prompt (lua_State *L, int firstline) {
  const char *p;
  lua_getfield(L, LUA_GLOBALSINDEX, firstline ? "_PROMPT" : "_PROMPT2");
  p = lua_tostring(L, -1);
  if (p == NULL) p = (firstline ? LUA_PROMPT : LUA_PROMPT2);
  lua_pop(L, 1);  /* remove global */
  return p;
}


static int incomplete (lua_State *L, int status) {
  if (status == LUA_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = lua_tolstring(L, -1, &lmsg);
    const char *tp = msg + lmsg - (sizeof(LUA_QL("<eof>")) - 1);
    if (strstr(msg, LUA_QL("<eof>")) == tp) {
      lua_pop(L, 1);
      return 1;
    }
  }
  return 0;  /* else... */
}


static int pushline (lua_State *L, int firstline) {
  char buffer[LUA_MAXINPUT];
  char *b = buffer;
  size_t l;
  const char *prmt = get_prompt(L, firstline);
  if (lua_readline(L, b, prmt) == 0)
    return 0;  /* no input */
  l = strlen(b);
  if (l > 0 && b[l-1] == '\n')  /* line ends with newline? */
    b[l-1] = '\0';  /* remove it */
  if (firstline && b[0] == '=')  /* first line starts with `=' ? */
    lua_pushfstring(L, "return %s", b+1);  /* change it to `return' */
  else
    lua_pushstring(L, b);
  lua_freeline(L, b);
  return 1;
}


static int loadline (lua_State *L) {
  int status;
  lua_settop(L, 0);
  if (!pushline(L, 1))
    return -1;  /* no input */
  for (;;) {  /* repeat until gets a complete line */
    status = luaL_loadbuffer(L, lua_tostring(L, 1), lua_strlen(L, 1), "=stdin");
    if (!incomplete(L, status)) break;  /* cannot try to add lines? */
    if (!pushline(L, 0))  /* no more input? */
      return -1;
    lua_pushliteral(L, "\n");  /* add a new line... */
    lua_insert(L, -2);  /* ...between the two lines */
    lua_concat(L, 3);  /* join them */
  }
  lua_saveline(L, 1);
  lua_remove(L, 1);  /* remove line */
  return status;
}


static void dotty (lua_State *L) {
  int status;
  const char *oldprogname = progname;
  progname = NULL;
  print_version();
  while ((status = loadline(L)) != -1) {
    if (status == 0) status = docall(L, 0, 0);
    report(L, status);
    if (status == 0 && lua_gettop(L) > 0) {  /* any result to print? */
      lua_getglobal(L, "print");
      lua_insert(L, 1);
      if (lua_pcall(L, lua_gettop(L)-1, 0, 0) != 0)
        l_message(progname, lua_pushfstring(L,
                               "error calling " LUA_QL("print") " (%s)",
                               lua_tostring(L, -1)));
    }
  }
  lua_settop(L, 0);  /* clear stack */
  fputs("\n", stdout);
  fflush(stdout);
  progname = oldprogname;
}

/* ---- start of LuaJIT extensions */

static int loadjitmodule (lua_State *L, const char *notfound) {
  lua_getglobal(L, "require");
  lua_pushliteral(L, "jit.");
  lua_pushvalue(L, -3);
  lua_concat(L, 2);
  if (lua_pcall(L, 1, 1, 0)) {
    const char *msg = lua_tostring(L, -1);
    if (msg && !strncmp(msg, "module ", 7)) {
      l_message(progname, notfound);
      return 1;
    }
    else
      return report(L, 1);
  }
  lua_getfield(L, -1, "start");
  lua_remove(L, -2);  /* drop module table */
  return 0;
}

/* JIT engine control command: try jit library first or load add-on module */
static int dojitcmd (lua_State *L, const char *cmd) {
  const char *val = strchr(cmd, '=');
  lua_pushlstring(L, cmd, val ? val - cmd : strlen(cmd));
  lua_getglobal(L, "jit");  /* get jit.* table */
  lua_pushvalue(L, -2);
  lua_gettable(L, -2);  /* lookup library function */
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);  /* drop non-function and jit.* table, keep module name */
    if (loadjitmodule(L, "unknown luaJIT command"))
      return 1;
  }
  else {
    lua_remove(L, -2);  /* drop jit.* table */
  }
  lua_remove(L, -2);  /* drop module name */
  if (val) lua_pushstring(L, val+1);
  return report(L, lua_pcall(L, val ? 1 : 0, 0, 0));
}

/* start optimizer */
static int dojitopt (lua_State *L, const char *opt) {
  lua_pushliteral(L, "opt");
  if (loadjitmodule(L, "LuaJIT optimizer module not installed"))
    return 1;
  lua_remove(L, -2);  /* drop module name */
  if (*opt) lua_pushstring(L, opt);
  return report(L, lua_pcall(L, *opt ? 1 : 0, 0, 0));
}

/* ---- end of LuaJIT extensions */

#define clearinteractive(i)	(*i &= 2)

static int handle_argv (lua_State *L, int argc, char **argv, int *interactive) {
  if (argv[1] == NULL) {  /* no arguments? */
    *interactive = 0;
    if (lua_stdin_is_tty())
      dotty(L);
    else
      dofile(L, NULL);  /* executes stdin as a file */
  }
  else {  /* other arguments; loop over them */
    int i;
    for (i = 1; argv[i] != NULL; i++) {
      if (argv[i][0] != '-') break;  /* not an option? */
      switch (argv[i][1]) {  /* option */
        case '-': {  /* `--' */
          if (argv[i][2] != '\0') {
            print_usage();
            return 1;
          }
          i++;  /* skip this argument */
          goto endloop;  /* stop handling arguments */
        }
        case '\0': {
          clearinteractive(interactive);
          dofile(L, NULL);  /* executes stdin as a file */
          break;
        }
        case 'i': {
          *interactive = 2;  /* force interactive mode after arguments */
          break;
        }
        case 'v': {
          clearinteractive(interactive);
          print_version();
          break;
        }
        case 'e': {
          const char *chunk = argv[i] + 2;
          clearinteractive(interactive);
          if (*chunk == '\0') chunk = argv[++i];
          if (chunk == NULL) {
            print_usage();
            return 1;
          }
          if (dostring(L, chunk, "=(command line)") != 0)
            return 1;
          break;
        }
        case 'l': {
          const char *filename = argv[i] + 2;
          if (*filename == '\0') filename = argv[++i];
          if (filename == NULL) {
            print_usage();
            return 1;
          }
          if (dolibrary(L, filename))
            return 1;  /* stop if file fails */
          break;
        }
        case 'j': {  /* LuaJIT extension. */
	  const char *cmd = argv[i] + 2;
	  if (*cmd == '\0') cmd = argv[++i];
          if (cmd == NULL) {
            print_usage();
            return 1;
          }
          if (dojitcmd(L, cmd))
	    return 1;
          break;
        }
        case 'O': {  /* LuaJIT extension. */
          if (dojitopt(L, argv[i] + 2))
	    return 1;
          break;
	}
        default: {
          clearinteractive(interactive);
          print_usage();
          return 1;
        }
      }
    } endloop:
    if (argv[i] != NULL) {
      int status;
      const char *filename = argv[i];
      int narg = getargs(L, argc, argv, i);  /* collect arguments */
      lua_setglobal(L, "arg");
      clearinteractive(interactive);
      status = luaL_loadfile(L, filename);
      lua_insert(L, -(narg+1));
      if (status == 0)
        status = docall(L, narg, 0);
      else
        lua_pop(L, narg);      
      return report(L, status);
    }
  }
  return 0;
}


static int handle_luainit (lua_State *L) {
  const char *init = getenv("LUA_INIT");
  if (init == NULL) return 0;  /* status OK */
  else if (init[0] == '@')
    return dofile(L, init+1);
  else
    return dostring(L, init, "=LUA_INIT");
}


struct Smain {
  int argc;
  char **argv;
  int status;
};


static int pmain (lua_State *L) {
  struct Smain *s = (struct Smain *)lua_touserdata(L, 1);
  int status;
  int interactive = 1;
  if (s->argv[0] && s->argv[0][0]) progname = s->argv[0];
  globalL = L;
  luaL_openlibs(L);  /* open libraries */
  status = handle_luainit(L);
  if (status == 0) {
    status = handle_argv(L, s->argc, s->argv, &interactive);
    if (status == 0 && interactive) dotty(L);
  }
  s->status = status;
  return 0;
}


int main (int argc, char **argv) {
  int status;
  struct Smain s;
  lua_State *L = lua_open();  /* create state */
  if (L == NULL) {
    l_message(argv[0], "cannot create state: not enough memory");
    return EXIT_FAILURE;
  }
  s.argc = argc;
  s.argv = argv;
  status = lua_cpcall(L, &pmain, &s);
  report(L, status);
  lua_close(L);
  return (status || s.status) ? EXIT_FAILURE : EXIT_SUCCESS;
}

