#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#include "headers/pwd.h"

// Fonction pour vérifier si un chemin est absolu
int is_absolute_path(char *path)
{
  return path[0] == '/';
}

// Fonction pour construire le chemin complet d'un fichier
char *chemin_du_fichier(char *dir_name, char *file_name)
{
  char *path = malloc(strlen(dir_name) + strlen(file_name) + 2);
  if (path == NULL)
  {
    perror("malloc");
    return NULL;
  }
  snprintf(path, strlen(dir_name) + strlen(file_name) + 2, "%s/%s", dir_name, file_name);
  return path;
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