#ifndef __ANANAS_CMDLINE_H__
#define __ANANAS_CMDLINE_H__

void cmdline_init(char* bootargs, size_t bootargs_len);
const char* cmdline_get_string(const char* key);

#endif /* __ANANAS_CMDLINE_H__ */
