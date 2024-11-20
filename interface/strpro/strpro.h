

#ifndef _STRPRO_H_
#define _STRPRO_H_

void strpro_clean_space(char **str);
void strpro_test();

void strpro_clean_by_ch(char **str_p, char ch);



int strpro_hex2str(void *hex, int hexLen, char *out, int outLen);
int strpro_str2hex(char *in, void *hex, int hexLen);


#endif

