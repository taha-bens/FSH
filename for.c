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

// Structure pour suivre les arguments et leur type d'allocation
typedef struct
{
    char *arg;
    int dynamic; // 1 si l'argument est alloué dynamiquement, 0 sinon
} Arg;

// Fonction pour libérer les arguments
void free_args(Arg *args, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (args[i].dynamic)
        {
            free(args[i].arg);
        }
    }
    free(args);
}

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

    // OPTIONS
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

    char start_dir[PATH_MAX];
    if (getcwd(start_dir, sizeof(start_dir)) == NULL)
    {
        perror("getcwd");
        return 1;
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
            continue;

        if (ext != NULL)
        {
            char *dot = strrchr(entry->d_name, '.');
            if (dot == NULL || strcmp(dot + 1, ext) != 0)
                continue;
        }

        if (type != NULL)
        {
            struct stat entry_stat;
            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", args[3], entry->d_name);
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
                continue;
            }
        }

        if (max_files != -1 && file_count >= max_files)
            break;

        // Construire le chemin absolu du fichier depuis start_dir
        char file_path[PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", args[3], entry->d_name);

        // Construire les arguments de la commande
        int cmd_arg_count = arg_count - 6;
        Arg *new_args = malloc((cmd_arg_count + 1) * sizeof(Arg));
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
                new_args[j].arg = strdup(file_path); // Allocation dynamique
                new_args[j].dynamic = 1;
            }
            else
            {
                new_args[j].arg = args[i];
                new_args[j].dynamic = 0;
            }
            j++;
        }
        new_args[j].arg = NULL; // Marquer la fin des arguments
        new_args[j].dynamic = 0;

        // Commande interne
        if (strcmp(new_args[0].arg, "ftype") == 0)
        {
            // Supprimer la partie inutile de new_args[1] puisque on execute ftype à partir de dir_name
            char *file_name = strrchr(new_args[1].arg, '/');
            if (file_name == NULL)
            {
                file_name = new_args[1].arg;
            }
            else
            {
                file_name++;
            }
            if (ftype(file_name, dir_name) != 0)
            {
                free_args(new_args, j);
                closedir(dir);
                return 1;
            }
            free_args(new_args, j);
            continue;
        }

        // Convertir les arguments pour execvp
        char **exec_args = malloc((j + 1) * sizeof(char *));
        for (int k = 0; k < j; k++)
        {
            exec_args[k] = new_args[k].arg;
        }
        exec_args[j] = NULL;

        // Commande externe
        pid_t pid = fork();
        if (pid == 0)
        {
            // Enfant : exécuter la commande dans le répertoire de départ
            if (chdir(start_dir) == -1)
            {
                perror("chdir");
                free_args(new_args, j);
                free(exec_args);
                closedir(dir);
                exit(EXIT_FAILURE);
            }

            execvp(exec_args[0], exec_args);
            perror("execvp");
            free_args(new_args, j);
            free(exec_args);
            closedir(dir);
            exit(EXIT_FAILURE);
        }
        else if (pid < 0)
        {
            perror("fork");
            free_args(new_args, j);
            free(exec_args);
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
                    free_args(new_args, j);
                    free(exec_args);
                    closedir(dir);
                    return 1;
                }
            }
            else
            {
                free_args(new_args, j);
                free(exec_args);
                closedir(dir);
                return 1;
            }
        }

        free_args(new_args, j);
        free(exec_args);
        file_count++;
    }

    closedir(dir);
    return 0;
}