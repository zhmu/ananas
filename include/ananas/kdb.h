#ifndef __ANANAS_KDB_H__
#define __ANANAS_KDB_H__

void kdb_init();
void kdb_enter(const char* why);
void kdb_panic();

#endif /* __ANANAS_KDB_H__ */
