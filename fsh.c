#include <stdlib.h>
#include <stdio.h>

#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LENGTH_PROMT 30

int main() {
    int last_return_value = 0;
    char formated_promt[MAX_LENGTH_PROMT];
    while (1) {
        snprintf(formated_promt,MAX_LENGTH_PROMT, "[%d]LeDirEstIci$ ",last_return_value);
        char *ligne = readline(formated_promt);
    
        last_return_value = 0;
        for (int i =0; i<1024;i++) {
            if (ligne[i] == 0) {
                break;
            }
            printf("%c\n",ligne[i]);
            last_return_value++;
        }
        add_history(ligne);
        free(ligne);
    }

}