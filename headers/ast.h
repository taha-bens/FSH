#pragma once

#include "string_util.h"
#include "node.h"

ast_node *parse_for_loop(char **tokens, int *index);
ast_node *parse_if(char **tokens, int *index);
ast_node *check_redirection(char **tokens, int *index, ast_node *node);
ast_node *parse_command(char **tokens, int *index);
// Fonction principale pour construire l'AST
ast_node *construct_ast(char *line);
// Fonction récursive pour construire l'AST
ast_node *construct_ast_rec(char **tokens, int *index);
void handle_substitution(command *cmd, char **copy_args, int *last_return_value);


