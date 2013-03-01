
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define cprint(...) fprintf(out, __VA_ARGS__)

static unsigned char* src;
static off_t src_size;
static FILE* out;

void cprint_header();
void cprint_footer();
void cprint_data();

void solc_generate_c(unsigned char* source, off_t source_size, FILE* output) {
    src = source;
    src_size = source_size;
    out = output;
    
    cprint_header();
    cprint_data();
    cprint_footer();
}

void cprint_header() {
    cprint("#include <sol/runtime.h>\n\n");
    cprint("unsigned char data[] = {");
}

void cprint_footer() {
    cprint("};\n\n");
    cprint("int main(int argc, char** argv) {\n");
    cprint("    sol_runtime_init();\n");
    cprint("    sol_runtime_execute(data);\n");
    cprint("    sol_runtime_destroy();\n");
    cprint("    return 0;\n");
    cprint("}\n");
}

void cprint_data() {
    int ch;
    int i = 0;
    for (; (ch = *src), --src_size; src++) {
        if (i++ % 12 == 0)
            fputs("\n  ", out);
        fprintf(out, "0x%02X,", ch);
    }
    fputc('\n', out);
}
