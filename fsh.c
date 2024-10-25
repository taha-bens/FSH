#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <readline/readline.h>
#include <readline/history.h>

#define RESET_COLOR "\033[00m"
#define RED_COLOR "\033[91m"
#define MAX_LENGTH_PROMT 30 + sizeof(RED_COLOR) + sizeof(RESET_COLOR)

// split l'entrée en plusieurs chaines (malloc)
char **split(char *src, char delimiter){
    // on compte l'espace requis pour le malloc
    int compteur = 0;
    for (int i = 0; src[i] !='\0'; i++) {
        if (src[i] != delimiter) {
            compteur++;
        }
    }
    printf("allocation %d\n",compteur);
    
}

char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

int main() {
    int last_return_value = 0;
    char formated_promt[MAX_LENGTH_PROMT];
    char *args;
    char **splited;
    while (1) {
        if (last_return_value == 0) {
            snprintf(formated_promt,MAX_LENGTH_PROMT, "[%d]LeDirEstIci$ ",last_return_value);
        }
        else {
            snprintf(formated_promt,MAX_LENGTH_PROMT, "[%s%d%s]LeDirEstIci$ ",RED_COLOR,last_return_value,RESET_COLOR);
        }
        char *ligne = readline(formated_promt);
        if (ligne[0] == 0) {
            last_return_value = 0;
            free(ligne);
            continue;
        }
        add_history(ligne);
        //splited = split(ligne,' ');
        splited = str_split(ligne, ' ');
        // if ((args = strtok(ligne, " "))) {
        //     printf("%s\n",args);
        //     args = strtok(NULL, " ");
        //     printf("%s\n",args);
        // }

        //cases pour le lancement des commandes
        if (strcmp(splited[0], "exit") == 0) {
            free(ligne);
            exit(0);
        }



        for (int i=0; splited[i]; i++) {
            printf("%s\n",splited[i]);
        }
        puts("\n");

        last_return_value = 0;
        for (int i =0; i<1024;i++) {
            if (ligne[i] == 0) {
                printf("\n");
                break;
            }
            printf("%c",ligne[i]);
            last_return_value++;
        }
        free(ligne);
    }

}