#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "headers/pwd.h"

#define PATH_MAX 4096


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