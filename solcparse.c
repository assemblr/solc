
#include "solc.h"

#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <pcre.h>

static char* src;

SolObject read_object();
SolObject read_list(bool object_mode, bool frozen);
SolObject read_object_literal(char* parent);
SolObject read_function_literal(SolList param_list);
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
    SolToken obj_literal_parent;
    SolList func_literal_params;
    while (*src != '\0') {
        bool func_modifier_active = func_modifier;
        bool obj_modifier_active = obj_modifier;
        SolToken obj_literal_parent_active = obj_literal_parent;
        SolList func_literal_params_active = func_literal_params;
        func_modifier = obj_modifier = false;
        obj_literal_parent = NULL;
        func_literal_params = NULL;
        
        // skip whitespace chars
        if (isspace(*src)) {
            src++;
            continue;
        }
        
        // process number literals
        if (isdigit(*src) || (*src == '-' && isdigit(*(src + 1)))) {
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
                if (func_modifier_active) {
                    SolList list = (SolList) read_list(false, true);
                    if (*(src) == '{') {
                        func_modifier = true;
                        func_literal_params = list;
                        continue;
                    }
                    fprintf(stderr, "solc: error while parsing source: function modifier found before frozen list\n");
                    exit(EXIT_FAILURE);
                }
                return (SolObject) read_list(false, true);
            case '[': // STATEMENTS
                if (func_modifier_active) {
                    SolList func_list = (SolList) sol_obj_retain((SolObject) sol_list_create(false));
                    sol_list_add_obj(func_list, (SolObject) sol_token_create("^"));
                    SolList param_list = sol_list_create(false);
                    sol_list_add_obj(func_list, (SolObject) param_list);
                    sol_list_add_obj(param_list, (SolObject) sol_token_create("freeze"));
                    sol_list_add_obj(param_list, (SolObject) sol_list_create(false));
                    sol_list_add_obj(func_list, (SolObject) read_list(obj_modifier_active, false));
                    return (SolObject) func_list;
                }
                return (SolObject) read_list(obj_modifier_active, false);
            case '{': // OBJECT LITERALS
                if (obj_modifier_active) {
                    if (obj_literal_parent_active) {
                        SolObject literal = (SolObject) read_object_literal(obj_literal_parent_active->identifier);
                        sol_obj_release((SolObject) obj_literal_parent_active);
                        return literal;
                    }
                    return (SolObject) read_object_literal("Object");
                }
                if (func_modifier_active) {
                    return (SolObject) read_function_literal(func_literal_params_active ? func_literal_params_active : (SolList) nil);
                }
                return (SolObject) read_object_literal(NULL);
            case '^': // FUNCTION SHORTHAND
                if (*(src + 1) == '[' || *(src + 1) == '(' || *(src + 1) == '{') {
                    func_modifier = true;
                    src++;
                    continue;
                }
                return read_token();
            case '@': // OBJECT MODE STATEMENTS
                if (*(src + 1) == '[' || *(src + 1) == '{') {
                    obj_modifier = true;
                    src++;
                    continue;
                }
                char* lookahead = src + 1;
                for (; !is_delimiter(*lookahead); lookahead++) {}
                if (*lookahead == '{') {
                    obj_modifier = true;
                    src++;
                    obj_literal_parent = (SolToken) read_token();
                    if (obj_literal_parent->super.type_id != TYPE_SOL_TOKEN) {
                        fprintf(stderr, "solc: error while parsing source: object literal parent was not a token\n");
                        exit(EXIT_FAILURE);
                    }
                    continue;
                }
                return read_token();
            case ':': // FROZEN OBJECTS
                src++;
                SolObject obj = read_object();
                SolList list = (SolList) sol_obj_retain((SolObject) sol_list_create(false));
                sol_list_add_obj(list, (SolObject) sol_token_create("freeze"));
                sol_list_add_obj(list, obj);
                sol_obj_release(obj);
                return (SolObject) list;
            default:
                return read_token();
        }
    }
    return NULL;
}

SolObject read_list(bool object_mode, bool frozen) {
    // advance past open delimiter
    src++;
    SolList list = (SolList) sol_obj_retain((SolObject) sol_list_create(object_mode));
    while (*src != '\0') {
        // skip whitespace characters
        if (isspace(*src)) {
            src++;
            continue;
        }
        // handle list termination
        if (*src == (frozen ? ')' : ']')) {
            src++;
            if (frozen) {
                SolList frozen = (SolList) sol_obj_retain((SolObject) sol_list_create(false));
                sol_list_add_obj(frozen, (SolObject) sol_token_create("freeze"));
                sol_list_add_obj(frozen, (SolObject) list);
                sol_obj_release((SolObject) list);
                return (SolObject) frozen;
            } else {
                return (SolObject) list;
            }
        }
        // add object
        SolObject object = read_object();
        sol_list_add_obj(list, object);
        sol_obj_release(object);
    }
    fprintf(stderr, "solc: error while parsing source: encountered unclosed list\n");
    exit(EXIT_FAILURE);
}

