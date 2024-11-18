#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "pwd.h"

#define RESET_COLOR "\033[00m"
#define RED_COLOR "\033[91m"
#define MAX_LENGTH_PROMT 30 + sizeof(RED_COLOR) + sizeof(RESET_COLOR)
#define SIZE_PWDBUF 1024
#define PATH_MAX 4096

// Split l'entrée en plusieurs chaines (malloc)
char **split(char *src, char delimiter)
{
  // On compte l'espace requis pour le malloc
  int compteur = 0;
  for (int i = 0; src[i] != '\0'; i++)
  {
    if (src[i] != delimiter)
    {
      compteur++;
    }
  }
  printf("allocation %d\n", compteur);
  return NULL;
}

char **str_split(char *a_str, const char a_delim)
{
  char **result = 0;
  size_t count = 0;
  char *tmp = a_str;
  char *last_comma = 0;
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

  result = malloc(sizeof(char *) * count);

  if (result)
  {
    size_t idx = 0;
    char *token = strtok(a_str, delim);

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

void free_split(char **splited)
{
  for (int i = 0; *(splited + i); i++)
  {
    free(splited[i]);
  }
  free(splited);
}

// Fonction pour exécuter la commande pwd
// On utilise ici un pipe pour communiquer entre le processus shell et le
// processus pwd Rappelons que le processus shell est le processus parent stocké
// dans pipe_pwd[1] et le processus pwd est le processus enfant stocké dans
// pipe_pwd[0] Par ailleurs, le pid de l'enfant est égal à 0 car fork() retourne
// 0 pour le processus enfant
int execute_pwd()
{
  int pipe_pwd[2];

  if (pipe(pipe_pwd) == -1)
  {
    perror("pipe");
    return -1;
  }

  pid_t pid = fork();

  if (pid == -1)
  {
    perror("fork");
    return 1;
  }
  if (pid == 0)
  {
    // Processus enfant cad pwd
    close(pipe_pwd[0]);               // Fermer le côté lecture du pipe
    dup2(pipe_pwd[1], STDOUT_FILENO); // Rediriger stdout vers le pipe
    close(pipe_pwd[1]);               // Fermer le côté écriture du pipe

    execlp("pwd", "pwd", (char *)NULL);
    perror("execlp");
    exit(EXIT_FAILURE);
  }
  else
  {
    // Processus parent cad le shell
    close(pipe_pwd[1]); // Fermer le côté écriture du pipe

    char buffer[SIZE_PWDBUF];
    ssize_t count = read(pipe_pwd[0], buffer, SIZE_PWDBUF - 1);
    if (count == -1)
    {
      perror("read");
      close(pipe_pwd[0]);
      return 1;
    }
    buffer[count] = '\0'; // Ne jamais oublier ce fichu caractère null

    close(pipe_pwd[0]);

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
    {
      printf("%s", buffer); // Afficher le chemin absolu
      return WEXITSTATUS(status);
    }
    else
    {
      return 1;
    }
  }
}

// Fonction pour changer de répertoire du shell
// On utilise ici la fonction chdir() pour changer de répertoire par souci de
// simplicité On utilise aussi la fonction realpath() pour obtenir le chemin
// absolu du répertoire demandé On met ensuite à jour les variables
// d'environnement PWD et OLDPWD pour refléter le changement de répertoire sur
// le shell
int execute_cd(char *path)
{
  int return_value = 0;

  if (path == NULL)
  {
    path = getenv("HOME");
    if (path == NULL)
    {
      fprintf(stderr, "cd: HOME not set\n");
      return 1;
    }
  }
  else if (strcmp(path, "-") == 0)
  {
    path = getenv("OLDPWD");
    if (path == NULL)
    {
      fprintf(stderr, "cd: OLDPWD not set\n");
      return 1;
    }
  }

  // On doit savoir ou on est puis on change de répertoire
  char *current_dir = nom_repertoire_courant();
  if (current_dir == NULL)
  {
    perror("get_current_dir_name");
    return 1; // L'allocation a échoué
  }

  // Ensuite on va chercher le chemin absolu du répertoire demandé
  char abs_path[PATH_MAX];
  if (realpath(path, abs_path) == NULL)
  {
    perror("realpath");
    return_value = 1;
    goto clean;
  }

  // Changer le répertoire courant en utilisant chdir
  if (chdir(abs_path) == -1)
  {
    perror("chdir");
    return_value = 1;
    goto clean;
  }

  // Mettre à jour les variables d'environnement PWD et OLDPWD
  char *old_pwd = getenv("PWD");
  if (old_pwd)
  {
    if (setenv("OLDPWD", old_pwd, 1) == -1)
    {
      perror("setenv");
      return_value = 1;
      goto clean;
    }
  }

  if (setenv("PWD", abs_path, 1) == -1)
  {
    perror("setenv");
    return_value = 1;
    goto clean;
  }

clean:
  free(current_dir);
  return return_value;
}

char *chemin_du_fichier(char *dir_name, char *file_name) {
    DIR *dir = opendir(dir_name);
    if (dir == NULL) {
        perror("opendir");
        return NULL;
    }

    struct dirent *entry;
    char *path = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, file_name) == 0) {
            path = malloc(strlen(dir_name) + strlen(file_name) + 3);
            if (path == NULL) {
                perror("malloc");
                closedir(dir);
                return NULL;
            }
            snprintf(path, strlen(dir_name) + strlen(file_name) + 3, "%s/%s", dir_name, file_name);
            break;
        }
    }
    closedir(dir);
    return path;
}

