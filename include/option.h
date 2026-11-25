/**//***************************************************************************

Functions to parse from a list of options

Parses an option or options given as a list. The format of an option is adjusted
according to the OS requirements, meaning that these functions can be used to
cross different operating systems.

We use the option introduction character from services.c. Unix "+" character
options are not supported. Where the character is '-' and the caller asks for
them, single character options are taken as POSIX has them.

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
    ami_long*     flag; /* flag encounter */
    ami_long*     ival; /* integer value */
    float*   fval; /* floating point value */
    string   str;  /* string value */

} ami_optrec, *ami_optptr;

void ami_dequote(string s);
ami_long ami_option(string s, ami_optrec opts[], ami_long single);
ami_long ami_options(ami_long* argi, ami_long* argc, char **argv, ami_optrec  opts[], ami_long single);

#ifdef __cplusplus
}
#endif

#endif /* __OPTION_H__ */
