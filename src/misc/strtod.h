// License: CC0
// Basic interface to strtod.c from the Sanos Operating System Kernel (http://www.jbox.dk/sanos/)
// Function names modified to prevent clashing with whatever OS this code is being built with


#ifndef __STRTOD_H
#define __STRTOD_H

#ifdef  __cplusplus
extern "C" {
#endif

double strtod_i8n(const char *str, char **endptr);
float strtof_i8n(const char *str, char **endptr);
long double strtold_i8n(const char *str, char **endptr);
double atof_i8n(const char *str);

#ifdef  __cplusplus
}
#endif

#endif
