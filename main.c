/* 
 * File:   main.c
 * Author: Jake
 *
 * Created on November 20, 2012, 6:42 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "solc.h"
#include "solgen.h"

/*
 * 
 */
int main(int argc, char** argv) {
    // parse command-line flags
    char* filename;
    bool flag_b, flag_c;
    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];
        if (*arg == '-') {
            if (arg[1] == '-') {
                fprintf(stderr, "Unrecognized flag %s.\n", arg);
            } else {
                while (*++arg != '\0') {
                    switch (*arg) {
                        case 'b':
                            flag_b = true;
                            break;
                        case 'c':
                            flag_c = true;
                            break;
                        default:
                            fprintf(stderr, "Unrecognized flag -%c.\n", *arg);
                            break;
                    }
                }
            }
        } else {
            filename = arg;
        }
    }
    
    // handle invalid input
    if (argc == 0 || !filename) {
        printf("usage:  solc filename\n");
        return EXIT_FAILURE;
    }
    if (flag_b && flag_c) {
        fprintf(stderr, "Invalid flag combination: -c and -b.\n");
        return EXIT_FAILURE;
    }
    
    // begin compilation
    FILE* in = fopen(filename, "r");
    if (in == NULL) {
        fprintf(stderr, "File '%s' could not be read.\n", filename);
        return -1;
    }
    
    // get file size
    struct stat st;
    if (stat(filename, &st) != 0) {
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
    
    char* bin_out_name = file_modify_extension(file_strip_path(filename), "solbin");
    FILE* bin_out = fopen(bin_out_name, "w+");
    char* out_name = file_modify_extension(file_strip_path(filename), "c");
    FILE* out = flag_b ? NULL : fopen(out_name, "w");
    if (bin_out == NULL) {
        fprintf(stderr, "File '%s' could not be written.\n", bin_out_name);
        return EXIT_FAILURE;
    }
    if (!flag_b && out == NULL) {
        fprintf(stderr, "File '%s' could not be written.\n", out_name);
        return EXIT_FAILURE;
    }
    
    // process file
    if (out) fprintf(out, "// generated from %s\n\n", file_strip_path(filename));
    solc_compile(in_contents, bin_out, out);
    fclose(bin_out);
    if (out) fclose(out);
    
    // remove bin file
    if (flag_c) {
        remove(bin_out_name);
    }
    
    free(bin_out_name);
    free(out_name);
    
    // free original string
    free(in_contents);
    return EXIT_SUCCESS;
}

