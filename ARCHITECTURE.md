# FSH Shell 

### Architecture du projet
Le projet est divisé en plusieurs fichiers sources, chacun contenant des fonctions pour une tâche spécifique.

Voici comment fonctionne brièvement le shell.

On récupère une commande de l'utilisateur, on la découpe en tokens grâce aux espaces, on la parse pour obtenir un arbre de syntaxe abstraite (AST) et on l'exécute.
Le parsing se fait grâce à une fonction récursive qui pour chaque token déduit le type de nœud et le créeant et l'ajoutant en tant qu'enfant du nœud courant.
Les fonctions de parsing sont situées dans le fichier `ast.c`.

L'éxecution de la commande se fait en parcourant l'arbre de syntaxe abstraite (AST) et en exécutant les commandes à chaque nœud en ordre préfixe.
Plusieurs fonctions d'exécution sont définies pour chaque type de nœud pour traiter les commandes, les redirections, les boucles, les conditions.
Toutes ces fonctions sont situées dans le fichier `executions.c`.

Pour la recursion dans la boucle for cad pour traiter tous les sous-dossiers et fichiers d'un dossier, on utilise une pile pour stocker les dossiers à traiter. Son implémentation est située dans le fichier `stack_dir.c`.

Pour la manipulation de strings, surtout pour le découpage en tokens ou encore la subsitution de variables à partir des espaces nous avons réalisé nos propres fonctions dans le fichier `string_utils.c`.

En ce qui concerne la subsitution de variables pour la boucle for on a utilisé setenv et getenv pour stocker et récupérer les variables d'environnement directement dans l'exécution de la commande.

Nos commandes internes ne sont pas forkées, elles sont exécutées dans le processus du shell. Pour les commandes externes, on fork et on exécute la commande dans le processus fils, le shell attend ensuite la fin de l'exécution de la comande pour poursuivre.