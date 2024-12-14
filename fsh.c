#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "headers/pwd.h"
#include "headers/ftype.h"
#include "headers/cd.h"
#include "headers/for.h"

#define RESET_COLOR "\033[00m"
#define RED_COLOR "\033[91m"
#define MAX_LENGTH_PROMPT 30 + sizeof(RED_COLOR) + sizeof(RESET_COLOR)
#define SIZE_PWDBUF 1024
#define PATH_MAX 4096

typedef struct ast_node ast_node;

char **str_split(char *a_str, const char a_delim);
void free_split(char **splited);
char *trim_and_reduce_spaces(const char *str);
void create_dir_prompt_name(char *prompt, const char *current_dir, int last_return_value);
void create_prompt(char *prompt, const char *current_dir, int last_return_value);
int handle_redirections(char **splited, int *last_return_value);
void restore_standard_fds(int saved_stdin, int saved_stdout, int saved_stderr);
void cleanup_and_exit(int last_return_value, ast_node *tree);

typedef enum node_type
{
  NODE_COMMAND,     // Une commande simple
  NODE_PIPELINE,    // Un pipeline de commandes
  NODE_FOR_LOOP,    // Une boucle for avec des commandes
  NODE_REDIRECTION, // Une redirection de fichier
  NODE_IF,          // Une condition if
  NODE_SEQUENCE     // Une séquence de commandes (séparées par des points-virgules)
} node_type;

typedef struct command
{
  char *name;
  char **args;
  int argc;
} command;

typedef struct pipeline
{
  command **commands;
  int nb_commands;
} pipeline;

typedef struct redirection
{
  char *file;
  int fd;
  int mode;
} redirection;

typedef struct if_statement
{
  char *condition;
  ast_node *then_block;
  ast_node *else_block;
} if_statement;

typedef struct for_loop
{
  char *dir;
  char *variable;
  char **options;
  ast_node *block;
} for_loop;

typedef struct sequence
{
  command **commands;
  int nb_commands;
} sequence;

typedef struct ast_node
{
  node_type type;             // Type du nœud
  struct ast_node **children; // Enfants (par exemple, commandes d'un pipeline ou boucle)
  int child_count;            // Nombre d'enfants
  union
  {
    command cmd;
    pipeline pipe;
    redirection redir;
    if_statement if_stmt;
    for_loop for_loop;
  } data;
} ast_node;

// Crée un nœud d'AST
ast_node *create_ast_node(node_type type, char *value)
{
  ast_node *node = malloc(sizeof(ast_node));
  node->type = type;
  node->children = NULL;
  node->child_count = 0;
  memset(&node->data, 0, sizeof(node->data));
  return node;
}

ast_node *create_command_node(char *name, char **args, int argc)
{
  ast_node *node = create_ast_node(NODE_COMMAND, name);
  node->data.cmd.name = strdup(name);
  node->data.cmd.args = malloc((argc + 1) * sizeof(char *)); // NULL terminaison

  for (int i = 0; i < argc; i++)
  {
    node->data.cmd.args[i] = strdup(args[i]);
  }
  node->data.cmd.args[argc] = NULL; 
  node->data.cmd.argc = argc;

  return node;
}

ast_node *create_pipeline_node(command **commands, int nb_commands)
{
  ast_node *node = create_ast_node(NODE_PIPELINE, "|");
  node->data.pipe.commands = malloc(nb_commands * sizeof(command *));
  for (int i = 0; i < nb_commands; i++)
  {
    node->data.pipe.commands[i] = commands[i];
  }
  node->data.pipe.nb_commands = nb_commands;
  return node;
}

ast_node *create_if_node(char *condition, ast_node *then_block, ast_node *else_block)
{
  ast_node *node = create_ast_node(NODE_IF, condition);
  node->data.if_stmt.condition = strdup(condition);
  node->data.if_stmt.then_block = then_block;
  node->data.if_stmt.else_block = else_block;
  return node;
}
ast_node *create_for_node(char *dir, char *variable, ast_node *block)
{
  ast_node *node = create_ast_node(NODE_FOR_LOOP, "for");
  node->data.for_loop.dir = strdup(dir);
  node->data.for_loop.variable = strdup(variable);
  node->data.for_loop.block = block;
  return node;
}

ast_node *create_redirection_node(char *file, int fd, int mode)
{
  ast_node *node = create_ast_node(NODE_REDIRECTION, file);
  node->data.redir.file = strdup(file);
  node->data.redir.fd = fd;
  node->data.redir.mode = mode;
  return node;
}

// Ajoute un enfant à un nœud
void add_child(ast_node *parent, ast_node *child)
{
  parent->children = realloc(parent->children, (parent->child_count + 1) * sizeof(ast_node *));
  parent->children[parent->child_count++] = child;
}

