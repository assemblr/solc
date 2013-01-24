
#include <string.h>
#include <stdlib.h>


char* file_strip_path(char* file) {
    char* slash = strrchr(file, '/');
    if (file == NULL) return slash;
    return slash + 1;
}

char* file_get_name(char* file) {
    const char* dot = strrchr(file, '.');
    if (dot == NULL) dot = file + strlen(file);
    char* name = malloc(dot - file + 1);
    memcpy(name, file, dot - file);
    name[dot - file + 1] = '\0';
    return name;
}

char* file_modify_extension(char* file, char* ext) {
    const char* dot = strrchr(file, '.');
    if (dot == NULL) dot = file + strlen(file);
    char* name = malloc(dot - file + strlen(ext) + 2);
    memcpy(name, file, dot - file);
    name[dot - file] = '.';
    memcpy(name + (dot - file) + 1, ext, strlen(ext) + 1);
    return name;
}
