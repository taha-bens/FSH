#ifndef NODE_H
#define NODE_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Enumération des types de nœuds
typedef enum node_type
{
  NODE_COMMAND,     // Une commande simple
  NODE_PIPELINE,    // Un pipeline de commandes
  NODE_FOR_LOOP,    // Une boucle for avec des commandes
  NODE_REDIRECTION, // Une redirection de fichier
  NODE_IF,          // Une condition if
  NODE_SEQUENCE     // Une séquence de commandes (séparées par des points-virgules)
} node_type;

// Structure pour une commande
typedef struct command
{
  char **args;
  int argc;
} command;

// Structure pour un pipeline
typedef struct pipeline
{
  command **commands;
  int nb_commands;
} pipeline;

// Structure pour une redirection
typedef struct redirection
{
  char *file;
  int fd;
  int mode;
} redirection;

// Structure pour une condition if
typedef struct if_statement
{
  struct ast_node *condition;
  struct ast_node *then_block;
  struct ast_node *else_block;
} if_statement;

// Structure pour une boucle for
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
  struct ast_node *block;
} for_loop;

// Structure pour un nœud d'AST
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

// Fonctions pour créer des nœuds d'AST
ast_node *create_ast_node(node_type type, char *value);
ast_node *create_command_node(char **args, int argc);
ast_node *create_pipeline_node(command **commands, int nb_commands);
ast_node *create_if_node(ast_node *condition, ast_node *then_block, ast_node *else_block);
ast_node *create_for_node(char *dir, char *variable, char **options, int show_all, int recursive, char *ext, char *type, int max_files, ast_node *block);
ast_node *create_redirection_node(char *file, int fd, int mode);

// Fonction pour ajouter un enfant à un nœud
void add_child(ast_node *parent, ast_node *child);

// Fonctions pour libérer des nœuds d'AST
void free_command(command *cmd);
void free_pipeline(pipeline *pipe);
void free_ast_node(ast_node *node);

// Fonctions pour créer et libérer des structures spécifiques
pipeline *create_pipeline(command **commands, int nb_commands);
redirection *create_redirection(char *file, int fd, int mode);
void free_redirection(redirection *red);

#endif // NODE_H