# FSH Shell 

### Architecture du projet
On récupère une commande de l'utilisateur, on la découpe en tokens, on la parse pour obtenir un arbre de syntaxe abstraite (AST) et on l'exécute.

L'éxecution de la commande se fait en parcourant l'arbre de syntaxe abstraite (AST) et en exécutant les commandes à chaque nœud en ordre préfixe.

### Structure de données

1. **Commande Structuré**
```c
// Structure pour une commande
typedef struct command
{
  char **args;
  int argc;
} command;
```

2. **Nœud**
```c
// Structure pour un nœud d'AST
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
}
```
