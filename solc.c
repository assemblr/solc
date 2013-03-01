
#include "solc.h"

unsigned char* solc_compile(char* source, off_t* size) {
    SolList data = solc_parse(source);
    unsigned char* ret = solc_emit(data, size);
    sol_obj_release((SolObject) data);
    return ret;
}

unsigned char* solc_compile_f(FILE* source, off_t* size) {
    SolList data = solc_parse_f(source);
    unsigned char* ret = solc_emit(data, size);
    sol_obj_release((SolObject) data);
    return ret;
}
