#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <ctype.h>

#include "headers/string_util.h"
#include "headers/node.h"

#include "headers/ast.h"

ast_node *parse_for_loop(char **tokens, int *index)
{
  // Vérifier la syntaxe le premier argument est nécessairement le nom de la variable
  if (tokens[*index + 1] == NULL || tokens[*index + 2] == NULL || tokens[*index + 3] == NULL || tokens[*index + 4] == NULL)
  {
    write(STDERR_FILENO, "for: Invalid syntax\n", 21);
    return NULL;
  }
  (*index)++;
  // Aller chercher le nom de la variable et le répertoire qui suit
  char *variable = malloc(strlen(tokens[*index]) + 2);
  strcpy(variable, tokens[*index]);
  (*index)++; // In
  (*index)++; // Le nom du répertoire
  char *dir = tokens[(*index)];

  (*index)++;

  // Initialiser les options
  int show_all = 0;
  int recursive = 0;
  char *ext = NULL;
  char *type = NULL;
  int max_files = 0;
  char **options = NULL;
  int options_len = 0;

  // Traiter les options
  while (tokens[*index] && tokens[*index][0] != '{' && tokens[*index][0] == '-')
  {
    if (strcmp(tokens[*index], "-A") == 0)
    {
      show_all = 1;
    }
    else if (strcmp(tokens[*index], "-r") == 0)
    {
      recursive = 1;
    }
    else if (strcmp(tokens[*index], "-e") == 0 && tokens[*index + 1])
    {
      // Allouer de la mémoire pour l'extension avec le point
      ext = malloc(strlen(tokens[*index + 1]) + 2); // +2 pour le point et le caractère nul
      if (ext == NULL)
      {
        perror("malloc");
        return NULL;
      }
      // Ajouter un point pour l'extension
      strcpy(ext, ".");
      strcat(ext, tokens[++(*index)]);
    }
    else if (strcmp(tokens[*index], "-t") == 0 && tokens[*index + 1])
    {
      type = tokens[++(*index)];
    }
    else if (strcmp(tokens[*index], "-p") == 0 && tokens[*index + 1])
    {
      // Vérifier qu'on a bien un nombre après l'option
      if (isdigit(tokens[*index + 1][0]) == 0)
      {
        fprintf(stderr, "for: Invalid syntax\n");
        free(variable);
        return NULL;
      }
      max_files = atoi(tokens[++(*index)]);
    }
    options = realloc(options, (options_len + 1) * sizeof(char *));
    options[options_len++] = strdup(tokens[*index]);
    (*index)++;
  }

  // Null-terminer le tableau d'options
  options = realloc(options, (options_len + 1) * sizeof(char *));
  options[options_len] = NULL;

  // On a fini de traiter les options si les options sont vides on les libère
  if (options_len == 0)
  {
    free(options);
    options = NULL;
  }

  // Une fois les options traitées, vérifier qu'il existe un bloc entre accolades
  if (tokens[*index] == NULL || strcmp(tokens[*index], "{") != 0)
  {
    fprintf(stderr, "for: Invalid syntax\n");
    free(variable);
    return NULL;
  }
  int end = *index + 1;
  int bracket_count = 1;
  while (bracket_count != 0)
  {
    if (tokens[end] == NULL)
    {
      fprintf(stderr, "for: Invalid syntax\n");
      free(variable);
      return NULL;
    }

    if (strcmp(tokens[end], "{") == 0)
    {
      bracket_count++;
    }
    if (strcmp(tokens[end], "}") == 0)
    {
      bracket_count--;
    }
    if (bracket_count != 0)
    {
      end++;
    }
  }
  (*index)++;
  // Construire le bloc
  ast_node *block = create_ast_node(NODE_SEQUENCE, "BLOCK");
  // Si il y a un point virgule ou un pipe c'est qu'il y a une autre commande après le bloc
  while (*index < end)
  {
    ast_node *child = construct_ast_rec(tokens, index);
    if (child != NULL)
    {
      if (tokens[*index] && strcmp(tokens[*index], "|") == 0)
      {
        // Construire le reste du pipeline
        (*index)++;
        ast_node *second = construct_ast_rec(tokens, index);
        if (second == NULL)
        {
          free_ast_node(child);
          return NULL;
        }
        ast_node *pipeline = create_pipeline_node(child, second);
        // Ajouter les autres commandes au pipeline
        while (tokens[*index] != NULL && strcmp(tokens[*index], "|") == 0)
        {
          (*index)++;
          ast_node *child = construct_ast_rec(tokens, index);
          if (child != NULL)
          {
            pipeline = create_pipeline_node(pipeline, child);
          }
          else
          {
            free_ast_node(pipeline);
            free_ast_node(block);
            return NULL;
          }
        }
        add_child(block, pipeline);
        continue;
      }
      add_child(block, child);
    }
    else
    {
      free_ast_node(block);
      return NULL;
    }
  }
  ast_node *node = create_for_node(dir, variable, options, show_all, recursive, ext, type, max_files, block);
  free(variable);
  return node;
}


