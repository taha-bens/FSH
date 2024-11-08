#include <assert.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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

int main()
{
  int last_return_value = 0;
  int empty_command = 0;
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
    if (last_return_value == 0 && empty_command == 0)
    {
      snprintf(formated_promt, MAX_LENGTH_PROMT, "[%d]%s$ ", last_return_value,
               prompt_dir);
    }
    else if (last_return_value != 0 && empty_command == 0)
    {
      snprintf(formated_promt, MAX_LENGTH_PROMT, "[%s%d%s]%s$ ", RED_COLOR,
               last_return_value, RESET_COLOR, prompt_dir);
    }
    free(current_dir);
    free(prompt_dir);
    char *ligne = readline(formated_promt);
    // Ligne vide ou contenant uniquement des espaces
    if (!ligne || strlen(ligne) == 0 || strspn(ligne, " \t\r\n") == strlen(ligne))
    {
      free(ligne);
      empty_command = 1;
    }
    else
    {
      add_history(ligne);
      // splited = split(ligne,' ');
      splited = str_split(ligne, ' ');
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
          empty_command = 0;
        }
        free(ligne);
        goto fin;
      }
      else if (strcmp(splited[0], "pwd") == 0)
      {
        last_return_value = execute_pwd();
        empty_command = 0;
      }
      else if (strcmp(splited[0], "cd") == 0)
      {
        last_return_value = execute_cd(splited[1]);
        empty_command = 0;
      }

      free(ligne);
      free_split(splited);
    }
  }

// On libère la mémoire et on quitte
fin:
  free_split(splited);
  exit(last_return_value);
}