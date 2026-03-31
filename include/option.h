/**//***************************************************************************

Functions to parse from a list of options

Parses an option or options given as a list. The format of an option is adjusted
according to the OS requirements, meaning that these functions can be used to
cross different operating systems.

We use the option introduction character from services.c. This means that Unix/
linux single character options, and Unix "+" character options are not
supported.

The following option formats are supported:

<lead>option
<lead>option=<number>
<lead>option=<string>

The <lead> is whatever option character services says.

*******************************************************************************/

#ifndef __OPTION_H__
#define __OPTION_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <localdefs.h>

/* option record */
typedef struct {

    string   name; /* name of option */
    int*     flag; /* flag encounter */
    int*     ival; /* integer value */
    float*   fval; /* floating point value */
    string   str;  /* string value */

} ami_optrec, *ami_optptr;

void ami_dequote(string s);
int ami_option(string s, ami_optrec opts[], int single);
int ami_options(int* argi, int* argc, char **argv, ami_optrec  opts[], int single);

#ifdef __cplusplus
}
#endif

#endif /* __OPTION_H__ */
