
#include <string.h>
#include <stdlib.h>

#include "solcobj.h"


SolCList solc_create_list(bool object_mode) {
    static const solc_list DEFAULT = {
        { SOLC_TYPE_LIST }, false, NULL, NULL, 0
    };
    SolCList list = memcpy(malloc(sizeof(solc_list)), &DEFAULT, sizeof(solc_list));
    list->object_mode = object_mode;
    return list;
}

SolCToken solc_create_token(char* identifier) {
    static const solc_token DEFAULT = {
        { SOLC_TYPE_TOKEN }, NULL
    };
    SolCToken token = memcpy(malloc(sizeof(solc_token)), &DEFAULT, sizeof(solc_token));
    token->identifier = identifier;
    return token;
}

SolCString solc_create_string(char* value) {
    static const solc_str DEFAULT = {
        { SOLC_TYPE_STR }, NULL
    };
    SolCString str = memcpy(malloc(sizeof(solc_str)), &DEFAULT, sizeof(solc_str));
    str->value = value;
    return str;
}

SolCNumber solc_create_number(double value) {
    static const solc_num DEFAULT = {
        { SOLC_TYPE_NUM }, 0
    };
    SolCNumber num = memcpy(malloc(sizeof(solc_num)), &DEFAULT, sizeof(solc_num));
    num->value = value;
    return num;
}

void solc_list_add(SolCList list, SolCObject obj) {
    SolCListNode node = malloc(sizeof(*node));
    node->obj = obj;
    if (list->first == NULL || list->last == NULL) {
        list->first = node;
        list->last = node;
    } else {
        list->last->next = node;
        node->prev = list->last;
        list->last = node;
    }
}