int is_absolute_path(char *path)
{
  return path[0] == '/';
}

// Fonction pour afficher le type d'un fichier
int ftype(char *file_name, char *dir_name)
{
  struct stat file_stat;
  char *path = NULL;

  if (file_name == NULL)
  {
    fprintf(stderr, "ftype: No such file or directory\n");
    return 1;
  }
  if (is_absolute_path(file_name))
  {
    path = malloc(strlen(file_name) + 1);
    if (path == NULL)
    {
      perror("malloc");
      return 1;
    }
    strcpy(path, file_name);
  }
  else
  {
    path = chemin_du_fichier(dir_name, file_name);
  }
  if (path == NULL)
  {
    fprintf(stderr, "ftype: No such file or directory\n");
    return 1;
  }

  // Obtenir les informations sur le fichier
  if (lstat(path, &file_stat) == -1)
  {
    fprintf(stderr, "ftype: No such file or directory\n");
    free(path);
    return 1;
  }
  free(path);
  path = NULL;

  // Afficher le type de fichier
  if (S_ISREG(file_stat.st_mode))
  {
    printf("regular file\n");
  }
  else if (S_ISLNK(file_stat.st_mode))
  {
    printf("symbolic link\n");
  }
  else if (S_ISDIR(file_stat.st_mode))
  {
    printf("directory\n");
  }
  else if (S_ISFIFO(file_stat.st_mode))
  {
    printf("named pipe\n");
  }
  else
  {
    printf("other\n");
  }

  return 0;
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
  char *command = args[5];
  int show_all = 0;
  int recursive = 0;
  char *ext = NULL;
  char *type = NULL;
  int max_files = -1;

  //OPTIONS j'ai mis la pour fill en attendant
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
        continue;
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

    char cmd[1024];
    char *new_args[3];
    new_args[0] = command;
    new_args[1] = entry->d_name;
    new_args[2] = NULL;
    snprintf(cmd, sizeof(cmd), "%s %s", command, entry->d_name);
    // Commande interne
    if (strcmp(command, "ftype") == 0)
    {
      ftype(new_args[1], dir_name);
      continue;
    }
    // Commande externe
    pid_t pid = fork();
    if (pid == 0)
    {
      // Enfant : exécuter la commande dans un shell
      execlp("sh", "sh", "-c", cmd, (char *)NULL);
      perror("execlp");
      exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
      perror("fork");
      closedir(dir);
      return 1;
    }
    else
    {
      // Parent : attendre l'enfant
      int status;
      waitpid(pid, &status, 0);
    }

    file_count++;
  }

  closedir(dir);
  return 0;
}

