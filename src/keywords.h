
#ifndef KEYWORD

int do_loglevel(int nargs, char **args);

#define __MAKE_KEYWORD_ENUM__
#define KEYWORD(symbol, flags, nargs, func) K_##symbol,
enum {
    K_UNKNOWN,
    KEYWORD(config,      SECTION, 0, 0)
    KEYWORD(command,     SECTION, 0, 0)
    KEYWORD(import,      SECTION, 1, 0)
    KEYWORD(daemon,      SECTION, 0, 0)

#endif 	/* #ifndef KEYWORD */
    KEYWORD(loglevel,    COMMAND, 1, do_loglevel)

#ifdef __MAKE_KEYWORD_ENUM__
    KEYWORD_COUNT,
};
#undef __MAKE_KEYWORD_ENUM__
#undef KEYWORD
#endif /* #ifdef __MAKE_KEYWORD_ENUM__ */

