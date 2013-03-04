
#include "solc.h"

#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

static char* src;

SolObject read_object();
SolList read_list(bool object_mode, int freeze, bool bracketed);
SolObject read_token();
SolString read_string();
SolNumber read_number();

bool is_delimiter(char c);

SolList solc_parse_f(FILE* source) {
    // get file size
    struct stat st;
    if (fstat(fileno(source), &st)) {
        fprintf(stderr, "solc: error while parsing source: could not determine file size - cannot stat\n");
        exit(EXIT_FAILURE);
    }
    off_t size = st.st_size;
    
    // read file contents
    char* contents = malloc(size + 1);
    if (fread(contents, size, 1, source) != 1) {
        fprintf(stderr, "solc: error while parsing source: error while reading file\n");
        exit(EXIT_FAILURE);
    }
    contents[size] = '\0';
    
    // parse file
    SolList ret = solc_parse(contents);
    free(contents);
    return ret;
}

SolList solc_parse(char* source) {
    // set up static variables
    src = source;
    SolList out = (SolList) sol_obj_retain((SolObject) sol_list_create(false));
    
    // trim whitespace
    while (isspace(*src)) {
        src++;
    }
    char* end = src + strlen(src);
    while (isspace(*end)) {
        end--;
    }
    *end = '\0';
    
    // begin parsing
    while (*src != '\0') {
        SolObject object = read_object();
        if (object != NULL) {
            sol_list_add_obj(out, object);
        }
    }
    
    return out;
}

SolObject read_object() {
    // handle special flags
    bool func_modifier, obj_modifier;
    while (*src != '\0') {
        bool func_modifier_active = func_modifier;
        bool obj_modifier_active = obj_modifier;
        func_modifier = obj_modifier = false;
        
        // skip whitespace chars
        if (isspace(*src)) {
            src++;
            continue;
        }
        
        // process number literals
        if (isdigit(*src)) {
            return (SolObject) read_number();
        }
        
        // process other datatypes
        switch (*src) {
            case ';': // COMMENTS
                src = strchr(src, '\n') + 1;
                continue;
            case '"': // STRINGS
                return (SolObject) read_string();
            case '(': // LISTS
                return (SolObject) read_list(false, 0, 0);
            case '[': // STATEMENTS
                if (func_modifier_active) {
                    SolList func_list = (SolList) sol_obj_retain((SolObject) sol_list_create(false));
                    sol_list_add_obj(func_list, (SolObject) sol_token_create("^"));
                    sol_list_add_obj(func_list, (SolObject) sol_list_create(false));
                    sol_list_add_obj(func_list, (SolObject) read_list(obj_modifier_active, -1, true));
                    return (SolObject) func_list;
                }
                return (SolObject) read_list(obj_modifier_active, -1, true);
            case '^': // FUNCTION SHORTHAND
                if (*(src + 1) == '[') {
                    func_modifier = true;
                    src++;
                    continue;
                }
                return read_token();
            case '@': // OBJECT MODE STATEMENTS
                if (*(src + 1) == '[') {
                    obj_modifier = true;
                    src++;
                    continue;
                }
                return read_token();
            default:
                return read_token();
        }
    }
    return NULL;
}

SolList read_list(bool object_mode, int freeze, bool bracketed) {
    // advance past open delimited
    src++;
    SolList list = (SolList) sol_obj_retain((SolObject) sol_list_create(object_mode));
    list->freezeCount = freeze;
    while (*src != '\0') {
        // skip whitespace characters
        if (isspace(*src)) {
            src++;
            continue;
        }
        // handle list termination
        if (*src == (bracketed ? ']' : ')')) {
            src++;
            return list;
        }
        // add object
        SolObject object = read_object();
        sol_list_add_obj(list, object);
        sol_obj_release(object);
    }
    fprintf(stderr, "solc: error while parsing source: encountered unclosed list\n");
    exit(EXIT_FAILURE);
}