ast_node *parse_if(char **tokens, int *index)
{
  // Vérifier si on a un crochet ouvrant qui marque le début de la condition
  if (tokens[*index + 1] == NULL || tokens[*index + 2] == NULL || tokens[*index + 3] == NULL)
  {
    fprintf(stderr, "if: Invalid syntax\n");
    return NULL;
  }

  int end = 0;
  bool sans_crochet = strcmp(tokens[*index + 1], "[") != 0;
  // Vérifier si on a un crochet ouvrant qui marque le début de la condition
  if (sans_crochet)
  {
    // Aller chercher la condition qui peut être une succession de commandes jusqu'à l'accolade ouvrante
    (*index)++;
    end = *index;
    while (tokens[end] != NULL && strcmp(tokens[end], "{") != 0)
    {
      if (strcmp(tokens[end], "for") == 0)
      {
        // S'avancer jusqu'à la première accolade ouvrante
        while (tokens[end] != NULL && strcmp(tokens[end], "{") != 0)
        {
          end++;
        }
        // Trouver son accolade fermante en comptant les accolades ouvrantes
        int bracket_count = 1;
        end++;
        while (bracket_count != 0)
        {
          if (tokens[end] == NULL)
          {
            fprintf(stderr, "for: Invalid syntax\n");
            return NULL;
          }

          if (strcmp(tokens[end], "{") == 0)
          {
            bracket_count++;
          }
          if (strcmp(tokens[end], "}") == 0)
          {
            bracket_count--;
          }
          if (bracket_count != 0)
          {
            end++;
          }
        }
      }
      end++;
    }
    if (tokens[end] == NULL)
    {
      fprintf(stderr, "if: Invalid syntax\n");
      return NULL;
    }
  }
  // Avec crochet
  else
  {
    (*index)++;
    // Aller chercher la condition qui est une commande jusqu'au crochet fermant
    end = *index + 1;
    while (tokens[end] != NULL && strcmp(tokens[end], "]") != 0)
    {
      end++;
    }
    if (tokens[end] == NULL)
    {
      fprintf(stderr, "if: Invalid syntax\n");
      return NULL;
    }
  }

  // Construire la condition
  ast_node *condition_node = create_ast_node(NODE_SEQUENCE, "CONDITION");
  int saut = sans_crochet ? 0 : 1;
  if (sans_crochet)
  {
    // Aller chercher la condition qui est une succession de commandes jusqu'à l'accolade ouvrante
    while (*index < end)
    {

      ast_node *child = construct_ast_rec(tokens, index);
      if (child != NULL)
      {
        if (tokens[*index] && strcmp(tokens[*index], "|") == 0)
        {
          // Construire le reste du pipeline
          (*index)++;
          ast_node *second = construct_ast_rec(tokens, index);
          if (second == NULL)
          {
            free_ast_node(condition_node);
            return NULL;
          }
          ast_node *pipeline = create_pipeline_node(child, second);
          // Ajouter les autres commandes au pipeline
          while (tokens[*index] != NULL && strcmp(tokens[*index], "|") == 0)
          {
            (*index)++;
            ast_node *child = construct_ast_rec(tokens, index);
            if (child != NULL)
            {
              pipeline = create_pipeline_node(pipeline, child);
            }
            else
            {
              free_ast_node(pipeline);
              free_ast_node(condition_node);
              return NULL;
            }
          }
          add_child(condition_node, pipeline);
          continue;
        }
        add_child(condition_node, child);
      }
      else
      {
        free_ast_node(condition_node);
        return NULL;
      }
    }
  }
  if (!sans_crochet)
  {
    (*index)++;
    while (*index < end)
    {
      ast_node *child = construct_ast_rec(tokens, index);
      // Si c'est avec crochet on ajoute test devant la commande
      if (child != NULL)
      {
        char **args = child->data.cmd.args;
        char **new_args = malloc((child->data.cmd.argc + 2) * sizeof(char *));
        new_args[0] = strdup("test");
        for (int j = 0; j < child->data.cmd.argc; j++)
        {
          new_args[j + 1] = strdup(args[j]);
        }
        new_args[child->data.cmd.argc + 1] = NULL;

        // Libérer les anciens arguments
        for (int j = 0; j < child->data.cmd.argc; j++)
        {
          free(args[j]);
        }
        free(child->data.cmd.args);
        // Ne pas oublier aussi NULL
        child->data.cmd.args = new_args;
        child->data.cmd.argc++;
        child->data.cmd.args[child->data.cmd.argc] = NULL;
        add_child(condition_node, child);
      }
      else
      {
        free_ast_node(condition_node);
        return NULL;
      }
    }
  }

  // Aller chercher le bloc then
  if (tokens[end + saut] == NULL || strcmp(tokens[end + saut], "{") != 0)
  {
    fprintf(stderr, "if: Invalid syntax\n");
    free_ast_node(condition_node);
    return NULL;
  }
  int then_end = end + 1 + saut;
  int bracket_count = 1;
  while (bracket_count != 0)
  {
    if (tokens[then_end] == NULL)
    {
      fprintf(stderr, "if: Invalid syntax\n");
      free_ast_node(condition_node);
      return NULL;
    }
    if (strcmp(tokens[then_end], "{") == 0)
    {
      bracket_count++;
    }
    if (strcmp(tokens[then_end], "}") == 0)
    {
      bracket_count--;
    }
    if (bracket_count != 0)
    {
      then_end++;
    }
  }
  (*index) = end + 1 + saut;
  ast_node *then_block = create_ast_node(NODE_SEQUENCE, "THEN_BLOCK");
  // Si il y a un point virgule et qu'on a pas atteint la fin c'est qu'il y a une autre commande après le bloc et on continue la récursion
  while (*index < then_end)
  {
    ast_node *child = construct_ast_rec(tokens, index);
    if (child != NULL)
    {
      if (tokens[*index] && strcmp(tokens[*index], "|") == 0)
      {
        // Construire le reste du pipeline
        (*index)++;
        ast_node *second = construct_ast_rec(tokens, index);
        if (second == NULL)
        {
          free_ast_node(then_block);
          free_ast_node(condition_node);
          return NULL;
        }
        ast_node *pipeline = create_pipeline_node(child, second);
        // Ajouter les autres commandes au pipeline
        while (tokens[*index] != NULL && strcmp(tokens[*index], "|") == 0)
        {
          (*index)++;
          ast_node *child = construct_ast_rec(tokens, index);
          if (child != NULL)
          {
            pipeline = create_pipeline_node(pipeline, child);
          }
          else
          {
            free_ast_node(then_block);
            free_ast_node(condition_node);
            free_ast_node(pipeline);
            return NULL;
          }
        }
        add_child(then_block, pipeline);
      }
      else
      {
        add_child(then_block, child);
      }
    }
    else
    {
      free_ast_node(then_block);
      free_ast_node(condition_node);
      return NULL;
    }
  }
  // Aller chercher le bloc else si il existe il est optionnel
  if (tokens[then_end + 1] != NULL && strcmp(tokens[then_end + 1], "else") == 0)
  {
    if (tokens[then_end + 2] == NULL || strcmp(tokens[then_end + 2], "{") != 0)
    {
      fprintf(stderr, "if: Invalid syntax\n");
      free_ast_node(then_block);
      free_ast_node(condition_node);
      return NULL;
    }
    int else_end = then_end + 3;
    bracket_count = 1;
    while (bracket_count != 0)
    {
      if (tokens[else_end] == NULL)
      {
        fprintf(stderr, "if: Invalid syntax\n");
        free_ast_node(then_block);
        free_ast_node(condition_node);
        return NULL;
      }

      if (strcmp(tokens[else_end], "{") == 0)
      {
        bracket_count++;
      }
      if (strcmp(tokens[else_end], "}") == 0)
      {
        bracket_count--;
      }
      if (bracket_count != 0)
      {
        else_end++;
      }
    }
    then_end += 3;
    (*index) = then_end;
    ast_node *else_block = create_ast_node(NODE_SEQUENCE, "ELSE_BLOCK");
    // Si il y a un point virgule et qu'on a pas atteint la fin
    while (*index < else_end)
    {
      ast_node *child = construct_ast_rec(tokens, index);
      if (child != NULL)
      {
        if (tokens[*index] && strcmp(tokens[*index], "|") == 0)
        {
          // Construire le reste du pipeline
          (*index)++;
          ast_node *second = construct_ast_rec(tokens, index);
          if (second == NULL)
          {
            free_ast_node(else_block);
            free_ast_node(condition_node);
            free_ast_node(then_block);
            return NULL;
          }
          ast_node *pipeline = create_pipeline_node(child, second);
          // Ajouter les autres commandes au pipeline
          while (tokens[*index] != NULL && strcmp(tokens[*index], "|") == 0)
          {
            (*index)++;
            ast_node *child = construct_ast_rec(tokens, index);
            if (child != NULL)
            {
              pipeline = create_pipeline_node(pipeline, child);
            }
            else
            {
              free_ast_node(else_block);
              free_ast_node(condition_node);
              free_ast_node(then_block);
              free_ast_node(pipeline);
              return NULL;
            }
          }
          add_child(else_block, pipeline);
        }
        else
        {
          add_child(else_block, child);
        }
      }
      else
      {
        free_ast_node(else_block);
        free_ast_node(condition_node);
        free_ast_node(then_block);
        return NULL;
      }
    }
    ast_node *node = create_if_node(condition_node, then_block, else_block);
    (*index) = else_end + 1;
    return node;
  }
  else
  {
    ast_node *node = create_if_node(condition_node, then_block, NULL);
    // On a fini de traiter le if on sort de la boucle
    (*index) = then_end + 1;
    return node;
  }
  return NULL;
}

