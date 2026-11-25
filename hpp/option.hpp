/** ****************************************************************************
 *
 * Option library interface C++ wrapper declarations
 *
 * See cpp/option.cpp for the rationale. The option record appears here
 * with the ami_ prefix stripped, laid out identically to its C form: the
 * wrapper casts between them. Its name is const, so that a table can be
 * written with string constants.
 *
 ******************************************************************************/

#ifndef __OPTION_HPP__
#define __OPTION_HPP__

extern "C" {

#include <localdefs.h>

}

namespace opt {

/* option record */
typedef struct optrec {

    const char* name; /* name of option */
    ami_long*   flag; /* flag encounter */
    ami_long*   ival; /* integer value */
    float*      fval; /* floating point value */
    char*       str;  /* string value */

} optrec, *optptr;

/* procedures and functions */
void     dequote(char* s);
ami_long option(const char* s, optrec opts[], ami_long single = 0);
ami_long options(ami_long* argi, ami_long* argc, char** argv, optrec opts[],
                 ami_long single = 0);

} /* namespace opt */

#endif /* __OPTION_HPP__ */
