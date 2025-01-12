#pragma once

void exec_for_simple(for_loop *loop, int *last_return_value, char **files, int previous_return_value, char *original_dir);
void exec_for_file(for_loop *loop, int *last_return_value, char *file, char **files, int previous_return_value, char *original_dir);
void execute_for(ast_node *node, int *last_return_value);
void execute_pipeline(ast_node *pipeline, int *last_return_value);
void execute_ast(ast_node *node, int *last_return_value);
void execute_command(ast_node *node, int *last_return_value);
void restore_standard_fds(int saved_stdin, int saved_stdout, int saved_stderr);
void cleanup_and_exit(int last_return_value, ast_node *tree);
void create_dir_prompt_name(char *prompt, const char *current_dir, int last_return_value, int sigint_received);
void create_prompt(char *prompt, const char *current_dir, int last_return_value, int sigint_received);