ast_node *check_redirection(char **tokens, int *index, ast_node *node)
{
  ast_node *redirection = NULL;
  if (strcmp(tokens[*index], ">") == 0)
  {
    if (tokens[*index + 1] == NULL)
    {
      fprintf(stderr, ">: Invalid syntax\n");
      return NULL;
    }
    (*index)++;
    if (access(tokens[*index], F_OK) == 0)
    {
      fprintf(stderr, "pipeline_run: File exists\n");
      return NULL;
    }
    // Créer le nœud de redirection
    redirection = create_redirection_node(tokens[*index], STDOUT_FILENO, O_WRONLY | O_CREAT | O_TRUNC);
    (*index)++;
  }
  else if (strcmp(tokens[*index], ">>") == 0)
  {
    // Vérifier la syntaxe
    if (tokens[*index + 1] == NULL)
    {
      fprintf(stderr, ">>: Invalid syntax\n");
      return NULL;
    }
    (*index)++;
    // Créer le nœud de redirection
    redirection = create_redirection_node(tokens[*index], STDOUT_FILENO, O_WRONLY | O_CREAT | O_APPEND);
    (*index)++;
  }
  else if (strcmp(tokens[*index], "<") == 0)
  {
    // Vérifier la syntaxe
    if (tokens[*index + 1] == NULL)
    {
      fprintf(stderr, "<: Invalid syntax\n");
      return NULL;
    }
    (*index)++;
    // Créer le nœud de redirection
    redirection = create_redirection_node(tokens[*index], STDIN_FILENO, O_RDONLY);
    (*index)++;
  }
  else if (strcmp(tokens[*index], "2>") == 0)
  {
    // Vérifier la syntaxe
    if (tokens[*index + 1] == NULL)
    {
      fprintf(stderr, "2>: Invalid syntax\n");
      return NULL;
    }
    (*index)++;
    if (access(tokens[*index], F_OK) == 0)
    {
      fprintf(stderr, "pipeline_run: File exists\n");
      return NULL;
    }
    // Créer le nœud de redirection
    redirection = create_redirection_node(tokens[*index], STDERR_FILENO, O_WRONLY | O_CREAT | O_TRUNC);
    (*index)++;
  }
  else if (strcmp(tokens[*index], "2>>") == 0)
  {
    // Vérifier la syntaxe
    if (tokens[*index + 1] == NULL)
    {
      fprintf(stderr, "2>>: Invalid syntax\n");
      return NULL;
    }
    (*index)++;
    // Créer le nœud de redirection
    redirection = create_redirection_node(tokens[*index], STDERR_FILENO, O_WRONLY | O_CREAT | O_APPEND);
    (*index)++;
  }
  else if (strcmp(tokens[*index], "2>|") == 0)
  {
    // Vérifier la syntaxe
    if (tokens[*index + 1] == NULL)
    {
      fprintf(stderr, "2>|: Invalid syntax\n");
      return NULL;
    }
    (*index)++;
    // Créer le nœud de redirection
    redirection = create_redirection_node(tokens[*index], STDERR_FILENO, O_WRONLY | O_CREAT | O_TRUNC);
    (*index)++;
  }
  else if (strcmp(tokens[*index], ">|") == 0)
  {
    // Vérifier la syntaxe
    if (tokens[*index + 1] == NULL)
    {
      fprintf(stderr, ">|: Invalid syntax\n");
      return NULL;
    }
    (*index)++;
    // Créer le nœud de redirection
    redirection = create_redirection_node(tokens[*index], STDOUT_FILENO, O_WRONLY | O_CREAT | O_TRUNC);
    (*index)++;
  }
  else
  {
    fprintf(stderr, "Unknown redirection\n");
    return NULL;
  }
  return redirection;
}

