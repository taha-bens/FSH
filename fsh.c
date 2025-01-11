#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include <signal.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "headers/node.h"
#include "headers/stack_dir.h"
#include "headers/string_util.h"
#include "headers/pwd.h"
#include "headers/ftype.h"
#include "headers/cd.h"

#define RESET_COLOR "\033[00m"
#define RED_COLOR "\033[91m"
#define MAX_LENGTH_PROMPT 30 + sizeof(RED_COLOR) + sizeof(RESET_COLOR)
#define SIZE_PWDBUF 1024
#define PATH_MAX 4096

volatile sig_atomic_t signal_received = 0;
volatile sig_atomic_t sigint_received = 0;
volatile sig_atomic_t in_exec = 0;

ast_node *construct_ast_rec(char **tokens, int *index);
void execute_ast(ast_node *node, int *last_return_value);
void create_dir_prompt_name(char *prompt, const char *current_dir, int last_return_value, int sigint_received);
void create_prompt(char *prompt, const char *current_dir, int last_return_value, int sigint_received);
void restore_standard_fds(int saved_stdin, int saved_stdout, int saved_stderr);
void cleanup_and_exit(int last_return_value, ast_node *tree);

void exec_for_simple(for_loop *loop, int *last_return_value, char **files, int previous_return_value, char *original_dir);

// Fonction pour renvoyer si une chaine de caractères marque la fin d'une commande
int is_special_char(char *c)
{
  return strcmp(c, ";") == 0 || strcmp(c, "|") == 0 || strcmp(c, ">") == 0 || strcmp(c, ">>") == 0 || strcmp(c, "<") == 0 || strcmp(c, ">|") == 0 || strcmp(c, "2>") == 0 || strcmp(c, "2>>") == 0 || strcmp(c, "2>|") == 0 || strcmp(c, "{") == 0 || strcmp(c, "}") == 0 || strcmp(c, "]") == 0;
}

int is_redirection_char(char *c)
{
  return strcmp(c, ">") == 0 || strcmp(c, ">>") == 0 || strcmp(c, "<") == 0 || strcmp(c, ">|") == 0 || strcmp(c, "2>") == 0 || strcmp(c, "2>>") == 0 || strcmp(c, "2>|") == 0;
}

