
#include "solc.h"

#include <math.h>
#include <float.h>

#define write(value, size) fwrite(&(value), (size), 1, out)
#define writes(value, size) fwrite((value), (size), 1, out)
#define writec(value) fputc((value), out)

static SolList src;
static FILE* out;

uint64_t htonll(uint64_t value);
uint64_t ntohll(uint64_t value);
void write_length(uint64_t length);

void write_object(SolObject obj);
void write_list(SolList list, bool literal);
void write_object_literal(SolObject obj);
void write_function(SolFunction func);
void write_token(SolToken token);
void write_string(SolString string);
void write_number(SolNumber number);

unsigned char* solc_emit(SolList source, off_t* size) {
    src = source;
    
    // create temporary file for writing
    out = tmpfile();
    
    writec('S'); writec('O'); writec('L'); writec('B'); writec('I'); writec('N');
    SOL_LIST_ITR_BEGIN(source)
        write_object(source->current->value);
    SOL_LIST_ITR_END(source)
    writec(0x0);
    
    // read temporary file into buffer
    fseek(out, 0, SEEK_END);
    off_t length = ftell(out);
    unsigned char* buffer = malloc(length);
    fseek(out, 0, SEEK_SET);
    if (fread(buffer, length, 1, out) != 1) {
        fprintf(stderr, "solc: error while emitting data: error while reading compiled data to buffer\n");
        exit(EXIT_FAILURE);
    }
    
    // store buffer length
    if (size) *size = length;
    
    // remove temporary file
    fclose(out);
    
    return buffer;
}

uint64_t htonll(uint64_t value) {
    uint16_t num = 1;
    if (*(char *)&num == 1) {
        uint32_t high_part = htonl((uint32_t) (value >> 32));
        uint32_t low_part = htonl((uint32_t) (value & 0xFFFFFFFFLL));
        return (((uint64_t) low_part) << 32) | high_part;
    } else {
        return value;
    }
}

uint64_t ntohll(uint64_t value) {
    uint16_t num = 1;
    if (*(char *)&num == 1) {
        uint32_t high_part = ntohl((uint32_t) (value >> 32));
        uint32_t low_part = ntohl((uint32_t) (value & 0xFFFFFFFFLL));
        return (((uint64_t) low_part) << 32) | high_part;
    } else {
        return value;
    }
}

void write_length(uint64_t length) {
    if (length <= 0xF) {
        char data = length + ((char) 0x1 << 0x4);
        writec(data);
    } else if (length <= 0xFFF) {
        uint16_t data = length + ((uint16_t) 0x2 << 0xC);
        data = htons(data);
        write(data, sizeof(data));
    } else if (length <= 0xFFFFF) {
        uint32_t data = length + ((uint32_t) 0x3 << 0x1C);
        data = htonl(data);
        write(data, sizeof(data));
    } else if (length <= 0xFFFFFFF) {
        uint64_t data = length + ((uint64_t) 0x4 << 0x3C);
        data = htonll(data);
        write(data, sizeof(data));
    }
}

void write_object(SolObject obj) {
    switch (obj->type_id) {
        case TYPE_SOL_LIST:
            write_list((SolList) obj, false);
            break;
        case TYPE_SOL_TOKEN:
            write_token((SolToken) obj);
            break;
        case TYPE_SOL_DATATYPE:
            switch (((SolDatatype) obj)->type_id) {
                case DATA_TYPE_NUM:
                    write_number((SolNumber) obj);
                    break;
                case DATA_TYPE_STR:
                    write_string((SolString) obj);
                    break;
                default:
                    fprintf(stderr, "solc: error while emitting binary: unsupported data type\n");
                    exit(EXIT_FAILURE);
            }
            break;
        case TYPE_SOL_FUNC:
            write_function((SolFunction) obj);
            break;
        case TYPE_SOL_OBJ: {
            SolString datatype = (SolString) sol_obj_get_prop(obj, "datatype");
            char* datatype_str = datatype->value;
            sol_obj_release((SolObject) datatype);
            if (!strcmp(datatype_str, "object-literal")) {
                write_object_literal(obj);
                break;
            }
        }
        case TYPE_SOL_OBJ_FROZEN:
            if (((SolObjectFrozen) obj)->value->type_id == TYPE_SOL_LIST) {
                write_list((SolList) ((SolObjectFrozen) obj)->value, true);
            } else {
                writec(0x8);
                write_object(((SolObjectFrozen) obj)->value);
            }
            break;
        default:
            fprintf(stderr, "solc: error while emitting binary: unsupported object type\n");
            exit(EXIT_FAILURE);
    }
}

void write_list(SolList list, bool literal) {
    writec(0x2);
    writec(list->object_mode);
    writec(literal);
    write_length(list->length);
    SOL_LIST_ITR_BEGIN(list)
        write_object(list->current->value);
    SOL_LIST_ITR_END(list)
}

void write_object_literal(SolObject obj) {
    SolString parent = (SolString) sol_obj_get_prop(obj, "parent");
    SolObject object = sol_obj_get_prop(obj, "object");
    writec(0x1);
    if (parent) {
        char* parent_str = parent->value;
        uint64_t parent_length = strlen(parent_str);
        write_length(parent_length);
        writes(parent_str, sizeof(*parent_str) * parent_length);
        sol_obj_release((SolObject) parent);
    } else {
        write_length(0x0);
    }
    uint64_t object_length = HASH_COUNT(object->properties);
    write_length(object_length);
    struct token_pool_entry* el, * tmp;
    HASH_ITER(hh, object->properties, el, tmp) {
        char* key = el->identifier;
        uint64_t key_length = strlen(key);
        write_length(key_length);
        writes(key, sizeof(*key) * key_length);
        write_object(el->binding->value);
    }
    sol_obj_release(object);
}

void write_function(SolFunction func) {
    writec(0x3);
    write_list(func->parameters, false);
    write_list(func->statements, false);
}

void write_token(SolToken token) {
    // handle special cases
    // handle data types
    if (!strcmp(token->identifier, "true")) {
        writec(0x7);
        writec(1);
        return;
    }
    if (!strcmp(token->identifier, "false")) {
        writec(0x7);
        writec(1);
        return;
    }
    // otherwise write a token
    writec(0x4);
    uint64_t length = strlen(token->identifier);
    write_length(length);
    writes(token->identifier, sizeof(*token->identifier) * length);
}

void write_string(SolString string) {
    writec(0x6);
    uint64_t length = strlen(string->value);
    write_length(length);
    writes(string->value, sizeof(*string->value) * length);
}

void write_number(SolNumber number) {
    writec(0x5);
    // handle sign/exponent sign
    char sign = 0x0;
    // get the exponent
    int exp;
    double base = frexp(fabs(number->value), &exp);
    if (exp >= 0) sign |= 0x1;
    if (number->value >= 0) sign |= 0x2;
    exp = abs(exp);
    // get the base
    base *= pow(FLT_RADIX, DBL_MANT_DIG);
    uint64_t base_data = htonll(base);
    // write the data
    writec(sign);
    write(base_data, sizeof(uint64_t));
    write_length(exp);
}