ast_node *parse_command(char **tokens, int *index)
{
  int argc = 0;

  // Compter les arguments
  int start_index = *index;
  // SAINTE MERE DE DIEU C'EST HORRIBLE
  while (tokens[*index] != NULL && !is_special_char(tokens[*index]))
  {
    (*index)++;
    argc++;
  }

  // Copier les arguments
  char **args = malloc(argc * sizeof(char *));
  for (int i = 0; i < argc; i++)
  {
    args[i] = strdup(tokens[start_index + i]);
  }

  // Créer le nœud
  ast_node *node = create_command_node(args, argc);

  // Libérer les arguments
  for (int i = 0; i < argc; i++)
  {
    free(args[i]);
  }
  free(args);

  // Regarder si on a des redirections après la commande
  ast_node *redirection = NULL;
  while (tokens[*index] != NULL && is_redirection_char(tokens[*index]))
  {
    redirection = check_redirection(tokens, index, node);

    // Ajouter la redirection à la commande
    if (redirection != NULL)
    {
      add_child(node, redirection);
      redirection = NULL;
    }
    else
    {
      free_ast_node(node);
      return NULL;
    }
  }

  return node;
}

// Fonction récursive pour construire l'AST
ast_node *construct_ast_rec(char **tokens, int *index)
{
  ast_node *node = NULL;

  while (tokens[*index] != NULL)
  {

    if (strcmp(tokens[*index], "|") == 0)
    {
      // Vérifier que l'on a une commande
      break;
    }

    if (strcmp(tokens[*index], "else") == 0)
    {
      // Vérifier que l'else est bien après un bloc then
      int if_index = *index - 1;
      while (if_index >= 0 && strcmp(tokens[if_index], "if") != 0)
      {
        if_index--;
      }
      if (if_index < 0 || strcmp(tokens[if_index], "if") != 0)
      {
        fprintf(stderr, "Unexpected else\n");
        fprintf(stderr, "At index %d\n", *index);
        return NULL;
      }
      (*index)++;
      break;
    }

    if (strcmp(tokens[*index], "]") == 0)
    {
      (*index)++;
      break;
    }

    if (strcmp(tokens[*index], "{") == 0)
    {
      // Regarder si on a pas un if avant le bloc
      int if_index = *index - 1;
      while (if_index >= 0 && strcmp(tokens[if_index], "if") != 0)
      {
        if_index--;
      }
      if (if_index >= 0 && strcmp(tokens[if_index], "if") == 0)
      {
        // On a un if avant le bloc il n'y a aucun traitement à faire
        break;
      }
      // Impossible car elle sont traitées par le for
      fprintf(stderr, "Unexpected opening bracket\n");
      fprintf(stderr, "At index %d\n", *index);
      return NULL;
    }

    if (strcmp(tokens[*index], "}") == 0)
    {
      // Ignorer les fermetures de blocs
      (*index)++;
      break;
    }

    else if (strcmp(tokens[*index], ";") == 0)
    {
      (*index)++;
      break;
    }

    else if (strcmp(tokens[*index], "for") == 0)
    {
      node = parse_for_loop(tokens, index);
      if (node == NULL)
      {
        return NULL;
      }
    }
    else if (strcmp(tokens[*index], "if") == 0)
    {
      node = parse_if(tokens, index);
      if (node == NULL)
      {
        return NULL;
      }
    }
    else
    {
      node = parse_command(tokens, index);
      if (node == NULL)
      {
        return NULL;
      }
    }
  }
  return node;
}

