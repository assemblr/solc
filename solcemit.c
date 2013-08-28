
#include "solc.h"

#include <math.h>
#include <float.h>
#include <arpa/inet.h>

#define write(value, size) fwrite(&(value), (size), 1, out)
#define writes(value, size) fwrite((value), (size), 1, out)
#define writec(value) fputc((value), out)

static SolList src;
static FILE* out;

uint64_t htonll(uint64_t value);
uint64_t ntohll(uint64_t value);
void write_length(uint64_t length);

void write_object(SolObject obj);
void write_list(SolList list);
void write_token(SolToken token);
void write_string(SolString string);
void write_number(SolNumber number);

unsigned char* solc_emit(SolList source, off_t* size) {
    src = source;
    
    // create temporary file for writing
    out = tmpfile();
    
    writec('S'); writec('O'); writec('L'); writec('B'); writec('I'); writec('N');
    SOL_LIST_ITR(source, current, i) {
        write_object(current->value);
    }
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
            write_list((SolList) obj);
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
        default:
            fprintf(stderr, "solc: error while emitting binary: unsupported object type\n");
            exit(EXIT_FAILURE);
    }
}

void write_list(SolList list) {
    writec(0x1);
    writec(list->object_mode);
    write_length(list->length);
    SOL_LIST_ITR(list, current, i) {
        write_object(current->value);
    }
}

void write_token(SolToken token) {
    // handle special cases
    // handle data types
    if (!strcmp(token->identifier, "true")) {
        writec(0x5);
        writec(1);
        return;
    }
    if (!strcmp(token->identifier, "false")) {
        writec(0x5);
        writec(0);
        return;
    }
    // otherwise write a token
    writec(0x2);
    uint64_t length = strlen(token->identifier);
    write_length(length);
    writes(token->identifier, sizeof(*token->identifier) * length);
}

void write_string(SolString string) {
    writec(0x4);
    uint64_t length = strlen(string->value);
    write_length(length);
    writes(string->value, sizeof(*string->value) * length);
}

void write_number(SolNumber number) {
    writec(0x3);
    // get the significand and exponent
    int32_t exponent;
    double fraction = frexp(number->value, &exponent);
    int64_t significand = fraction * pow(2, 52);
    
    significand = htonll(significand);
    exponent = htonl(exponent);
    write(significand, sizeof(significand));
    write(exponent, sizeof(exponent));
}