SolObject read_object_literal(char* parent) {
    // advance past open delimiter
    src++;
    // create raw parameter
    SolList raw_list = (SolList) sol_obj_retain((SolObject) sol_list_create(true));
    sol_list_add_obj(raw_list, (SolObject) sol_token_create("Object"));
    sol_list_add_obj(raw_list, (SolObject) sol_token_create("create"));
    // read literal data
    while (*src != '\0') {
        // skip whitespace characters
        if (isspace(*src)) {
            src++;
            continue;
        }
        // handle literal termination
        if (*src == '}') {
            src++;
            if (parent) {
                SolList result_list = (SolList) sol_obj_retain((SolObject) sol_list_create(true));
                sol_list_add_obj(result_list, (SolObject) sol_token_create(parent));
                sol_list_add_obj(result_list, (SolObject) sol_token_create("clone"));
                sol_list_add_obj(result_list, (SolObject) raw_list);
                sol_obj_release((SolObject) raw_list);
                return (SolObject) result_list;
            } else {
                return (SolObject) raw_list;
            }
        }
        // read key/value
        SolObject key = read_object();
        SolObject value = read_object();
        sol_list_add_obj(raw_list, key);
        sol_list_add_obj(raw_list, value);
        sol_obj_release(key);
        sol_obj_release(value);
    }
    fprintf(stderr, "solc: error while parsing source: encountered unclosed object literal\n");
    exit(EXIT_FAILURE);
}

SolObject read_function_literal(SolList param_list) {
    // advance past open delimiter
    src++;
    SolList statement_list = (SolList) sol_obj_retain((SolObject) sol_list_create(false));
    // read literal data
    while (*src != '\0') {
        // skip whitespace characters
        if (isspace(*src)) {
            src++;
            continue;
        }
        // handle literal termination
        if (*src == '}') {
            src++;
            SolList result_list = (SolList) sol_obj_retain((SolObject) sol_list_create(false));
            sol_list_add_obj(result_list, (SolObject) sol_token_create("^"));
            sol_list_add_obj(result_list, (SolObject) param_list);
            sol_obj_release((SolObject) param_list);
            SOL_LIST_ITR(statement_list) {
                sol_list_add_obj(result_list, statement_list->current->value);
            }
            return (SolObject) result_list;
        }
        // read key/value
        SolObject statement = read_object();
        sol_list_add_obj(statement_list, statement);
        sol_obj_release(statement);
    }
    fprintf(stderr, "solc: error while parsing source: encountered unclosed object literal\n");
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
    
    // handle object '.'/'@' getter shorthand
    static pcre* regex = NULL;
    if (regex == NULL) {
        const char* error_msg;
        int error_pos;
        regex = pcre_compile("[.@]?[^.@]+[.@]*", 0, &error_msg, &error_pos, NULL);
        if (regex == NULL) {
            printf("regex compilation failed at offset %d: %s\n", error_pos, error_msg);
        }
    }
    int osize = 15;
    int* ovector = malloc(sizeof(*ovector) * osize);
    int matches;
    int offset = 0;
    int len = strlen(result);
    SolObject result_object = NULL;
    while (matches = pcre_exec(regex, NULL, result, len, offset, 0, ovector, osize), ovector[1] > ovector[0]) {
        if (matches == 0) {
            osize *= 2;
            realloc(ovector, osize);
            continue;
        }
        char* match;
        int match_len = pcre_get_substring(result, ovector, matches, 0, (const char**) &match);
        offset = ovector[1];
        char final = match[match_len - 1];
        if (final == '.' || final == '@') {
            SolList list = sol_list_create(true);
            match[match_len - 1] = '\0';
            if (!result_object) {
                sol_list_add_obj(list, (SolObject) sol_token_create(match));
            } else {
                SolList current_list = (SolList) result_object;
                SolList frozen = (SolList) sol_obj_retain((SolObject) sol_list_create(false));
                sol_list_add_obj(frozen, (SolObject) sol_token_create("freeze"));
                sol_list_add_obj(frozen, (SolObject) sol_token_create(match));
                sol_list_add_obj(current_list, (SolObject) frozen);
                sol_obj_release((SolObject) frozen);
                sol_list_add_obj(list, (SolObject) current_list);
            }
            if (final == '.') {
                sol_list_add_obj(list, (SolObject) sol_token_create("get"));
            } else {
                sol_list_add_obj(list, (SolObject) sol_token_create("@get"));
            }
            result_object = (SolObject) list;
        } else {
            if (!result_object) {
                result_object = (SolObject) sol_token_create(match);
            } else {
                SolList frozen = (SolList) sol_obj_retain((SolObject) sol_list_create(false));
                sol_list_add_obj(frozen, (SolObject) sol_token_create("freeze"));
                sol_list_add_obj(frozen, (SolObject) sol_token_create(match));
                sol_list_add_obj((SolList) result_object, (SolObject) frozen);
                sol_obj_release((SolObject) frozen);
            }
            pcre_free(match);
            break;
        }
        pcre_free(match);
    }
    
    free(ovector);
    free(result);
    return sol_obj_retain(result_object);
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
                    size_t result_len = buff_pos - buff;
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