ast_node *construct_pipeline_and_add_child(char **tokens, int *index, ast_node *block)
{
  ast_node *child = construct_ast_rec(tokens, index);
  if (child == NULL)
  {
    return NULL;
  }

  if (tokens[*index] && strcmp(tokens[*index], "|") == 0)
  {
    (*index)++;
    ast_node *second = construct_ast_rec(tokens, index);
    if (second == NULL)
    {
      free_ast_node(child);
      return NULL;
    }
    ast_node *pipeline = create_pipeline_node(child, second);
    while (tokens[*index] != NULL && strcmp(tokens[*index], "|") == 0)
    {
      (*index)++;
      ast_node *next_child = construct_ast_rec(tokens, index);
      if (next_child != NULL)
      {
        pipeline = create_pipeline_node(pipeline, next_child);
      }
      else
      {
        free_ast_node(pipeline);
        return NULL;
      }
    }
    add_child(block, pipeline);
  }
  else
  {
    add_child(block, child);
    (*index)--;
  }

  return block;
}

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
  // Créer la première commande dans le for
  ast_node *block = construct_ast_rec(tokens, index);
  (*index)--;
  // Si il y a un point virgule ou un pipe c'est qu'il y a une autre commande après le bloc
  while (*index < end)
  {
    (*index)++;
    if (construct_pipeline_and_add_child(tokens, index, block) == NULL)
    {
      free_ast_node(block);
      free(variable);
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
  ast_node *then_block = construct_ast_rec(tokens, index);
  // Si il y a un point virgule et qu'on a pas atteint la fin c'est qu'il y a une autre commande après le bloc et on continue la récursion
  (*index)--; // On a déjà incrémenté l'index
  while (*index < then_end)
  {
    (*index)++;
    if (construct_pipeline_and_add_child(tokens, index, then_block) == NULL)
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
    ast_node *else_block = construct_ast_rec(tokens, index);
    // Si il y a un point virgule et qu'on a pas atteint la fin
    (*index)--; // On a déjà incrémenté l'index
    while (*index < else_end)
    {
      (*index)++;
      if (construct_pipeline_and_add_child(tokens, index, else_block) == NULL)
      {
        free_ast_node(then_block);
        free_ast_node(condition_node);
        free_ast_node(else_block);
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
    }

    else if (strcmp(tokens[*index], ";") == 0)
    {
      (*index)++;
      break;
    }

    else if (strcmp(tokens[*index], "for") == 0)
    {
      node = parse_for_loop(tokens, index);
    }
    else if (strcmp(tokens[*index], "if") == 0)
    {
      node = parse_if(tokens, index);
    }
    else
    {
      node = parse_command(tokens, index);
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
    if (child != NULL)
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
    else
    {
      free_ast_node(root);
      root = NULL;
      break;
    }
  }

  free_split(tokens);
  tokens = NULL;
  return root;
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

void execute_for(ast_node *node, int *last_return_value)
{
  for_loop *loop = &node->data.for_loop;
  char *original_dir = strdup(loop->dir);
  char *substituted_dir = substitute_variables(loop->dir, last_return_value);
  if (substituted_dir == NULL)
  {
    free(original_dir);
    return;
  }
  free(loop->dir);
  loop->dir = substituted_dir;
  struct stat path_stat;
  if (stat(loop->dir, &path_stat) == -1)
  {
    fprintf(stderr, "command_for_run: %s\n", loop->dir);
    perror("command_for_run");
    free(loop->dir);
    loop->dir = NULL;

    // Restaurer dir à sa valeur d'origine
    loop->dir = strdup(original_dir);

    // Libérer la mémoire allouée pour le chemin du répertoire
    free(original_dir);
    *last_return_value = 1;
    return;
  }

  if (!S_ISDIR(path_stat.st_mode))
  {
    fprintf(stderr, "command_for_run: Not a directory\n");
    free(loop->dir);
    loop->dir = NULL;

    // Restaurer dir à sa valeur d'origine
    loop->dir = strdup(original_dir);

    // Libérer la mémoire allouée pour le chemin du répertoire
    free(original_dir);
    *last_return_value = 1;
    return;
  }

  DIR *dir = opendir(loop->dir);
  if (dir == NULL)
  {
    perror("command_for_run");
    *last_return_value = 1;
    free(loop->dir);
    loop->dir = NULL;

    // Restaurer dir à sa valeur d'origine
    loop->dir = strdup(original_dir);

    // Libérer la mémoire allouée pour le chemin du répertoire
    free(original_dir);
    return;
  }
  closedir(dir);
  struct dirent *entry;
  int file_count = 0;

  // Obtenir la liste correcte des fichiers en les stockant dans un tableau
  char **files = malloc(1 * sizeof(char *));
  if (files == NULL)
  {
    perror("malloc");
    *last_return_value = 1;
    free(loop->dir);
    loop->dir = NULL;

    // Restaurer dir à sa valeur d'origine
    loop->dir = strdup(original_dir);

    // Libérer la mémoire allouée pour le chemin du répertoire
    free(original_dir);
    return;
  }
  int files_len = 0;
  stack_dir *stack = NULL;
  stack = push(stack, strdup(loop->dir));
  char *cur_path = NULL;

  while ((cur_path = pop(&stack)) != NULL)
  {
    DIR *cur_dir = opendir(cur_path);
    if (cur_dir == NULL)
    {
      perror("opendir");
      free(cur_path);
      break;
    }
    while ((entry = readdir(cur_dir)) != NULL)
    {

      // Ignorer . et ..
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;

      // Ignorer les fichiers cachés si show_all n'est pas activé
      if (entry->d_name[0] == '.' && !loop->show_all) // Fichier caché
        continue;

      // Créer le chemin complet du fichier
      char file_path[PATH_MAX];
      snprintf(file_path, PATH_MAX, "%s/%s", cur_path, entry->d_name);

      // Vérification des options (extension, type, etc.)
      struct stat file_stat;
      if (stat(file_path, &file_stat) == -1)
      {
        perror("stat");
        continue;
      }

      if (loop->recursive && stat(file_path, &file_stat) == 0 && S_ISDIR(file_stat.st_mode))
      {
        stack = push(stack, strdup(file_path));
      }

      // Filtrer les fichiers par type
      if (loop->type)
      {
        if ((strcmp(loop->type, "f") == 0 && !S_ISREG(file_stat.st_mode)) ||
            (strcmp(loop->type, "d") == 0 && !S_ISDIR(file_stat.st_mode)))
          continue;
      }

      // parallèle d'un maximum de max_files tours de boucle.
      if (loop->max_files > 0 && file_count >= loop->max_files)
      {
        //printf("Max files reached\n");
        //break;
      }

      // Filtrer les fichiers par extension
      if (loop->ext)
      {
        char *file_ext = strrchr(entry->d_name, '.');
        if (file_ext == NULL || strcmp(file_ext, loop->ext) != 0)
          continue;
        // On a trouvé un fichier qui correspond à l'extension recherchée on supprime toute la chaine
        // pour ne garder que le nom du fichier on change donc file_path
        char *file_name = malloc(strlen(entry->d_name) + 1);
        strcpy(file_name, entry->d_name);
        file_name[strlen(entry->d_name) - strlen(file_ext)] = '\0';
        snprintf(file_path, PATH_MAX, "%s/%s", cur_path, file_name);
        free(file_name);
      }

      // Ajouter le fichier au tableau
      files = realloc(files, (files_len + 1) * sizeof(char *));
      files[files_len++] = strdup(file_path);

      // Compteur de fichiers traités
      file_count++;
    }
    closedir(cur_dir);
    free(cur_path);
  }
  free_stack(stack);

  // Null-terminer le tableau
  files = realloc(files, (files_len + 1) * sizeof(char *));
  files[files_len] = NULL;

  int previous_return_value = *last_return_value;

  //traitement parallèle si défini
  if (loop->max_files > 0)
  {
    for (int i = 0; i < loop->max_files; i++)
    {
      if (fork() == 0)
      {
        exec_for_simple(loop, last_return_value, files, previous_return_value, original_dir);
        exit(0);
      }
    }
    while (wait(NULL) > 0);
  }
  else
  {
    exec_for_simple(loop, last_return_value, files, previous_return_value, original_dir);
  }
}

void exec_for_simple(for_loop *loop, int *last_return_value, char **files, int previous_return_value, char *original_dir)
{
  
  // Il faut maintenant exécuter les commandes pour chaque fichier
  for (int i = 0; files[i] != NULL; i++)
  {
    // Définir la variable d'environnement
    setenv(loop->variable, files[i], 1);

    execute_ast(loop->block, last_return_value);

    if (*last_return_value == 1 || sigint_received)
    {
      // Quitter la boucle si une commande a échoué
      // Libérer la mémoire allouée pour les fichiers
      for (int i = 0; files[i] != NULL; i++)
      {
        free(files[i]);
      }
      free(files);

      free(loop->dir);
      loop->dir = NULL;

      // Restaurer dir à sa valeur d'origine
      loop->dir = strdup(original_dir);

      // Libérer la mémoire allouée pour le chemin du répertoire
      free(original_dir);

      // Supprimer la variable d'environnement temporaire
      unsetenv(loop->variable);
      return;
    }
    signal_received = 0;

    // Faire le max entre le retour de la commande et le retour précédent
    if (*last_return_value > previous_return_value)
    {
      previous_return_value = *last_return_value;
    }
  }

  if (*last_return_value != 0)
  {
    *last_return_value = previous_return_value;
  }

  // Libérer la mémoire allouée pour les fichiers
  for (int i = 0; files[i] != NULL; i++)
  {
    free(files[i]);
  }
  free(files);

  free(loop->dir);
  loop->dir = NULL;

  // Restaurer dir à sa valeur d'origine
  loop->dir = strdup(original_dir);

  // Libérer la mémoire allouée pour le chemin du répertoire
  free(original_dir);

  // Supprimer la variable d'environnement temporaire
  unsetenv(loop->variable);
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

void execute_command(ast_node *node, int *last_return_value)
{
  command *cmd = &node->data.cmd;
  // Faire une copie des arguments pour pouvoir les modifier
  char **copy_args = malloc((cmd->argc + 1) * sizeof(char *));
  for (int i = 0; i < cmd->argc; i++)
  {
    copy_args[i] = strdup(cmd->args[i]);
  }
  copy_args[cmd->argc] = NULL;

  handle_substitution(cmd, copy_args, last_return_value);

  int saved_stdin = dup(STDIN_FILENO);
  int saved_stdout = dup(STDOUT_FILENO);
  int saved_stderr = dup(STDERR_FILENO);

  // Appliquer si elle existe plusieurs redirections sur la commande
  for (int i = 0; i < node->child_count; i++)
  {
    if (node->children[i]->type == NODE_REDIRECTION)
    {
      redirection *redir = &node->children[i]->data.redir;
      char *file_name = substitute_variables(redir->file, last_return_value);
      if (file_name == NULL)
      {
        restore_standard_fds(saved_stdin, saved_stdout, saved_stderr);
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

      int fd = open(file_name, redir->mode, 0664);
      free(file_name);
      if (fd == -1)
      {
        perror("open");
        *last_return_value = 1;
        restore_standard_fds(saved_stdin, saved_stdout, saved_stderr);
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

      if (redir->fd == STDOUT_FILENO)
      {
        dup2(fd, STDOUT_FILENO);
      }
      if (redir->fd == STDERR_FILENO)
      {
        dup2(fd, STDERR_FILENO);
      }
      if (redir->fd == STDIN_FILENO)
      {
        dup2(fd, STDIN_FILENO);
      }

      close(fd);
    }
  }

  if (strcmp(cmd->args[0], "exit") == 0)
  {
    // Vérifier que l'on a déjà pas un argument en trop (on accepte pas plus d'un argument)
    if (cmd->argc > 2)
    {
      fprintf(stderr, "exit: too many arguments\n");
      *last_return_value = 1;
      // Retablir les arguments originaux
      for (int i = 0; i < cmd->argc; i++)
      {
        free(cmd->args[i]);
        cmd->args[i] = strdup(copy_args[i]);
      }
      for (int i = 0; i < cmd->argc; i++)
      {
        free(copy_args[i]);
      }
      free(copy_args);
      return;
    }
    // Si on a un argument, c'est la valeur de retour
    if (cmd->argc == 2)
    {
      int code = atoi(cmd->args[1]);
      if (code > 255)
      {
        code %= 256; // Normaliser dans la plage [0, 255]
      }
      for (int i = 0; i < cmd->argc; i++)
      {
        free(cmd->args[i]);
        cmd->args[i] = strdup(copy_args[i]);
      }
      for (int i = 0; i < cmd->argc; i++)
      {
        free(copy_args[i]);
      }
      free(copy_args);
      cleanup_and_exit(code, node);
    }
    for (int i = 0; i < cmd->argc; i++)
    {
      free(cmd->args[i]);
      cmd->args[i] = strdup(copy_args[i]);
    }
    for (int i = 0; i < cmd->argc; i++)
    {
      free(copy_args[i]);
    }
    free(copy_args);
    cleanup_and_exit(*last_return_value, node);
  }
  else if (strcmp(cmd->args[0], "pwd") == 0)
  {
    if (cmd->argc != 1)
    {
      fprintf(stderr, "pwd: too many arguments\n");
      *last_return_value = 1;
      for (int i = 0; i < cmd->argc; i++)
      {
        free(cmd->args[i]);
        cmd->args[i] = strdup(copy_args[i]);
      }
      for (int i = 0; i < cmd->argc; i++)
      {
        free(copy_args[i]);
      }
      free(copy_args);
      return;
    }
    *last_return_value = pwd();
    for (int i = 0; i < cmd->argc; i++)
    {
      free(cmd->args[i]);
      cmd->args[i] = strdup(copy_args[i]);
    }
    for (int i = 0; i < cmd->argc; i++)
    {
      free(copy_args[i]);
    }
    free(copy_args);
  }
  else if (strcmp(cmd->args[0], "cd") == 0)
  {
    if (cmd->argc > 2)
    {
      fprintf(stderr, "cd: invalid number of arguments\n");
      *last_return_value = 1;
      for (int i = 0; i < cmd->argc; i++)
      {
        free(cmd->args[i]);
        cmd->args[i] = strdup(copy_args[i]);
      }
      for (int i = 0; i < cmd->argc; i++)
      {
        free(copy_args[i]);
      }
      free(copy_args);
      return;
    }
    *last_return_value = execute_cd(cmd->args[1]);
    for (int i = 0; i < cmd->argc; i++)
    {
      free(cmd->args[i]);
      cmd->args[i] = strdup(copy_args[i]);
    }
    for (int i = 0; i < cmd->argc; i++)
    {
      free(copy_args[i]);
    }
    free(copy_args);
  }
  else if (strcmp(cmd->args[0], "ftype") == 0)
  {
    *last_return_value = ftype(cmd->args[1], ".");
    for (int i = 0; i < cmd->argc; i++)
    {
      free(cmd->args[i]);
      cmd->args[i] = strdup(copy_args[i]);
    }
    for (int i = 0; i < cmd->argc; i++)
    {
      free(copy_args[i]);
    }
    free(copy_args);
  }
  else
  {
    pid_t pid = fork();
    if (pid == 0)
    {
      execvp(cmd->args[0], cmd->args);
      perror("execvp");
      exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
      perror("fork");
    }
    else
    {
      int status;
      waitpid(pid, &status, 0);
      if (WIFEXITED(status))
      {
        *last_return_value = WEXITSTATUS(status);
        for (int i = 0; i < cmd->argc; i++)
        {
          free(cmd->args[i]);
          cmd->args[i] = strdup(copy_args[i]);
        }
        for (int i = 0; i < cmd->argc; i++)
        {
          free(copy_args[i]);
        }
        free(copy_args);
        if (*last_return_value > 255)
        {
          *last_return_value %= 256; // Normaliser dans la plage [0, 255]
        }
      }
      // Regarder si on a recu sigint
      else if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
      {
        sigint_received = 1;
        signal_received = 1;
        for (int i = 0; i < cmd->argc; i++)
        {
          free(cmd->args[i]);
          cmd->args[i] = strdup(copy_args[i]);
        }
        for (int i = 0; i < cmd->argc; i++)
        {
          free(copy_args[i]);
        }
        free(copy_args);
      }
      else if (WIFSIGNALED(status) && WTERMSIG(status) != SIGINT){
        // Continuer le programme si on a reçu un signal autre que SIGINT
        signal_received = 1;
      }
      else
      {
        *last_return_value = 1; // Erreur par défaut
      }
    }
  }

  restore_standard_fds(saved_stdin, saved_stdout, saved_stderr);
}

void execute_pipeline(ast_node *pipeline, int *last_return_value)
{
  int nb_cmds = pipeline->child_count;
  int pipes[nb_cmds - 1][2];

  for (int i = 0; i < nb_cmds - 1; i++)
  {
    if (pipe(pipes[i]) < 0)
    {
      perror("pipe");
      *last_return_value = 1;
      return;
    }
  }

  for (int i = 0; i < nb_cmds; i++)
  {
    pid_t pid = fork();
    if (pid < 0)
    {
      perror("fork");
      *last_return_value = 1;
      return;
    }

    if (pid == 0)
    {
      if (i > 0)
      {
        if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0)
        {
          perror("dup2");
          exit(1);
        }
      }

      if (i < nb_cmds - 1)
      {
        if (dup2(pipes[i][1], STDOUT_FILENO) < 0)
        {
          perror("dup2");
          exit(1);
        }
      }

      for (int j = 0; j < nb_cmds - 1; j++)
      {
        close(pipes[j][0]);
        close(pipes[j][1]);
      }

      execute_ast(pipeline->children[i], last_return_value);
      if (signal_received == 1 && i < nb_cmds - 1)
      {
        signal_received = 0;
        *last_return_value = 0; // On n'arrête pas le pipeline si une commande a été interrompue
      }
      exit(*last_return_value);
    }
  }

  for (int i = 0; i < nb_cmds - 1; i++)
  {
    close(pipes[i][0]);
    close(pipes[i][1]);
  }

  int status;
  for (int i = 0; i < nb_cmds; i++)
  {
    wait(&status);
    if (WIFEXITED(status))
    {
      *last_return_value = WEXITSTATUS(status);
    }
  }
}

void execute_ast(ast_node *node, int *last_return_value)
{
  if (node == NULL)
  {
    return;
  }

  if (sigint_received)
  {
    return;
  }

  if (node->type == NODE_PIPELINE)
  {
    execute_pipeline(node, last_return_value);
  }

  if (node->type == NODE_IF)
  {
    // Vérifier la condition du if en exécutant sa commande et tous ses enfants
    int *if_return_value = malloc(sizeof(int));
    *if_return_value = 0;
    ast_node *condition = node->data.if_stmt.condition;
    // Exécuter la condition
    execute_ast(condition, if_return_value);
    // Récupérer le retour de la condition
    if (*if_return_value == 0)
    {
      // Si la condition est vraie, exécuter le bloc then
      execute_ast(node->data.if_stmt.then_block, last_return_value);
    }
    else
    {
      // Si la condition est fausse, exécuter le bloc else s'il existe
      if (node->data.if_stmt.else_block != NULL)
      {
        execute_ast(node->data.if_stmt.else_block, last_return_value);
      }
    }
    free(if_return_value);
  }

  if (node->type == NODE_FOR_LOOP)
  {
    execute_for(node, last_return_value);
  }

  if (node->type == NODE_COMMAND)
  {
    execute_command(node, last_return_value);
  }

  if (node->type != NODE_PIPELINE && sigint_received == 0)
  {
    for (int i = 0; i < node->child_count; i++)
    {
      if (sigint_received)
      {
        return;
      }
      signal_received = 0;
      execute_ast(node->children[i], last_return_value);
    }
  }
}

void handle_sigint(int sig)
{
  signal_received = 1;
  sigint_received = 1;
}

void handle_sigterm(int sig)
{
  if(in_exec){
    signal_received = 1;
  }
}

int main()
{
  int last_return_value = 0;
  char formatted_prompt[MAX_LENGTH_PROMPT];
  char *line;

  // Gerer les signaux avec sigaction
  struct sigaction sa;
  sa.sa_handler = handle_sigterm;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGTERM, &sa, NULL);
  sa.sa_handler = handle_sigint;
  sigaction(SIGINT, &sa, NULL);

  rl_outstream = stderr;

  while (1)
  {
    if (signal_received)
    {
      last_return_value = 255;
      signal_received = 0;
      sigint_received = 0;
      char *prompt_dir = malloc(MAX_LENGTH_PROMPT);
      char *current_dir = chemin_du_repertoire();
      if (current_dir == NULL)
      {
        perror("chemin_du_repertoire");
        return 1;
      }
      create_dir_prompt_name(prompt_dir, current_dir, last_return_value, 1);
      create_prompt(formatted_prompt, prompt_dir, last_return_value, 1);
      free(current_dir);
      free(prompt_dir);
    }
    else
    {
      sigint_received = 0;
      char *current_dir = chemin_du_repertoire();
      if (current_dir == NULL)
      {
        perror("chemin_du_repertoire");
        return 1;
      }

      char *prompt_dir = malloc(MAX_LENGTH_PROMPT);
      create_dir_prompt_name(prompt_dir, current_dir, last_return_value, 0);
      create_prompt(formatted_prompt, prompt_dir, last_return_value, 0);
      free(current_dir);
      free(prompt_dir);
    }

    in_exec = 0;
    line = readline(formatted_prompt);
    in_exec = 1;
    if (line == NULL)
    {
      exit(last_return_value);
    }

    char *cleaned_line = trim_and_reduce_spaces(line);
    if (strlen(cleaned_line) == 0)
    {
      free(line);
      free(cleaned_line);
      continue;
    }

    add_history(line);

    // Parser la ligne de commande
    ast_node *root = construct_ast(cleaned_line);
    free(cleaned_line);
    if (root == NULL)
    {
      last_return_value = 1;
      free(line);
      continue;
    }

    // Exécuter l'AST
    execute_ast(root, &last_return_value);

    // Libérer la mémoire
    free_ast_node(root);

    free(line);
  }

  return 0;
}

void create_dir_prompt_name(char *prompt, const char *current_dir, int last_return_value, int sigint_received)
{
  // Technique attroce pour calculer le nombre de chiffres dans un nombre...
  int number_of_digits = 1;
  if (last_return_value > 0)
  {
    int temp = last_return_value;
    number_of_digits = 0;
    while (temp > 0)
    {
      temp /= 10;
      number_of_digits++;
    }
  }
  if (sigint_received)
  {
    number_of_digits = 3;
    snprintf(prompt, MAX_LENGTH_PROMPT, "...%s", current_dir + strlen(current_dir) - 23 + number_of_digits);
    return;
  }
  else if (strlen(current_dir) > 23 - number_of_digits)
  {
    snprintf(prompt, MAX_LENGTH_PROMPT, "...%s", current_dir + strlen(current_dir) - 23 + number_of_digits);
  }
  else
  {
    snprintf(prompt, MAX_LENGTH_PROMPT, "%s", current_dir);
  }
}

void create_prompt(char *prompt, const char *current_dir, int last_return_value, int sigint_received)
{
  if (sigint_received)
  {
    snprintf(prompt, MAX_LENGTH_PROMPT, "[SIG]%s$ ", current_dir);
  }
  else if (last_return_value != 1)
  {
    snprintf(prompt, MAX_LENGTH_PROMPT, "[%d]%s$ ", last_return_value,
             current_dir);
  }
  else
  {
    snprintf(prompt, MAX_LENGTH_PROMPT, "[%s%d%s]%s$ ", RED_COLOR,
             last_return_value, RESET_COLOR, current_dir);
  }
}

void restore_standard_fds(int saved_stdin, int saved_stdout, int saved_stderr)
{
  dup2(saved_stdin, STDIN_FILENO);
  dup2(saved_stdout, STDOUT_FILENO);
  dup2(saved_stderr, STDERR_FILENO);
  close(saved_stdin);
  close(saved_stdout);
  close(saved_stderr);
}

void cleanup_and_exit(int last_return_value, ast_node *tree)
{
  free_ast_node(tree);
  exit(last_return_value);
}