// Fonction principale pour construire l'AST
ast_node *construct_ast(char *line)
{
  char **tokens = str_split(line, ' ');
  int index = 0;
  // Trouver tous les ; qui sont en dehors des blocs
  ast_node *root = create_ast_node(NODE_SEQUENCE, "ROOT");

  while (tokens[index] != NULL)
  {
    ast_node *child = construct_ast_rec(tokens, &index);
    if (child == NULL)
    {
      free_ast_node(root);
      free_split(tokens);
      return NULL;
    }
    else
    {
      // Vérifier si on a un pipe après la commande
      if (tokens[index] != NULL && strcmp(tokens[index], "|") == 0)
      {
        index++;
        // Construire le reste du pipeline
        ast_node *second = construct_ast_rec(tokens, &index);
        if (second == NULL)
        {
          free_ast_node(root);
          free_ast_node(child);
          free_split(tokens);
          return NULL;
        }
        ast_node *pipeline = create_pipeline_node(child, second);
        // Ajouter les autres commandes au pipeline
        while (tokens[index] != NULL && strcmp(tokens[index], "|") == 0)
        {
          index++;
          ast_node *child = construct_ast_rec(tokens, &index);
          if (child != NULL)
          {
            pipeline = create_pipeline_node(pipeline, child);
          }
          else
          {
            free_ast_node(pipeline);
            free_ast_node(root);
            free_split(tokens);
            return NULL;
          }
        }
        add_child(root, pipeline);
      }
      else
      {
        add_child(root, child);
      }
    }
  }

  free_split(tokens);
  tokens = NULL;
  return root;
}

void handle_substitution(command *cmd, char **copy_args, int *last_return_value)
{
  for (int i = 0; i < cmd->argc; i++)
  {
    char *substituted = substitute_variables(cmd->args[i], last_return_value);
    if (substituted == NULL)
    {
      for (int j = 0; j < cmd->argc; j++)
      {
        free(cmd->args[j]);
        cmd->args[j] = strdup(copy_args[j]);
      }
      for (int j = 0; j < cmd->argc; j++)
      {
        free(copy_args[j]);
      }
      free(copy_args);
      return;
    }
    free(cmd->args[i]);
    cmd->args[i] = substituted;
  }
}
