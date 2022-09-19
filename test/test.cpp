#include <mmgr.h>

//--------------------------------------
int
main() {
    // leaks detection
    //----------------------------------
    auto leak1 = new int;
    auto leak2 = new int[5];
    auto leak3 = (int *) malloc(sizeof(int));

    *leak1   = 1;
    leak2[1] = 1;
    *leak3   = 1;

    // double delete
    //----------------------------------
    auto doubleDelete = new int;
    delete doubleDelete;
//    delete doubleDelete;      // You will hit an assert

    // double free
    //----------------------------------
    auto doubleFree = (int *)malloc(sizeof(int));
    free(doubleFree);
    //free(doubleFree);         // You will hit an assert

    // Wrong delete method
    //----------------------------------
    auto wrongDelete1 = new int;
//    delete []wrongDelete1;    // You will hit an assert

    auto wrongDelete2 = new int[5];
    //delete wrongDelete2;      // You will hit an assert

    // mix malloc/delete, new/free
    //----------------------------------
    auto mix1 = (int *)malloc(sizeof(int));
    //delete mix1;              // You will hit an assert

    auto mix2 = (int *)malloc(sizeof(int));
    //delete []mix2;            // You will hit an assert

    auto mix3 = new int;
    //free(mix3);               // You will hit an assert

    auto mix4 = new int[5];
    //free(mix4);               // You will hit an assert

    // overflow
    //----------------------------------
    auto overflow = new int[5];
    overflow[5] = 1;
    //delete[]overflow;           // You will hit an assert

    // underflow
    //----------------------------------
    auto underflow = new int[5];
    underflow[-1] = 1;
    //delete[]underflow;           // You will hit an assert

    return 0;
}