void free_command(command *cmd)
{
  free(cmd->name);
  for (int i = 0; i < cmd->argc; i++)
  {
    free(cmd->args[i]);
  }
  free(cmd->args);
  free(cmd);
}

// Libère un nœud d'AST
void free_ast_node(ast_node *node)
{
  if (!node)
    return;

  // Libération des enfants
  for (int i = 0; i < node->child_count; i++)
  {
    free_ast_node(node->children[i]);
  }
  free(node->children);

  // Libération des ressources spécifiques
  switch (node->type)
  {
  case NODE_COMMAND:
    free(node->data.cmd.name);
    for (int i = 0; i < node->data.cmd.argc; i++)
    {
      free(node->data.cmd.args[i]);
    }
    free(node->data.cmd.args);
    break;
  case NODE_REDIRECTION:
    free(node->data.redir.file);
    break;
  case NODE_FOR_LOOP:
    free(node->data.for_loop.dir);
    free(node->data.for_loop.variable);
    break;
  case NODE_IF:
    free(node->data.if_stmt.condition);
    free_ast_node(node->data.if_stmt.then_block);
    free_ast_node(node->data.if_stmt.else_block);
    break;
  default:
    break;
  }
  free(node);
}

pipeline *create_pipeline(command **commands, int nb_commands)
{
  pipeline *pipe = malloc(sizeof(pipeline));
  pipe->commands = commands;
  pipe->nb_commands = nb_commands;
  return pipe;
}

void free_pipeline(pipeline *pipe)
{
  for (int i = 0; i < pipe->nb_commands; i++)
  {
    free_command(pipe->commands[i]);
  }
  free(pipe->commands);
  free(pipe);
}

redirection *create_redirection(char *file, int fd, int mode)
{
  redirection *red = malloc(sizeof(redirection));
  red->file = file;
  red->fd = fd;
  red->mode = mode;
  return red;
}

void free_redirection(redirection *red)
{
  free(red->file);
  free(red);
}

// Fonction récursive pour construire l'AST
ast_node *construct_ast_recursive(char **tokens, int *index)
{
  ast_node *node = NULL;

  while (tokens[*index] != NULL)
  {
    if (strcmp(tokens[*index], ";") == 0)
    {
      (*index)++;
      break;
    }
    else if (strcmp(tokens[*index], "for") == 0)
    {
      (*index)++;
      char *dir = tokens[(*index)++];
      char *variable = tokens[(*index)++];
      ast_node *block = construct_ast_recursive(tokens, index);
      node = create_for_node(dir, variable, block);
    }
    else if (strcmp(tokens[*index], "if") == 0)
    {
      (*index)++;
      char *condition = tokens[(*index)++];
      ast_node *then_block = construct_ast_recursive(tokens, index);
      ast_node *else_block = NULL;
      if (tokens[*index] && strcmp(tokens[*index], "else") == 0)
      {
        (*index)++;
        else_block = construct_ast_recursive(tokens, index);
      }
      node = create_if_node(condition, then_block, else_block);
    }
    else
    {
      char *name = tokens[(*index)++];
      char **args = &tokens[*index];
      int argc = 0;
      while (tokens[*index] && strcmp(tokens[*index], ";") != 0)
      {
        (*index)++;
        argc++;
      }
      node = create_command_node(name, args, argc);
    }
  }

  return node;
}

// Fonction principale pour construire l'AST
ast_node *construct_ast(char *line)
{
  char **tokens = str_split(line, ' ');
  int index = 0;
  ast_node *root = create_ast_node(NODE_SEQUENCE, ";");

  while (tokens[index] != NULL)
  {
    ast_node *child = construct_ast_recursive(tokens, &index);
    if (child)
    {
      add_child(root, child);
    }
  }

  free_split(tokens);
  return root;
}

void execute_ast(ast_node *node, int *last_return_value)
{
  if (node == NULL)
  {
    return;
  }

  for (int i = 0; i < node->child_count; i++)
  {
    execute_ast(node->children[i], last_return_value);
  }

  if (node->type == NODE_COMMAND)
  {
    // Exécuter la commande
    command *cmd = &node->data.cmd;
    if (strcmp(cmd->name, "exit") == 0)
    {
      // Vérifier que l'on a déjà pas un argument en trop (on accepte pas plus d'un argument)
      if (cmd->argc > 1)
      {
        fprintf(stderr, "exit: too many arguments\n");
        *last_return_value = 1;
        return;
      }
      // Si on a un argument, c'est la valeur de retour
      if (cmd->argc == 1)
      {
        int code = atoi(cmd->args[0]);
        if (code > 255)
        {
          code %= 256; // Normaliser dans la plage [0, 255]
        }
        cleanup_and_exit(code, node);
      }
      cleanup_and_exit(*last_return_value, node);
    }
    else if (strcmp(cmd->name, "pwd") == 0)
    {
      if (cmd->argc != 0)
      {
        fprintf(stderr, "pwd: too many arguments\n");
        *last_return_value = 1;
        return;
      }
      *last_return_value = pwd();
    }
    else if (strcmp(cmd->name, "cd") == 0)
    {
      if (cmd->argc > 1)
      {
        fprintf(stderr, "cd: invalid number of arguments\n");
        *last_return_value = 1;
        return;
      }
      *last_return_value = execute_cd(cmd->args[0]);
    }
    else if (strcmp(cmd->name, "ftype") == 0)
    {
      *last_return_value = ftype(cmd->args[0], ".");
    }
    else if (strcmp(cmd->name, "for") == 0)
    {
      *last_return_value = execute_for(cmd->args);
    }
    else
    {
      pid_t pid = fork();
      if (pid == 0)
      {
        execvp(cmd->name, cmd->args);
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
          if (*last_return_value > 255)
          {
            *last_return_value %= 256; // Normaliser dans la plage [0, 255]
          }
        }
        else
        {
          *last_return_value = 1; // Erreur par défaut
        }
      }
    }
  }
}

