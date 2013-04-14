
#include "config.h"

#include "solc.h"

#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#if defined(HAVE_PCRE_H)
#include <pcre.h>
#else
#include <regex.h>
#endif

static char* src;

SolObject read_object();
SolList read_list(bool object_mode, int freeze, bool bracketed);
SolcObjectLiteral read_object_literal(char* parent);
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
    while (*src != '\0') {
        bool func_modifier_active = func_modifier;
        bool obj_modifier_active = obj_modifier;
        SolToken obj_literal_parent_active = obj_literal_parent;
        func_modifier = obj_modifier = false;
        obj_literal_parent = NULL;
        
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
                return (SolObject) read_list(false, 0, 0);
            case '[': // STATEMENTS
                if (func_modifier_active) {
                    SolList func_list = (SolList) sol_obj_retain((SolObject) sol_list_create(false));
                    func_list->freezeCount = -1;
                    sol_list_add_obj(func_list, (SolObject) sol_token_create("^"));
                    sol_list_add_obj(func_list, (SolObject) sol_list_create(false));
                    sol_list_add_obj(func_list, (SolObject) read_list(obj_modifier_active, -1, true));
                    return (SolObject) func_list;
                }
                return (SolObject) read_list(obj_modifier_active, -1, true);
            case '{': // OBJECT LITERALS
                if (obj_modifier_active) {
                    if (obj_literal_parent_active) {
                        SolObject literal = (SolObject) read_object_literal(obj_literal_parent_active->identifier);
                        sol_obj_release((SolObject) obj_literal_parent_active);
                        return literal;
                    }
                    return (SolObject) read_object_literal("Object");
                }
                return (SolObject) read_object_literal(NULL);
            case '^': // FUNCTION SHORTHAND
                if (*(src + 1) == '[') {
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
            default:
                return read_token();
        }
    }
    return NULL;
}

SolList read_list(bool object_mode, int freeze, bool bracketed) {
    // advance past open delimiter
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

SolcObjectLiteral read_object_literal(char* parent) {
    // advance past open delimiter
    src++;
    // create special object
    SolcObjectLiteral object = sol_obj_clone_type(Object, &(struct solc_object_literal_raw){
        strdup(parent),
        sol_obj_create_raw()
    }, sizeof(*object));
    sol_obj_retain((SolObject) object);
    object->super.type_id = TYPE_SOLC_OBJ_LITERAL;
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
            return (SolcObjectLiteral) object;
        }
        // read key/value
        SolObject key = read_object();
        if (key->type_id != TYPE_SOL_TOKEN) {
            fprintf(stderr, "solc: error while parsing source: object literal key was not a token\n");
            exit(EXIT_FAILURE);
        }
        SolObject value = read_object();
        sol_obj_set_prop(object->object, ((SolToken) key)->identifier, value);
        sol_obj_release(key);
        sol_obj_release(value);
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
#if defined(HAVE_PCRE_H)
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
            list->freezeCount = -1;
            match[match_len - 1] = '\0';
            if (!result_object) {
                sol_list_add_obj(list, (SolObject) sol_token_create(match));
            } else {
                SolList current_list = (SolList) result_object;
                sol_list_add_obj(current_list, (SolObject) sol_token_create(match));
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
                sol_list_add_obj((SolList) result_object, (SolObject) sol_token_create(match));
            }
            pcre_free(match);
            break;
        }
        pcre_free(match);
    }
    
    free(ovector);
    free(result);
    return sol_obj_retain(result_object);
#else
    regex_t regex;
    regcomp(&regex, "[.@]\\{0,1\\}[^.@][^.@]*[.@]*", 0);
    char* result_offset = result;
    regmatch_t matches[1];
    SolObject result_object = NULL;
    while (regexec(&regex, result_offset, 1, matches, 0) == 0 && matches->rm_eo > matches->rm_so) {
        char* match = malloc(matches->rm_eo + 1);
        memcpy(match, result_offset, matches->rm_eo);
        match[matches->rm_eo] = '\0';
        char final = match[matches->rm_eo - 1];
        if (final == '.' || final == '@') {
            SolList list = sol_list_create(true);
            list->freezeCount = -1;
            match[matches->rm_eo - 1] = '\0';
            if (!result_object) {
                sol_list_add_obj(list, (SolObject) sol_token_create(match));
            } else {
                SolList current_list = (SolList) result_object;
                sol_list_add_obj(current_list, (SolObject) sol_token_create(match));
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
                sol_list_add_obj((SolList) result_object, (SolObject) sol_token_create(match));
            }
            free(match);
            break;
        }
        free(match);
        result_offset += matches->rm_eo;
    }
    free(result);
    return sol_obj_retain(result_object);
#endif
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
