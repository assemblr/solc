
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

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
    cprint("    SolList arguments = (SolList) sol_obj_retain((SolObject) sol_list_create(false));");
    cprint("    for (int i = 0; i < argc; i++) { sol_list_add_obj(arguments, (SolObject) sol_string_create(argv[i])); }");
    cprint("    sol_token_register(\"arguments\", (SolObject) arguments);");
    cprint("    sol_runtime_execute(data);\n");
    cprint("    sol_obj_release((SolObject) arguments);");
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
