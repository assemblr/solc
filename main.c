/* 
 * File:   main.c
 * Author: Jake
 *
 * Created on November 20, 2012, 6:42 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "solc.h"
#include "solgen.h"

/*
 * 
 */
int main(int argc, char** argv) {
    if (argc != 2) {
        printf("usage:  solc filename\n");
    } else {
        FILE* in = fopen(argv[1], "r");
        if (in == NULL) {
            fprintf(stderr, "File '%s' could not be read.\n", argv[1]);
            return -1;
        }
        
        // get file size
        struct stat st;
        if (stat(argv[1], &st) != 0) {
            fprintf(stderr, "Error determining size of file.\n");
            return -1;
        }
        off_t in_size = st.st_size;
        
        // read file contents
        char* in_contents = malloc(in_size + 1);
        if (fread(in_contents, in_size, 1, in) != 1) {
            fprintf(stderr, "Error reading file.\n");
            return -1;
        }
        in_contents[in_size] = '\0';
        fclose(in);
        
        char* out_name = file_modify_extension(file_strip_path(argv[1]), "c");
        FILE* out = fopen(out_name, "w");
        free(out_name);
        if (out == NULL) {
            fprintf(stderr, "File '%s' could not be written.\n", out_name);
            return -1;
        }
        
        // process file
        // printf("File successfully read:\n%s", in_contents);
        fprintf(out, "// generated from %s\n\n", file_strip_path(argv[1]));
        solc_compile(in_contents, out);
        
        // free original string
        free(in_contents);
    }
    return (EXIT_SUCCESS);
}

