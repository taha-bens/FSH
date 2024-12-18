#include <stdio.h>  
#include <stdlib.h>
#include <string.h>

// Pile pour gérer les dossiers à traiter
typedef struct stack_dir
{
  char *path;
  struct stack_dir *next;
} stack_dir;

stack_dir *create_stack()
{
  stack_dir *stack = malloc(sizeof(stack_dir));
  if (stack == NULL)
  {
    perror("malloc");
    return NULL;
  }
  stack->path = NULL;
  stack->next = NULL;
  return stack;
}

stack_dir *push(stack_dir *stack, char *path)
{
  stack_dir *new_node = malloc(sizeof(stack_dir));
  if (new_node == NULL)
  {
    perror("malloc");
    return NULL;
  }
  new_node->path = path;
  new_node->next = stack;
  return new_node;
}

void free_stack(stack_dir *stack)
{
  while (stack != NULL)
  {
    stack_dir *temp = stack;
    stack = stack->next;
    free(temp->path);
    free(temp);
  }
}

char *pop(stack_dir **stack)
{
  if (*stack == NULL)
  {
    return NULL;
  }
  stack_dir *temp = *stack;
  *stack = (*stack)->next;
  char *path = temp->path;
  free(temp);
  return path;
}