SolObject read_token() {
    // read token to buffer
    char* buff = malloc(256);
    size_t buff_size = 256;
    char* buff_pos = buff;
    for (; !is_delimiter(*src); src++) {
        if (buff_pos - buff == buff_size) {
            buff = realloc(buff, buff_size *= 2);
            buff_pos = buff + buff_size/2;
        }
        *buff_pos = *src;
        buff_pos++;
    }
    size_t result_len = buff_pos - buff;
    char* result = memcpy(malloc(result_len + 1), buff, result_len);
    result[result_len] = '\0';
    free(buff);
    
    // handle object '@' getter shorthand
    char* at_pos;
    if ((at_pos = strchr(result, '@')) && (at_pos - result > 0 || (at_pos = strchr(result + 1, '@')))) {
        SolList result_list = (SolList) sol_obj_retain((SolObject) sol_list_create(true));
        result_list->freezeCount = -1;
        char* token = strtok(result, "@");
        // handle tokens that start with '@'
        if (*result == '@') {
            char* new_token = malloc(sizeof(*token) * (strlen(token) + 1));
            new_token[0] = '@';
            new_token[1] = '\0';
            token = strcat(new_token, token);
        }
        sol_list_add_obj(result_list, (SolObject) sol_token_create(token));
        sol_list_add_obj(result_list, (SolObject) sol_token_create("get"));
        token = strtok(NULL, "@");
        sol_list_add_obj(result_list, (SolObject) sol_token_create(token));
        while ((token = strtok(NULL, "@")) != NULL) {
            SolList old_list = result_list;
            result_list = (SolList) sol_obj_retain((SolObject) sol_list_create(true));
            result_list->freezeCount = -1;
            sol_list_add_obj(result_list, (SolObject) old_list);
            sol_list_add_obj(result_list, (SolObject) sol_token_create("get"));
            sol_list_add_obj(result_list, (SolObject) sol_token_create(token));
            sol_obj_release((SolObject) old_list);
        }
        free(result);
        return (SolObject) result_list;
    } else {
        SolObject result_token = sol_obj_retain((SolObject) sol_token_create(result));
        free(result);
        return result_token;
    }
}

SolString read_string() {
    // advance past open quote
    src++;
    char* buff = malloc(256);
    size_t buff_size = 256;
    char* buff_pos = buff;
    bool escaped = false;
    while (true) {
        if (buff_pos - buff == buff_size) {
            buff = realloc(buff, buff_size *= 2);
            buff_pos = buff + buff_size/2;
        }
        // handle escape sequences
        if (escaped) {
            escaped = false;
            switch (*src) {
                case 'b':
                    *buff_pos = '\b';
                    break;
                case 't':
                    *buff_pos = '\t';
                    break;
                case 'n':
                    *buff_pos = '\n';
                    break;
                case 'f':
                    *buff_pos = '\f';
                    break;
                case 'r':
                    *buff_pos = '\r';
                    break;
                case '"':
                    *buff_pos = '"';
                    break;
                case '\\':
                    *buff_pos = '\\';
                    break;
                default:
                    printf("WARNING: Invalid escape sequence encountered: \\%c", *src);
            }
        } else {
            switch (*src) {
                case '"': {
                    size_t result_len = strlen(buff);
                    char* result = memcpy(malloc(result_len + 1), buff, result_len);
                    result[result_len] = '\0';
                    free(buff);
                    src++;
                    SolString result_str = (SolString) sol_obj_retain((SolObject) sol_string_create(result));
                    free(result);
                    return result_str;
                }
                case '\\':
                    escaped = true;
                    src++;
                    continue;
                default:
                    *buff_pos = *src;
            }
        }
        src++;
        buff_pos++;
    }
}

SolNumber read_number() {
    double value = 0;
    int offset = 0;
    sscanf(src, "%lf%n", &value, &offset);
    src += offset;
    return (SolNumber) sol_obj_retain((SolObject) sol_num_create(value));
}

bool is_delimiter(char c) {
    static char* delimiters = "()[]{}";
    return isspace(c) || strchr(delimiters, c) != NULL;
}