int main()
{
  int last_return_value = 0;
  char formatted_prompt[MAX_LENGTH_PROMPT];
  char *line;

  rl_outstream = stderr;

  while (1)
  {
    char *current_dir = chemin_du_repertoire();
    if (current_dir == NULL)
    {
      perror("chemin_du_repertoire");
      return 1;
    }

    char *prompt_dir = malloc(MAX_LENGTH_PROMPT);
    create_dir_prompt_name(prompt_dir, current_dir, last_return_value);
    create_prompt(formatted_prompt, prompt_dir, last_return_value);
    free(current_dir);
    free(prompt_dir);

    line = readline(formatted_prompt);
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

    // Parse the command and build the AST
    ast_node *root = construct_ast(cleaned_line);
    free(cleaned_line);

    // Execute the AST
    execute_ast(root, &last_return_value);

    // Free the AST
    free_ast_node(root);

    free(line);
  }

  return 0;
}

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

void create_dir_prompt_name(char *prompt, const char *current_dir, int last_return_value)
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
  if (strlen(current_dir) > 23 - number_of_digits)
  {
    snprintf(prompt, MAX_LENGTH_PROMPT, "...%s", current_dir + strlen(current_dir) - 23 + number_of_digits);
  }
  else
  {
    snprintf(prompt, MAX_LENGTH_PROMPT, "%s", current_dir);
  }
}

void create_prompt(char *prompt, const char *current_dir, int last_return_value)
{
  if (last_return_value != 1)
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

int handle_redirections(char **splited, int *last_return_value)
{
  for (int i = 0; splited[i] != NULL; i++)
  {
    char *redirection = NULL;
    int fd = -1;

    // Vérifier les redirections avec des descripteurs spécifiques
    if (strstr(splited[i], ">") != NULL || strstr(splited[i], "<") != NULL)
    {
      if (isdigit(splited[i][0]))
      {
        fd = atoi(splited[i]);
        redirection = &splited[i][1];
      }
      else
      {
        fd = (strstr(splited[i], "<") != NULL) ? STDIN_FILENO : STDOUT_FILENO;
        redirection = splited[i];
      }
    }

    if (redirection != NULL && splited[i + 1] != NULL)
    {
      int file_fd = -1;
      if (strcmp(redirection, "<") == 0)
      {
        file_fd = open(splited[i + 1], O_RDONLY);
        if (file_fd == -1)
        {
          perror("open");
          *last_return_value = 1;
          return -1;
        }
      }
      else if (strcmp(redirection, ">") == 0)
      {
        if (access(splited[i + 1], F_OK) == 0)
        {
          fprintf(stderr, "pipeline_run: File exists\n");
          *last_return_value = 1;
          return -1;
        }
        file_fd = open(splited[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0664);
        if (file_fd == -1)
        {
          perror("open");
          *last_return_value = 1;
          return -1;
        }
      }
      else if (strcmp(redirection, ">>") == 0)
      {
        file_fd = open(splited[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0664);
        if (file_fd == -1)
        {
          perror("open");
          *last_return_value = 1;
          return -1;
        }
      }
      else if (strcmp(redirection, ">|") == 0)
      {
        file_fd = open(splited[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0664);
        if (file_fd == -1)
        {
          perror("open");
          *last_return_value = 1;
          return -1;
        }
      }

      // Configurer la redirection
      if (file_fd != -1)
      {
        dup2(file_fd, fd);
        close(file_fd);
      }

      // Supprimer les redirections des arguments
      free(splited[i]);
      free(splited[i + 1]);
      splited[i] = NULL;

      // Supprimer les redirections des arguments
      for (int j = i; splited[j - 1] != NULL; j++)
      {
        splited[j] = splited[j + 2];
      }
      i--;
    }
  }

  return 0;
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