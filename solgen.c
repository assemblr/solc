
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "solgen.h"
#include "solcobj.h"

#define write(value, size) fwrite(&(value), (size), 1, bin_stream)
#define writes(value, size) fwrite((value), (size), 1, bin_stream)
#define writec(value) fputc((value), bin_stream)

#define cprint(...) fprintf(stream, __VA_ARGS__)

static char* expression;
static char* expr_end;
static size_t expr_len;
static FILE* bin_stream;
static FILE* stream;

static int free_list_id = 0;
static int last_list_id = 0;

void prep_expression();
char is_delimiter(char c);

SolCObject read_obj();
SolCList read_list(bool object_mode, int freeze, char bracketed);
SolCObject read_token();
SolCString read_string();
SolCNumber read_number();

void write_obj(SolCObject obj);
void write_list(SolCList list);
void write_token(SolCToken token);
void write_string(SolCString string);
void write_number(SolCNumber number);

void cprint_header();
void cprint_footer();
void cprint_data();

void solc_compile(char* source, FILE* bin_out, FILE* out) {
    // set up processor
    expression = source;
    bin_stream = bin_out;
    stream = out;
    prep_expression();
    expr_len = strlen(expression);
    expr_end = expression + expr_len;
    
    // begin processing
    SolCList top_level = solc_create_list(false);
    int top_level_objs = 0;
    while (expression < expr_end) {
        SolCObject obj = read_obj();
        if (obj != NULL) {
            solc_list_add(top_level, obj);
            top_level_objs++;
        }
    }
    
    // write magic number
    writec('S'); writec('O'); writec('L'); writec('B'); writec('I'); writec('N');
    // begin code generation
    SolCListNode top_level_node = top_level->first;
    for (; top_level_node != NULL; top_level_node = top_level_node->next) {
        write_obj(top_level_node->obj);
    }
    writec(0x0);
    fseek(bin_stream, 0, SEEK_SET);
    
    // print c file
    cprint_header();
    cprint_data();
    cprint_footer();
}

void prep_expression() {
    // trim whitespace
    while (isspace((unsigned char) *expression)) expression++;
    char* end = expression + strlen(expression);
    while (isspace((unsigned char) *expression)) end--;
    *end = '\0';
}

SolCObject read_obj() {
    bool func_modifier = false;
    bool obj_modifier = false;
    while (expression < expr_end) {
        bool func_modifier_active = func_modifier;
        bool obj_modifier_active = obj_modifier;
        func_modifier = false;
        obj_modifier = false;
        
        // skip whitespace chars
        if (isspace((unsigned char) *expression)) {
            expression++;
            continue;
        }
        // process number literals manually
        if (isdigit((unsigned char) *expression)) {
            return (SolCObject) read_number();
        }
        
        switch (*expression) {
            case ';': // COMMENTS
                expression = strchr(expression, '\n') + 1;
                continue;
            case '"': // STRINGS
                return (SolCObject) read_string();
            case '(': // LISTS
                return (SolCObject) read_list(false, 0, 0);
            case '[': // STATEMENTS
                if (func_modifier_active) {
                    SolCList func_list = solc_create_list(false);
                    solc_list_add(func_list, (SolCObject) solc_create_token("^"));
                    solc_list_add(func_list, (SolCObject) solc_create_list(false));
                    solc_list_add(func_list, (SolCObject) read_list(obj_modifier_active, -1, 1));
                    return (SolCObject) func_list;
                }
                return (SolCObject) read_list(obj_modifier_active, -1, 1);
            case '^': // FUNCTION SHORTHAND
                if (*(expression + 1) == '[') {
                    func_modifier = true;
                    expression++;
                    continue;
                }
                return read_token();
            case '@': // OBJECT MODE STATEMENTS
                if (*(expression + 1) == '[') {
                    obj_modifier = true;
                    expression++;
                    continue;
                }
                return read_token();
            default: // TOKENS
                return read_token();
        }
    }
    return NULL;
}

SolCList read_list(bool object_mode, int freeze, char bracketed) {
    // advance past open delimiter
    expression++;
    SolCList list = solc_create_list(object_mode);
    list->freeze_count = freeze;
    while (expression < expr_end) {
        // skip whitespace chars
        if (isspace((unsigned char) *expression)) {
            expression++;
            continue;
        }
        // handle list termination
        if (*expression == (bracketed ? ']' : ')')) {
            expression++;
            return list;
        }
        // if no special cases, just read the object
        solc_list_add(list, read_obj());
    }
    fprintf(stderr, "Encountered unclosed list.");
    return list;
}

