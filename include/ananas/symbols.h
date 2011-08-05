#ifndef __SYMBOLS_H__
#define __SYMBOLS_H__

struct SYMBOL {
	addr_t		s_addr;
	const char*	s_name;
	int		s_flags;
};

void symbols_init();
int symbol_resolve_addr(addr_t addr, struct SYMBOL* s);
int symbol_resolve_name(const char* name, struct SYMBOL* s);

#endif /* __SYMBOLS_H__ */
