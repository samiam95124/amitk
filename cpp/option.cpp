/** ****************************************************************************
 *
 * Option library interface C++ wrapper
 *
 * Wraps the calls in option with C++ conventions, the same way
 * cpp/network.cpp wraps the network calls. This brings:
 *
 * 1. The functions and types do not need an "ami_" prefix: the opt
 * namespace does the isolation. The namespace is opt and not option
 * because option is a function in it, and a name cannot be both to a
 * program that is using the namespace.
 *
 * 2. The option record's name is const, so that an option table can be
 * written with string constants, which C++ will not give to a char*.
 *
 * 3. The single flag defaults off.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

extern "C" {

#include <localdefs.h>
#include <option.h>

}

#include "option.hpp"

namespace opt {

/* procedures and functions */
void dequote(char* s) { ami_dequote(s); }
ami_long option(const char* s, optrec opts[], ami_long single)
    { return ami_option((char*)s, (ami_optrec*)opts, single); }
ami_long options(ami_long* argi, ami_long* argc, char** argv, optrec opts[],
                 ami_long single)
    { return ami_options(argi, argc, argv, (ami_optrec*)opts, single); }

} /* namespace opt */
