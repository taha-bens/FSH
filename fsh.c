#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "pwd.h"

#define RESET_COLOR "\033[00m"
#define RED_COLOR "\033[91m"
#define MAX_LENGTH_PROMT 30 + sizeof(RED_COLOR) + sizeof(RESET_COLOR)
#define SIZE_PWDBUF 1024

// Split l'entrée en plusieurs chaines (malloc)
char **split(char *src, char delimiter){
    // On compte l'espace requis pour le malloc
    int compteur = 0;
    for (int i = 0; src[i] !='\0'; i++) {
        if (src[i] != delimiter) {
            compteur++;
        }
    }
    printf("allocation %d\n",compteur);
    return NULL;
}

char** str_split(char* a_str, const char a_delim)
{
    char** result = 0;
    size_t count = 0;
    char* tmp = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Compte le nombre de tokens. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Ajoute un token pour la dernière chaine. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Ajoute un token pour le cas ou la chaine est vide */
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

void free_split(char **splited){
    for (int i = 0; *(splited + i); i++){
        free(splited[i]);
    }
    free(splited);
}

int execute_pwd() {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return -1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // Processus enfant cad pwd
        close(pipefd[0]); // Fermer le côté lecture du pipe
        dup2(pipefd[1], STDOUT_FILENO); // Rediriger stdout vers le pipe
        close(pipefd[1]); // Fermer le côté écriture du pipe

        execlp("pwd", "pwd", (char *)NULL);
        // Si execlp échoue alors on affiche une erreur
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {
        // Processus parent cad le shell
        close(pipefd[1]); // Fermer le côté écriture du pipe

        char buffer[SIZE_PWDBUF]; // Juste une taille suffisante pour le chemin absolu
        ssize_t count = read(pipefd[0], buffer, SIZE_PWDBUF - 1);
        if (count == -1) {
            perror("read");
            close(pipefd[0]);
            return -1;
        }
        buffer[count] = '\0'; // Ne jamais oublier ce fichu caractère null

        close(pipefd[0]);

        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("%s", buffer); // Afficher le chemin absolu
            return WEXITSTATUS(status);
        } else {
            return -1;
        }
    }
}

int main() {
    int last_return_value = 0;
    char formated_promt[MAX_LENGTH_PROMT];
    char **splited;
    // Rediriger stdout vers stderr pour les tests
    dup2(STDERR_FILENO, STDOUT_FILENO);
    while (1) {
        char *current_dir = nom_repertoire_courant();
        if (current_dir == NULL) {
            perror("get_current_dir_name");
            return 1;
        }
        if (last_return_value == 0) {
            snprintf(formated_promt,MAX_LENGTH_PROMT, "[%d]%s$ ",last_return_value, current_dir);
        }
        else {
            snprintf(formated_promt,MAX_LENGTH_PROMT, "[%s%d%s]%s$ ", RED_COLOR, last_return_value, RESET_COLOR, current_dir);
        }
        free(current_dir);
        char *ligne = readline(formated_promt);
        // Ligne vide
        if (!ligne) {
            free(ligne);
            continue;
        }
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

        // Simplement afficher les arguments pour tester
        for (int i=0; splited[i]; i++) {
            printf("%s\n",splited[i]);
        }

        // Cases pour le lancement des commandes
        if (strcmp(splited[0], "exit") == 0) {
            // Si on a un argument alors c'est la valeur de retour
            if (splited[1]) {
                last_return_value = atoi(splited[1]); 
            }
            free(ligne);
            goto fin;
        }
        else if (strcmp(splited[0], "pwd") == 0) {
            last_return_value = execute_pwd();
        }
        

        free(ligne);
        free_split(splited);
    }

    // On libère la mémoire et on quitte
    fin:
    free_split(splited);
    exit(last_return_value);

}