SolCObject read_token() {
    char buff[1024];
    char* current = buff;
    for (; !is_delimiter(*expression); expression++) {
        *current = *expression;
        current++;
    }
    *current = '\0';
    int result_len = strlen(buff);
    char* result_str = memcpy(malloc(result_len) + 1, buff, result_len);
    result_str[result_len] = '\0';
    // use object '@' access shorthand
    char* at_pos = strchr(result_str, '@');
    if (at_pos != NULL && (at_pos - result_str > 0 || (at_pos = strchr(result_str + 1, '@')) != NULL)) {
        SolCList result_list = solc_create_list(true);
        result_list->freeze_count = -1;
        char* token = strtok(result_str, "@");
        // handle tokens that start with '@'
        if (*result_str == '@') {
            char* new_token = malloc(sizeof(*token) * (strlen(token) + 1));
            new_token[0] = '@';
            new_token[1] = '\0';
            token = strcat(new_token, token);
        }
        solc_list_add(result_list, (SolCObject) solc_create_token(token));
        solc_list_add(result_list, (SolCObject) solc_create_token("get"));
        token = strtok(NULL, "@");
        solc_list_add(result_list, (SolCObject) solc_create_token(token));
        while ((token = strtok(NULL, "@")) != NULL) {
            SolCList old_list = result_list;
            result_list = solc_create_list(true);
            result_list->freeze_count = -1;
            solc_list_add(result_list, (SolCObject) old_list);
            solc_list_add(result_list, (SolCObject) solc_create_token("get"));
            solc_list_add(result_list, (SolCObject) solc_create_token(token));
        }
        return (SolCObject) result_list;
    } else {
        return (SolCObject) solc_create_token(result_str);
    }
}

SolCString read_string() {
    // advance past open quote
    expression++;
    char buff[1024];
    char* current = buff;
    char escaped = 0;
    while (true) {
        // handle escape sequences
        if (escaped) {
            escaped = 0;
            switch (*expression) {
                case 'b':
                    *current = '\b';
                    break;
                case 't':
                    *current = '\t';
                    break;
                case 'n':
                    *current = '\n';
                    break;
                case 'f':
                    *current = '\f';
                    break;
                case 'r':
                    *current = '\r';
                    break;
                case '"':
                    *current = '"';
                    break;
                case '\\':
                    *current = '\\';
                    break;
                default:
                    printf("WARNING: Invalid escape sequence encountered: \\%c", *expression);
            }
        } else {
            switch (*expression) {
                case '"':
                    *current = '\0';
                    int result_len = strlen(buff) + 1;
                    char* result_str = memcpy(malloc(result_len), buff, result_len);
                    expression++;
                    return solc_create_string(result_str);
                case '\\':
                    escaped = 1;
                    break;
                default:
                    *current = *expression;
            }
        }
        expression++;
        current++;
    }
}

SolCNumber read_number() {
    double value = 0;
    int offset = 0;
    sscanf(expression, "%lf%n", &value, &offset);
    expression += offset;
    return solc_create_number(value);
}

char is_delimiter(char c) {
    static char* delimiters = "()[]{}";
    return isspace(c) || strchr(delimiters, c) != NULL;
}

void write_obj(SolCObject obj) {
    switch (obj->type_id) {
        case SOLC_TYPE_LIST:
            write_list((SolCList) obj);
            break;
        case SOLC_TYPE_TOKEN:
            write_token((SolCToken) obj);
            break;
        case SOLC_TYPE_STR:
            write_string((SolCString) obj);
            break;
        case SOLC_TYPE_NUM:
            write_number((SolCNumber) obj);
            break;
    }
}

void write_list(SolCList list) {
    int list_id = free_list_id;
    free_list_id++;
    writec(0x2);
    writec(list->object_mode);
    list->freeze_count++;
    write(list->freeze_count, sizeof(list->freeze_count));
    list->freeze_count--;
    uint32_t length = 0;
    SolCListNode node = list->first;
    for (; node != NULL; node = node->next) {
        length++;
    }
    write(length, sizeof(length));
    node = list->first;
    for (; node != NULL; node = node->next) {
        write_obj(node->obj);
    }
    last_list_id = list_id;
}

void write_token(SolCToken token) {
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
    uint32_t length = strlen(token->identifier);
    write(length, sizeof(length));
    writes(token->identifier, sizeof(*token->identifier) * length);
}

void write_string(SolCString string) {
    writec(0x6);
    uint64_t length = strlen(string->value);
    write(length, sizeof(length));
    writes(string->value, sizeof(*string->value) * length);
}

void write_number(SolCNumber number) {
    writec(0x5);
    write(number->value, sizeof(number->value));
}

void cprint_header() {
    cprint("#include <sol/runtime.h>\n\n");
    cprint("unsigned char data[] = {");
}

void cprint_footer() {
    cprint("};\n\n");
    cprint("int main(int argc, char** argv) {\n");
    cprint("    sol_runtime_init();\n");
    cprint("    sol_runtime_execute(data);\n");
    cprint("    sol_runtime_destroy();\n");
    cprint("    return 0;\n");
    cprint("}\n");
}

void cprint_data() {
    int ch;
    int i = 0;
    while ((ch = fgetc(bin_stream)) != EOF) {
        if (i++ % 12 == 0)
            fputs ("\n  ", stream);
        fprintf (stream, "0x%02X,", ch);
    }
    fputc ('\n', stream);
}
