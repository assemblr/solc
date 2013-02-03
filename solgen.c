
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "solgen.h"
#include "solcobj.h"

#define write(...) fprintf(stream, __VA_ARGS__)

static char* expression;
static char* expr_end;
static size_t expr_len;
static FILE* stream;

static int free_list_id = 0;
static int last_list_id = 0;

void prep_expression();
char is_delimiter(char c);

SolCObject read_obj();
SolCList read_list(bool object_mode, int freeze, char bracketed);
SolCToken read_token();
SolCString read_string();
SolCNumber read_number();

void write_header();
void write_footer();

void write_obj(SolCObject obj);
void write_list(SolCList list);
void write_token(SolCToken token);
void write_string(SolCString string);
void write_number(SolCNumber number);

void solc_compile(char* source, FILE* out) {
    // set up processor
    expression = source;
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
    
    // begin code generation
    write_header();
    SolCListNode top_level_node = top_level->first;
    int* top_level_ids = malloc(sizeof(int) * top_level_objs);
    for (int current_obj = 0; top_level_node != NULL; top_level_node = top_level_node->next) {
        top_level_ids[current_obj] = free_list_id;
        write_obj(top_level_node->obj);
        current_obj++;
    }
    for (int i = 0; i < top_level_objs; i++) {
        write("    sol_obj_release(sol_obj_evaluate(sol_obj_retain((SolObject) list_%i)));\n", top_level_ids[i]);
    }
    write_footer();
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
                return (SolCObject) read_token();
            case '@': // OBJECT MODE STATEMENTS
                if (*(expression + 1) == '[') {
                    obj_modifier = true;
                    expression++;
                    continue;
                }
                return (SolCObject) read_token();
            default: // TOKENS
                return (SolCObject) read_token();
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

SolCToken read_token() {
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
    return solc_create_token(result_str);
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

void write_header() {
    write("#include <sol/runtime.h>\n");
    write("#include <stdbool.h>\n\n");
    write("int main(int argc, char** argv) {\n");
    write("    sol_runtime_init();\n");
}

void write_footer() {
    write("    sol_runtime_destroy();\n");
    write("    return 0;\n");
    write("}\n");
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
    write("    SolList list_%i = sol_list_create(%s);\n", list_id, list->object_mode ? "true" : "false");
    write("    list_%i->freezeCount = %i;\n", list_id, list->freeze_count);
    SolCListNode node = list->first;
    for (; node != NULL; node = node->next) {
        SolCObject obj = node->obj;
        if (obj->type_id == SOLC_TYPE_LIST) {
            write_obj(obj);
            write("    sol_list_add_obj(list_%i, (SolObject) list_%i);\n", list_id, last_list_id);
        } else {
            write("    sol_list_add_obj(list_%i, (SolObject) ", list_id);
            write_obj(obj);
            write(");\n");
        }
    }
    last_list_id = list_id;
}

void write_token(SolCToken token) {
    // handle special cases
    // handle data types
    if (!strcmp(token->identifier, "true")) {
        write("sol_bool_create(true)");
        return;
    }
    if (!strcmp(token->identifier, "false")) {
        write("sol_bool_create(false)");
        return;
    }
    // otherwise write a token
    write("sol_token_create(\"%s\")", token->identifier);
}

void write_string(SolCString string) {
    write("sol_string_create(\"%s\")", string->value);
}

void write_number(SolCNumber number) {
    write("sol_num_create(%lf)", number->value);
}
