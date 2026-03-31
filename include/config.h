/**//***************************************************************************

Parse config file

Parses config files into a tree structured database.

*******************************************************************************/

#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <localdefs.h>

/* Tree structured value record */

typedef struct ami_value {

    struct ami_value* next;    /* next value in list */
    struct ami_value* sublist; /* new begin/end block */
    string name;           /* name of node */
    string value;          /* value of this node */

} ami_value, *ami_valptr;

void ami_prttre(ami_valptr list); /* print tree structured config list */
ami_valptr ami_schlst(string id, ami_valptr root); /* search config list */
void ami_merge(ami_valptr* root, ami_valptr newroot); /* merge config trees */
void ami_configfile(string fn, ami_valptr* root); /* parse config file */
void ami_config(ami_valptr* root); /* parse config files */

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_H__ */
