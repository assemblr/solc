/* 
 * File:   solc.h
 * Author: Jake
 *
 * Created on November 20, 2012, 7:10 PM
 */

#ifndef SOLC_H
#define	SOLC_H

#include <stdlib.h>
#include <stdio.h>
#include <sol/runtime.h>

SolList solc_parse(char* source);
SolList solc_parse_f(FILE* source);

unsigned char* solc_emit(SolList source, off_t* size);

unsigned char* solc_compile(char* source, off_t* size);
unsigned char* solc_compile_f(FILE* source, off_t* size);

#endif	/* SOLC_H */

