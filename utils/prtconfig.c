/*******************************************************************************

                           PRINT CURRENT CONFIGURATION TREE

Parses and prints the current Petit-Ami configuration tree.

*******************************************************************************/

#include <stdio.h>

#include <config.h>

int main()

{

    ami_valptr root;

    printf("Petit-Ami configuration tree:\n");
    printf("\n");

    root = NULL; /* clear root */
    ami_config(&root); /* parse and load the tree */
    ami_prttre(root); /* print resulting tree */

}
