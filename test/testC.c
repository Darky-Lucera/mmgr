#include <string.h>
#include <stdlib.h>
#include <mmgr.h>

//--------------------------------------
int
main() {
    EnableMultithreadInMMGR();

    // leaks detection
    //----------------------------------
    int *leak3 = (int *)malloc(sizeof(int));
    char *str = _strdup("Hola");

    *leak3 = 1;

    // double free
    //----------------------------------
    int *doubleFree = (int *)malloc(sizeof(int));
    free(doubleFree);
    //free(doubleFree);     // You will hit an assert

    // overflow
    //----------------------------------
    int *overflow = (int *) malloc(sizeof(int) * 5);
    overflow[5] = 1;
    //free(overflow);       // You will hit an assert

    // underflow
    //----------------------------------
    int *underflow = (int *)malloc(sizeof(int) * 5);
    underflow[-1] = 1;
    //free(underflow);      // You will hit an assert

    return 0;
}