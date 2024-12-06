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

#include "headers/pwd.h"
#include "headers/ftype.h"
#include "headers/cd.h"
#include "headers/for.h"

#define RESET_COLOR "\033[00m"
#define RED_COLOR "\033[91m"
#define MAX_LENGTH_PROMT 30 + sizeof(RED_COLOR) + sizeof(RESET_COLOR)
#define SIZE_PWDBUF 1024
#define PATH_MAX 4096

// Split une chaine de caractères en fonction d'un délimiteur
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

  result = malloc(sizeof(char *) * (count + 1));

  if (result)
  {
    size_t idx = 0;
    char *token = strtok(a_str, delim);

    while (token)
    {
      assert(idx < count);
      result[idx++] = strdup(token);
      token = strtok(0, delim);
    }
    if (idx == 0)
    {
      result[idx++] = strdup(a_str);
    }
    result[idx] = 0;
  }

  return result;
}

// Libérer la mémoire allouée pour un tableau de chaînes de caractères
void free_split(char **splited)
{
  for (int i = 0; *(splited + i); i++)
  {
    free(splited[i]);
  }
  free(splited);
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
  char *line;
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
    line = readline(formated_promt);
    // Ligne qui s'est fait EOF
    if (line == NULL)
    {
      // Arrêter la boucle et quitter
      exit(last_return_value);
    }
    // Supprimer les espaces en trop
    char *cleaned_line = trim_and_reduce_spaces(line);
    if (strlen(cleaned_line) == 0)
    {
      free(line);
      free(cleaned_line);
      continue;
    }
    add_history(line);
    // Diviser la ligne en commandes séparées par des points-virgules
    char **commands = str_split(cleaned_line, ';');
    free(cleaned_line);
    cleaned_line = NULL;

    // Exécuter chaque commande séparément
    for (int cmd_idx = 0; commands[cmd_idx]; cmd_idx++)
    {
      // Diviser la commande en arguments
      splited = str_split(commands[cmd_idx], ' ');

      // Cases pour le lancement des commandes intégrées
      if (strcmp(splited[0], "exit") == 0)
      {
        // Si on a un argument alors c'est la valeur de retour
        if (splited[1])
        {
          last_return_value = atoi(splited[1]);
        }
        free(line);
        free_split(commands);
        goto fin;
      }
      else if (strcmp(splited[0], "pwd") == 0)
      {
        last_return_value = pwd();
      }
      else if (strcmp(splited[0], "cd") == 0)
      {
        last_return_value = execute_cd(splited[1]);
      }
      else if (strcmp(splited[0], "ftype") == 0)
      {
        last_return_value = ftype(splited[1], ".");
      }
      else if (strcmp(splited[0], "for") == 0)
      {
        last_return_value = execute_for(splited);
      }
      else
      {
        // Commande externe
        pid_t pid = fork();
        if (pid == 0)
        {
          // Enfant : exécuter la commande
          execvp(splited[0], splited);
          fprintf(stderr, "redirect_exec: No such file or directory\n");
          free(line);
          free_split(splited);
          free_split(commands);
          exit(EXIT_FAILURE);
        }
        else if (pid < 0)
        {
          perror("fork");
          free(line);
          free_split(splited);
          free_split(commands);
          continue;
        }
        else
        {
          // Parent : attendre l'enfant
          int status;
          waitpid(pid, &status, 0);
          if (WIFEXITED(status))
          {
            last_return_value = WEXITSTATUS(status);
          }
          else
          {
            last_return_value = 1;
          }
        }
      }

      free_split(splited);
      splited = NULL;
    }

    free(line);
    line = NULL;
    free_split(commands);
    commands = NULL;
  }

// On libère la mémoire et on quitte
fin:
  free_split(splited);
  exit(last_return_value);
}