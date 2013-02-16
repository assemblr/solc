/* 
 * File:   solcobj.h
 * Author: Jake
 *
 * Created on November 20, 2012, 8:35 PM
 */

#ifndef SOLCOBJ_H
#define	SOLCOBJ_H

#include <stdbool.h>
#include <stdint.h>

typedef enum solc_type {
    SOLC_TYPE_LIST,
    SOLC_TYPE_TOKEN,
    SOLC_TYPE_STR,
    SOLC_TYPE_NUM
} solc_type;

typedef struct solc_obj {
    solc_type type_id;
} solc_obj;
typedef solc_obj* SolCObject;

typedef struct solc_list_node {
    struct solc_list_node* next;
    struct solc_list_node* prev;
    SolCObject obj;
} solc_list_node;
typedef solc_list_node* SolCListNode;

typedef struct solc_list {
    solc_obj super;
    bool object_mode;
    SolCListNode first;
    SolCListNode last;
    uint32_t freeze_count;
} solc_list;
typedef solc_list* SolCList;

typedef struct solc_token {
    solc_obj super;
    char* identifier;
} solc_token;
typedef solc_token* SolCToken;

typedef struct solc_str {
    solc_obj super;
    char* value;
} solc_str;
typedef solc_str* SolCString;

typedef struct solc_num {
    solc_obj super;
    double value;
} solc_num;
typedef solc_num* SolCNumber;

SolCList solc_create_list(bool object_mode);
SolCToken solc_create_token(char* identifier);
SolCString solc_create_string(char* value);
SolCNumber solc_create_number(double value);

void solc_list_add(SolCList list, SolCObject obj);

#endif	/* SOLCOBJ_H */

