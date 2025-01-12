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
#include "headers/ast.h"
#include "headers/execution.h"


#define SIZE_PWDBUF 1024
#define PATH_MAX 4096

volatile sig_atomic_t signal_received = 0;
volatile sig_atomic_t sigint_received = 0;
volatile sig_atomic_t in_exec = 0;

void handle_sigint(int sig)
{
  signal_received = 1;
  sigint_received = 1;
}

void handle_sigterm(int sig)
{
  if (in_exec)
  {
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

