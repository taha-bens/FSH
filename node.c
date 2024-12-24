# include <stdlib.h>
# include <string.h>
# include <stdbool.h>

typedef struct ast_node ast_node;

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


typedef struct redirection
{
  char *file;
  int fd;
  int mode;
} redirection;

typedef struct if_statement
{
  ast_node *condition;
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

ast_node *create_pipeline_node(ast_node *first, ast_node *second)
{
  ast_node *node = create_ast_node(NODE_PIPELINE, "|");
  node->children = malloc(2 * sizeof(ast_node *));
  node->children[0] = first;
  node->children[1] = second;
  node->child_count = 2;
  return node;
}

ast_node *create_if_node(ast_node *condition, ast_node *then_block, ast_node *else_block)
{
  ast_node *node = create_ast_node(NODE_IF, "IF");
  node->data.if_stmt.condition = condition;
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
  node->data.for_loop.ext = ext; // Aliasing mais bon on s'en fout
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
    free_ast_node(node->data.if_stmt.condition);
    node->data.if_stmt.condition = NULL;
    free_ast_node(node->data.if_stmt.then_block);
    node->data.if_stmt.then_block = NULL;
    free_ast_node(node->data.if_stmt.else_block);
    node->data.if_stmt.else_block = NULL;
    break;
  default:
    break;
  }
  free(node);
  node = NULL;
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
