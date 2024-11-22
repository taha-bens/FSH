#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "headers/ftype.h"

#define PATH_MAX 4096

int execute_for(char **args)
{
    int arg_count = 0;
    while (args[arg_count] != NULL)
    {
        arg_count++;
    }
    if (arg_count < 6 || strcmp(args[0], "for") != 0 || strcmp(args[2], "in") != 0 || strcmp(args[4], "{") != 0 || strcmp(args[arg_count - 1], "}") != 0)
    {
        fprintf(stderr, "for: Invalid syntax\n");
        return 1;
    }

    char *var_name = args[1];
    char *dir_name = args[3];
    int show_all = 0;
    int recursive = 0;
    char *ext = NULL;
    char *type = NULL;
    int max_files = -1;

    // OPTIONS j'ai mis la pour fill en attendant
    for (int i = 6; i < arg_count - 1; i++)
    {
        if (strcmp(args[i], "-A") == 0)
        {
            show_all = 1;
        }
        else if (strcmp(args[i], "-r") == 0)
        {
            recursive = 1;
        }
        else if (strcmp(args[i], "-e") == 0 && i + 1 < arg_count - 1)
        {
            ext = args[++i];
        }
        else if (strcmp(args[i], "-t") == 0 && i + 1 < arg_count - 1)
        {
            type = args[++i];
        }
        else if (strcmp(args[i], "-p") == 0 && i + 1 < arg_count - 1)
        {
            max_files = atoi(args[++i]);
        }
    }

    DIR *dir = opendir(dir_name);
    if (dir == NULL)
    {
        perror("opendir");
        return 1;
    }

    struct dirent *entry;
    int file_count = 0;
    int last_return_value = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        if (!show_all && entry->d_name[0] == '.')
        {
            continue; // Fichiers cachés
        }

        if (ext != NULL)
        {
            char *dot = strrchr(entry->d_name, '.');
            if (dot == NULL || strcmp(dot + 1, ext) != 0)
            {
                continue; // Fichiers qui n'ont pas l'extension spécifiée
            }
        }

        if (type != NULL)
        {
            struct stat entry_stat;
            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_name, entry->d_name);
            if (stat(full_path, &entry_stat) == -1)
            {
                perror("stat");
                closedir(dir);
                return 1;
            }

            if ((strcmp(type, "f") == 0 && !S_ISREG(entry_stat.st_mode)) ||
                (strcmp(type, "d") == 0 && !S_ISDIR(entry_stat.st_mode)) ||
                (strcmp(type, "l") == 0 && !S_ISLNK(entry_stat.st_mode)) ||
                (strcmp(type, "p") == 0 && !S_ISFIFO(entry_stat.st_mode)))
            {
                continue; // Mauvais type de fichier
            }
        }

        if (max_files != -1 && file_count >= max_files)
        {
            break; // Limite de fichiers atteinte
        }

        // Construire les arguments de la commande
        int cmd_arg_count = arg_count - 6; // Exclure "for", var_name, "in", dir_name, "{", "}"
        char **new_args = malloc((cmd_arg_count + 2) * sizeof(char *)); // +2 pour le fichier et NULL
        if (new_args == NULL)
        {
            perror("malloc");
            closedir(dir);
            return 1;
        }

        int j = 0;
        for (int i = 5; i < arg_count - 1; i++)
        {
            if (strcmp(args[i], var_name) == 0 || (args[i][0] == '$' && strcmp(args[i] + 1, var_name) == 0))
            {
                new_args[j++] = entry->d_name;
            }
            else
            {
                new_args[j++] = args[i];
            }
        }
        new_args[j] = NULL;

        // Afficher l'argument pour le débogage
        fprintf(stderr, "%s\n", new_args[1]);

        // Commande interne
        if (strcmp(new_args[0], "ftype") == 0)
        {
            if (ftype(new_args[1], dir_name) != 0)
            {
                free(new_args);
                closedir(dir);
                return 1;
            }
            free(new_args);
            continue;
        }

        // Commande externe
        pid_t pid = fork();
        if (pid == 0)
        {
            // Enfant : exécuter la commande
            execvp(new_args[0], new_args);
            perror("execvp");
            exit(EXIT_FAILURE);
        }
        else if (pid < 0)
        {
            perror("fork");
            free(new_args);
            closedir(dir);
            return 1;
        }
        else
        {
            // Parent : attendre l'enfant
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status))
            {
                last_return_value = WEXITSTATUS(status);
                if (last_return_value != 0)
                {
                    free(new_args);
                    closedir(dir);
                    return 1;
                }
            }
            else
            {
                free(new_args);
                closedir(dir);
                return 1;
            }
        }

        free(new_args);
        file_count++;
    }

    closedir(dir);
    return 0;
}