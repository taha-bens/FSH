#ifndef STACK_DIR_H
#define STACK_DIR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pile pour gérer les dossiers à traiter
typedef struct stack_dir
{
  char *path;
  struct stack_dir *next;
} stack_dir;

// Fonction pour créer une pile
stack_dir *create_stack();

// Fonction pour ajouter un élément à la pile
stack_dir *push(stack_dir *stack, char *path);

// Fonction pour libérer la pile
void free_stack(stack_dir *stack);

// Fonction pour retirer un élément de la pile
char *pop(stack_dir **stack);

#endif // STACK_DIR_H