char *trim_and_reduce_spaces(const char *str)
{
  if (str == NULL)
    return NULL;

  // Supprimer les espaces en début
  const char *start = str;
  while (*start == ' ' || *start == '\t' || *start == '\n')
  {
    start++;
  }

  // Supprimer les espaces en fin
  const char *end = start + strlen(start) - 1;
  while (end > start && (*end == ' ' || *end == '\t' || *end == '\n'))
  {
    end--;
  }

  // Calculer la longueur de la nouvelle chaîne
  size_t new_len = end - start + 1;

  // Allouer de la mémoire pour la nouvelle chaîne
  char *new_str = malloc(new_len + 1);
  if (new_str == NULL)
  {
    return NULL; // Allocation échouée
  }

  // Réduire les espaces multiples entre les mots à un seul espace
  char *dest = new_str;
  const char *src = start;
  int in_space = 0;

  while (src <= end)
  {
    if (*src == ' ' || *src == '\t' || *src == '\n')
    {
      if (!in_space)
      {
        *dest++ = ' ';
        in_space = 1;
      }
    }
    else
    {
      *dest++ = *src;
      in_space = 0;
    }
    src++;
  }
  *dest = '\0';

  return new_str;
}

int main()
{
  int last_return_value = 0;
  char formated_promt[MAX_LENGTH_PROMT];
  char **splited;
  // Rediriger stdout vers stderr pour les tests
  rl_outstream = stderr;
  while (1)
  {
    char *current_dir = chemin_du_repertoire();
    char *prompt_dir = malloc(MAX_LENGTH_PROMT);
    // Prendre uniquement les 30 derniers caractères du chemin
    if (strlen(current_dir) > 22)
    {
      snprintf(prompt_dir, MAX_LENGTH_PROMT, "...%s",
               current_dir + strlen(current_dir) - 22);
    }
    else
    {
      snprintf(prompt_dir, MAX_LENGTH_PROMT, "%s", current_dir);
    }
    if (current_dir == NULL)
    {
      perror("chemin_du_repertoire");
      return 1;
    }
    if (last_return_value == 0)
    {
      snprintf(formated_promt, MAX_LENGTH_PROMT, "[%d]%s$ ", last_return_value,
               prompt_dir);
    }
    else if (last_return_value != 0)
    {
      snprintf(formated_promt, MAX_LENGTH_PROMT, "[%s%d%s]%s$ ", RED_COLOR,
               last_return_value, RESET_COLOR, prompt_dir);
    }
    free(current_dir);
    free(prompt_dir);
    // Afficher le prompt
    char *line = readline(formated_promt);
    // Ligne qui contient uniquement le caractère de fin de ligne
    if (line == NULL || line[0] == '\0')
    {
      free(line);
      // Arrêter la boucle et quitter
      exit(last_return_value);
    }
    // Si la ligne a que des espaces alors on la libère et on recommence
    while (strlen(line) == 0 || line[0] == ' ' || line[0] == '\t' || line[0] == '\n')
    {
      free(line);
      line = readline(formated_promt);
      if (line[0] == '\0')
      {
        free(line);
        // Arrêter la boucle et quitter
        exit(last_return_value);
      }
    }
    // Supprimer les espaces en trop
    char *cleaned_line = trim_and_reduce_spaces(line);
    add_history(line);
    // splited = split(ligne,' ');
    splited = str_split(cleaned_line, ' ');
    free(cleaned_line);
    cleaned_line = NULL;
    // if ((args = strtok(ligne, " "))) {
    //     printf("%s\n",args);
    //     args = strtok(NULL, " ");
    //     printf("%s\n",args);
    // }

    // Simplement afficher les arguments pour tester
    /*for (int i = 0; splited[i]; i++) {
      printf("%s\n", splited[i]);
    }*/
    // Cases pour le lancement des commandes intégrées
    if (strcmp(splited[0], "exit") == 0)
    {
      // Si on a un argument alors c'est la valeur de retour
      if (splited[1])
      {
        last_return_value = atoi(splited[1]);
      }
      free(line);
      goto fin;
    }
    else if (strcmp(splited[0], "pwd") == 0)
    {
      last_return_value = execute_pwd();
    }
    else if (strcmp(splited[0], "cd") == 0)
    {
      last_return_value = execute_cd(splited[1]);
    }
    else if (strcmp(splited[0], "ftype") == 0)
    {
      last_return_value = ftype(splited[1], ".");
    }
    else if (strcmp(splited[0] , "for") == 0)
    {
      last_return_value = execute_for(splited);
    }

    free(line);
    line = NULL;
    free_split(splited);
    splited = NULL;
  }

// On libère la mémoire et on quitte
fin:
  free_split(splited);
  exit(last_return_value);
}