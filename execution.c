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

#include "headers/node.h"
#include "headers/stack_dir.h"
#include "headers/string_util.h"
#include "headers/pwd.h"
#include "headers/ftype.h"
#include "headers/cd.h"
#include "headers/ast.h"

#include "headers/execution.h"

extern volatile sig_atomic_t signal_received;
extern volatile sig_atomic_t sigint_received;
extern volatile sig_atomic_t in_exec;

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

void exec_for_file(for_loop *loop, int *last_return_value, char *file, char **files, int previous_return_value, char *original_dir)
{
  // Définir la variable d'environnement
  setenv(loop->variable, file, 1);

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

  // Traitement parallèle des fichiers
  if (loop->max_files > 0)
  {
    for (int i = 0; i < loop->max_files; i++)
    {
      if(files[i] == NULL)
      {
        break;
      }
      if (fork() == 0)
      {
        exec_for_file(loop, last_return_value, files[i], files, previous_return_value, original_dir);
        exit(*last_return_value);
      }
    }
    // Attendre tous les processus enfants
    int status; int ret;
    for (int i = 0; i < loop->max_files; i++)
    {
      ret = waitpid(-1, &status, 0);
      if (ret < 0)
      {
        // Eviter un processus zombie
        wait(NULL);
      }
      if (WIFEXITED(status))
      {
        if (WEXITSTATUS(status) > *last_return_value)
        {
          *last_return_value = WEXITSTATUS(status);
        }
      }
    }
  }
  else
  {
    exec_for_simple(loop, last_return_value, files, previous_return_value, original_dir);
  }
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
    else {
      wait(NULL); // Eviter le zombie en cas d'interruption
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
      int ret = 0;
      ret = waitpid(pid, &status, 0);
      if (ret < 0)
      {
        // Eviter un processus zombie
        wait(NULL);
      }
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
      else if (WIFSIGNALED(status) && WTERMSIG(status) != SIGINT)
      {
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


