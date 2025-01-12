#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

// Fonction pour renvoyer si une chaine de caractères marque la fin d'une commande
int is_special_char(char *c)
{
  return strcmp(c, ";") == 0 || strcmp(c, "|") == 0 || strcmp(c, ">") == 0 || strcmp(c, ">>") == 0 || strcmp(c, "<") == 0 || strcmp(c, ">|") == 0 || strcmp(c, "2>") == 0 || strcmp(c, "2>>") == 0 || strcmp(c, "2>|") == 0 || strcmp(c, "{") == 0 || strcmp(c, "}") == 0 || strcmp(c, "]") == 0;
}

int is_redirection_char(char *c)
{
  return strcmp(c, ">") == 0 || strcmp(c, ">>") == 0 || strcmp(c, "<") == 0 || strcmp(c, ">|") == 0 || strcmp(c, "2>") == 0 || strcmp(c, "2>>") == 0 || strcmp(c, "2>|") == 0;
}


char *substitute_variables(const char *str, int *last_return_value)
{
  char *result = strdup(str);
  char *dollar_pos = strchr(result, '$');

  while (dollar_pos != NULL)
  {
    // On a un $ on doit faire une substitution
    // Les variables ne sont qu'une lettre
    char *var_name = malloc(2);
    strncpy(var_name, dollar_pos + 1, 1);
    var_name[1] = '\0';
    char *var_value = getenv(var_name);
    if (var_value == NULL)
    {
      fprintf(stderr, "Variable %s not set\n", var_name);
      *last_return_value = 1;
      free(var_name);
      free(result);
      return NULL;
    }
    // On a la valeur de la variable on peut la remplacer
    size_t expanded_len = strlen(result) + strlen(var_value) - 1;
    char *expanded = malloc(expanded_len + 1);
    if (expanded == NULL)
    {
      perror("malloc");
      *last_return_value = 1;
      free(var_name);
      free(result);
      return NULL;
    }
    char *suffix = dollar_pos + 1;
    strncpy(expanded, result, dollar_pos - result);
    expanded[dollar_pos - result] = '\0';
    strcat(expanded, var_value);
    strcat(expanded, suffix + 1);
    free(result);
    result = expanded;
    free(var_name);

    // Rechercher la prochaine occurrence de $
    dollar_pos = strchr(result, '$');
  }

  return result;
}



