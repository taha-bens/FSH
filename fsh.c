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
  int show_all;
  int recursive;
  char *ext;
  char *type;
  int max_files;
  ast_node *block;
} for_loop;
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

ast_node *create_command_node(char **args, int argc)
{
  if (argc == 0 || args == NULL)
  {
    return NULL;
  }
  ast_node *node = create_ast_node(NODE_COMMAND, args[0]);
  size_t size = argc * sizeof(char *);
  node->data.cmd.args = malloc(size + sizeof(char *));
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

// Ajoute un enfant à un nœud
void add_child(ast_node *parent, ast_node *child)
{
  parent->children = realloc(parent->children, (parent->child_count + 1) * sizeof(ast_node *));
  parent->children[parent->child_count++] = child;
}

ast_node *create_for_node(char *dir, char *variable, char **options, int show_all, int recursive, char *ext, char *type, int max_files, ast_node *block)
{
  ast_node *node = create_ast_node(NODE_FOR_LOOP, "for");
  node->data.for_loop.dir = strdup(dir);
  node->data.for_loop.variable = strdup(variable);
  node->data.for_loop.options = options;
  node->data.for_loop.show_all = show_all;
  node->data.for_loop.recursive = recursive;
  node->data.for_loop.ext = ext ? strdup(ext) : NULL;
  node->data.for_loop.type = type ? strdup(type) : NULL;
  node->data.for_loop.max_files = max_files;
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

void free_command(command *cmd)
{
  for (int i = 0; i < cmd->argc; i++)
  {
    free(cmd->args[i]);
  }
  free(cmd->args);
  free(cmd);
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

// Libère un nœud d'AST
void free_ast_node(ast_node *node)
{
  if (!node)
    return;

  // Libération des enfants
  for (int i = 0; i < node->child_count; i++)
  {
    free_ast_node(node->children[i]);
    node->children[i] = NULL;
  }
  free(node->children);
  node->children = NULL;

  // Libération des ressources spécifiques
  switch (node->type)
  {
  case NODE_COMMAND:
    for (int i = 0; i < node->data.cmd.argc; i++)
    {
      free(node->data.cmd.args[i]);
      node->data.cmd.args[i] = NULL;
    }
    free(node->data.cmd.args);
    node->data.cmd.args = NULL;
    break;
  case NODE_REDIRECTION:
    free(node->data.redir.file);
    node->data.redir.file = NULL;
    break;
  case NODE_FOR_LOOP:
    free(node->data.for_loop.dir);
    node->data.for_loop.dir = NULL;
    free(node->data.for_loop.variable);
    node->data.for_loop.variable = NULL;
    for (int i = 0; node->data.for_loop.options && node->data.for_loop.options[i]; i++)
    {
      free(node->data.for_loop.options[i]);
      node->data.for_loop.options[i] = NULL;
    }
    free(node->data.for_loop.options);
    node->data.for_loop.options = NULL;
    free(node->data.for_loop.ext);
    node->data.for_loop.ext = NULL;
    free(node->data.for_loop.type);
    node->data.for_loop.type = NULL;
    free_ast_node(node->data.for_loop.block);
    node->data.for_loop.block = NULL;
    break;
  case NODE_IF:
    free(node->data.if_stmt.condition);
    node->data.if_stmt.condition = NULL;
    free_ast_node(node->data.if_stmt.then_block);
    node->data.if_stmt.then_block = NULL;
    free_ast_node(node->data.if_stmt.else_block);
    node->data.if_stmt.else_block = NULL;
    break;
  case NODE_PIPELINE:
    free_pipeline(&node->data.pipe);
    node->data.pipe.commands = NULL;
    break;
  default:
    break;
  }
  free(node);
  node = NULL;
}

pipeline *create_pipeline(command **commands, int nb_commands)
{
  pipeline *pipe = malloc(sizeof(pipeline));
  pipe->commands = commands;
  pipe->nb_commands = nb_commands;
  return pipe;
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
    if (strcmp(tokens[*index], "}") == 0)
    {
      (*index)++;
      // On a fini le bloc for il faut quitter la récursion
      break;
    }
    else if (strcmp(tokens[*index], ";") == 0)
    {
      (*index)++;
      break;
    }
    else if (strcmp(tokens[*index], "for") == 0)
    {
      // Vérifier la syntaxe le premier argument est nécessairement le nom de la variable
      if (tokens[*index + 1] == NULL || tokens[*index + 2] == NULL || tokens[*index + 3] == NULL || tokens[*index + 4] == NULL)
      {
        write(STDERR_FILENO, "for: Invalid syntax\n", 21);
        return NULL;
      }
      // Aller chercher le nom de la variable et le répertoire qui suit
      char *variable = malloc(strlen(tokens[*index + 1]) + 2);
      strcpy(variable, tokens[*index + 1]);
      (*index)++;
      (*index)++; // On saute le in
      char *dir = tokens[(*index + 1)];

      // Initialiser les options
      int show_all = 0;
      int recursive = 0;
      char *ext = NULL;
      char *type = NULL;
      int max_files = 0;
      char **options = NULL;
      int options_len = 0;

      // Traiter les options
      while (tokens[*index] && tokens[*index][0] == '-')
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
          ext = tokens[++(*index)];
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

      // On a fini de traiter les options si les options sont vides on les libère
      if (options_len == 0)
      {
        free(options);
        options = NULL;
        ++(*index);
      }

      (*index)++;
      // Une fois les options traitées, vérifier qu'il existe un bloc entre accolades
      if (tokens[*index] == NULL || strcmp(tokens[*index], "{") != 0)
      {
        fprintf(stderr, "for: Invalid syntax\n");
        free(variable);
        return NULL;
      }
      (*index)++;
      ast_node *block = construct_ast_recursive(tokens, index);
      (*index)--;
      int multiple_commands = 0;
      // Si il y a un point virgule c'est qu'il y a une autre commande après le bloc et on continue la récursion
      while (tokens[*index] && strcmp(tokens[*index], ";") == 0)
      {
        multiple_commands = 1;
        (*index)++;
        ast_node *child = construct_ast_recursive(tokens, index);
        if (child != NULL)
        {
          add_child(block, child);
        }
        else
        {
          free_ast_node(block);
          free(variable);
          return NULL;
        }
      }

      if (multiple_commands)
      {
        (*index)--;
      }
      if (tokens[*index] == NULL || strcmp(tokens[*index], "}") != 0)
      {
        fprintf(stderr, "for: Invalid syntax\n");
        free_ast_node(block);
        free(variable);
        return NULL;
      }
      node = create_for_node(dir, variable, options, show_all, recursive, ext, type, max_files, block);
      free(variable);
    }
    else if (strcmp(tokens[*index], "if") == 0)
    {
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
      int argc = 0;

      // Compter les arguments
      int start_index = *index;
      // SAINTE MERE DE DIEU C'EST HORRIBLE
      while (tokens[*index] != NULL && (strcmp(tokens[*index], ";") != 0 && strcmp(tokens[*index], "}") != 0 && strcmp(tokens[*index], "{") != 0 && strcmp(tokens[*index], "|") != 0 && strcmp(tokens[*index], ">") != 0 && strcmp(tokens[*index], "<") != 0 && strcmp(tokens[*index], ">>") != 0 && strcmp(tokens[*index], ">|") != 0))
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
      node = create_command_node(args, argc);

      // Libérer les arguments
      for (int i = 0; i < argc; i++)
      {
        free(args[i]);
      }
      free(args);
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
  ast_node *root = create_ast_node(NODE_SEQUENCE, ";");

  while (tokens[index] != NULL)
  {
    ast_node *child = construct_ast_recursive(tokens, &index);
    if (child != NULL)
    {
      add_child(root, child);
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

void execute_ast(ast_node *node, int *last_return_value)
{
  if (node == NULL)
  {
    return;
  }

  if (node->type == NODE_FOR_LOOP)
  {
    for_loop *loop = &node->data.for_loop;
    DIR *dir = opendir(loop->dir);
    if (dir == NULL)
    {
      perror("opendir");
      *last_return_value = 1;
      return;
    }

    struct dirent *entry;
    int file_count = 0;

    // Parcourir les fichiers du répertoire
    while ((entry = readdir(dir)) != NULL)
    {
      // Ignorer les fichiers cachés si show_all n'est pas activé
      if (entry->d_name[0] == '.' && !loop->show_all)
        continue;

      // Créer le chemin complet du fichier
      char file_path[PATH_MAX];
      snprintf(file_path, PATH_MAX, "%s/%s", loop->dir, entry->d_name);

      // Vérification des options (extension, type, etc.)
      struct stat file_stat;
      if (stat(file_path, &file_stat) == -1)
      {
        perror("stat");
        continue;
      }

      // Filtrer les fichiers par extension
      if (loop->ext && !strstr(entry->d_name, loop->ext))
        continue;

      // Filtrer les fichiers par type
      if (loop->type)
      {
        if ((strcmp(loop->type, "file") == 0 && !S_ISREG(file_stat.st_mode)) ||
            (strcmp(loop->type, "dir") == 0 && !S_ISDIR(file_stat.st_mode)))
          continue;
      }

      // Limiter le nombre de fichiers si max_files est défini
      if (loop->max_files > 0 && file_count >= loop->max_files)
        break;

      // Ajouter une variable d'environnement temporaire pour la variable du `for
      setenv(loop->variable, file_path, 1);

      // Exécuter le bloc de commandes avec la variable remplacée
      execute_ast(loop->block, last_return_value);

      // Compteur de fichiers traités
      file_count++;
    }

    // Supprimer la variable d'environnement temporaire
    unsetenv(loop->variable);
    closedir(dir);
  }

  if (node->type == NODE_COMMAND)
  {
    // Exécuter la commande
    command *cmd = &node->data.cmd;

    // Faire une copie des arguments pour pouvoir les modifier
    char **copy_args = malloc((cmd->argc + 1) * sizeof(char *));
    for (int i = 0; i < cmd->argc; i++)
    {
      copy_args[i] = strdup(cmd->args[i]);
    }
    copy_args[cmd->argc] = NULL;

    // Regarder dans args si on a pas une substitution de variable à faire
    for (int i = 0; i < cmd->argc; i++)
    {
      if (cmd->args[i][0] == '$')
      {
        char *var = getenv(cmd->args[i] + 1);
        if (var != NULL)
        {
          free(cmd->args[i]);
          cmd->args[i] = strdup(var);
        }
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
        for (int i = 0; i < cmd->argc; i++)
        {
          printf("%s ", cmd->args[i]);
        }
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
        else
        {
          *last_return_value = 1; // Erreur par défaut
        }
      }
    }
  }

  for (int i = 0; i < node->child_count; i++)
  {
    execute_ast(node->children[i], last_return_value);
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

    int saved_stdin = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stderr = dup(STDERR_FILENO);

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

    // Parser la ligne de commande
    ast_node *root = construct_ast(cleaned_line);
    free(cleaned_line);
    if (root == NULL)
    {
      last_return_value = 2;
      free(line);
      continue;
    }

    // Exécuter l'AST
    execute_ast(root, &last_return_value);

    // Restaurer les descripteurs de fichiers standard
    restore_standard_fds(saved_stdin, saved_stdout, saved_stderr);

    // Libérer la mémoire
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