# Projet : programmation d'un interpréteur de commandes

**L3 Informatique - Système**

Il est important de bien lire le sujet jusqu'au bout et de bien y
réfléchir avant de se lancer dans la programmation du projet.

## Sujet : `fsh`, un petit shell avec des boucles `for`

Le but du projet est de programmer un interpréteur de commandes (aka
*shell*) interactif reprenant quelques fonctionnalités plus ou moins
classiques des shells usuels : outre la possibilité d'exécuter toutes les
commandes externes, `fsh` devra proposer quelques commandes internes,
permettre la redirection des flots standard ainsi que les combinaisons
par tube, adapter le prompt à la situation, et supporter les structures
conditionnelles et un certain type particulier de boucles.

La gestion des tâches n'est pas requise : tous les processus lancés à
partir de votre shell le seront en premier-plan.

Plus précisément, `fsh` doit respecter les prescriptions suivantes : 


### Commandes externes

`fsh` peut exécuter toutes les commandes externes, avec ou sans
arguments, en tenant compte de la variable d'environnement `PATH`. 


### Commandes internes

`fsh` possède (au moins) les commandes internes listées ci-dessous.
Elles ont comme valeur de retour 0 en cas de succès (à l'exception
d'`exit`, naturellement), et 1 en cas d'échec.

##### `pwd`

Affiche la référence physique absolue du répertoire de travail
courant.

##### `cd [REF | -]`

Change de répertoire de travail courant, qui devient le répertoire `REF`
(s'il s'agit d'une référence valide), le précédent répertoire de travail
si le paramètre est `-`, ou `$HOME` en l'absence de paramètre.

##### `exit [VAL]` 

Termine le processus `fsh` avec comme valeur de retour `VAL` (ou par 
défaut la valeur de retour de la dernière commande exécutée).

Lorsqu'il atteint la fin de son entrée standard (ie si l'utilisateur
saisit `ctrl-D` en mode interactif), `fsh` se comporte comme si la
commande interne `exit` (sans paramètre) avait été saisie.

##### `ftype REF`

Affiche le type du fichier de référence `REF` (s'il s'agit d'une
référence valide) : `directory`, `regular file` , `symbolic link`, `named
pipe`, `other`.


### Redirections vers des fichiers

`fsh` gère les redirections suivantes :

- `CMD < FIC` : redirection de l'entrée standard de la commande `CMD` sur
  le fichier (ordinaire, tube nommé...) `FIC`

- `CMD > FIC` : redirection de la sortie standard de la commande `CMD`
  sur le fichier (ordinaire, tube nommé...) `FIC` **sans écrasement**
  (ie, échoue si `FIC` existe déjà)

- `CMD >| FIC` : redirection de la sortie standard de la commande `CMD`
  sur le fichier (ordinaire, tube nommé...) `FIC` **avec écrasement**
  éventuel

- `CMD >> FIC` : redirection de la sortie standard de la commande `CMD`
  sur le fichier (ordinaire, tube nommé...) `FIC` **en concaténation**

- `CMD 2> FIC` : redirection de la sortie erreur standard de la commande
  `cmd` sur le fichier (ordinaire, tube nommé...) `FIC` **sans
  écrasement** (ie, échoue si `FIC` existe déjà)

- `CMD 2>| FIC` : redirection de la sortie erreur standard de la commande
  `CMD` sur le fichier (ordinaire, tube nommé...) `FIC` **avec
  écrasement** éventuel

- `CMD 2>> FIC` : redirection de la sortie erreur standard de la commande
  `CMD` sur le fichier (ordinaire, tube nommé...) `FIC` **en
  concaténation**

Les espaces de part et d'autre des symboles de redirection sont requis.

En cas d'échec d'une redirection, la ligne de commande saisie n'est pas
exécutée, et la valeur de retour est 1.



### Hiérarchie des lignes de commande


#### Commandes simples

Une **commande simple** est une ligne de commande exécutant une unique
commande, interne ou externe, avec ou sans redirection.


#### Pipelines

Si `CMD_1` et `CMD_2` sont deux commandes simples, `CMD_1 | CMD_2`
provoque l'exécution **simultanée** de `CMD_1` et `CMD_2`, avec
redirection de la sortie standard de `CMD_1` et de l'entrée standard de
`CMD_2` sur un même tube anonyme.

Un **pipeline** est soit une commande simple, soit une ligne de commande
de la forme `CMD_1 | CMD_2 | ... | CMD_N`, avec les redirections
additionnelles éventuelles :
- redirection de l'entrée standard de `CMD_1` 
- redirection de la sortie standard de `CMD_N`
- redirection des sorties erreurs des `N` commandes.

Les espaces de part et d'autre du symbole `|` sont requis.

La valeur de retour est celle de `CMD_N`.


#### Commandes structurées

Une **commande structurée** est soit un pipeline, soit une ligne de
commande d'une des formes suivantes :

##### `CMD_1 ; CMD_2 ; ... ; CMD_N`

Exécute `CMD_1`, **puis** après la terminaison de celle-ci, `CMD_2`, et ainsi
de suite jusqu'à `CMD_N`. 

La valeur de retour est celle de `CMD_N`.


##### `if TEST { CMD }`

Exécute le pipeline `TEST` puis, en cas de succès (_ie_ si sa valeur de
retour est 0), exécute la commande structurée `CMD` entre accolades. 

La valeur de retour est celle de `CMD` si celle-ci est exécutée, et 0
sinon.


##### `if TEST { CMD_1 } else { CMD_2 }`

Exécute le pipeline `TEST` puis, selon sa valeur de retour, exécute l'une
ou l'autre des commandes structurées entre accolades. 

La valeur de retour est celle de `CMD_1` ou de `CMD_2` selon le cas.


##### `for F in REP [-A] [-r] [-e EXT] [-t TYPE] [-p MAX] { CMD }`

Sans option, exécute la commande structurée `CMD` pour chaque (référence
de) fichier non caché du répertoire `REP`. `CMD` peut dépendre de la
valeur de la variable de boucle `F`, désignée par `$F`. Par souci de
simplification, les noms de variables sont limités à un caractère.

Effet des options :

- `-A` : étend la portée de la boucle aux fichiers cachés (sauf `.` et
  `..`);

- `-r` : parcours récursif de toute l'arborescence de racine `REP`;

- `-e EXT` : filtrage des résultats selon l'extension : les fichiers dont
  le nom de base ne se termine pas par `.EXT` sont ignorés; pour les
  autres fichiers, la variable de boucle `F` prend pour valeur la
  référence du fichier *amputée de son extension*, pour permettre des
  usages tels que `for F in . -e jpg { convert $F.jpg $F.png }`;

- `-t TYPE` : filtrage des résultats selon le type de fichier : `f` pour
  les fichiers ordinaires, `d` pour les répertoires, `l` pour les liens
  symboliques, `p` pour les tubes;

- `-p MAX` : permet le traitement en parallèle d'un maximum de `MAX`
  tours de boucle.

La valeur de retour est le **maximum** des valeurs de retour des
différents tours de boucle.

Les espaces de part et d'autre des symboles `;`, `{` et `}` sont requis.


#### Autres lignes de commande

Les commandes structurées sont les lignes de commande les plus génériques
que `fsh` doit savoir interpréter et exécuter. En particulier, il n'est
pas demandé de pouvoir rediriger l'entrée ou la sortie d'une commande
structurée, que ce soit vers un fichier ou dans un pipeline.



### Gestion de la ligne de commande

`fsh` fonctionne en mode interactif : il affiche une invite de commande
(*prompt*), lit la ligne de commande saisie par l'utilisateur, la découpe
en mots selon les (blocs d')espaces, interprète les éventuels caractères
spéciaux (`<`, `>`, `|`, `[`, `}`, `;`, `$`), puis exécute la ou les
commandes correspondantes.

`fsh` ne fournit aucun mécanisme d'échappement; les arguments de la
ligne de commande ne peuvent donc pas contenir de caractères spéciaux
(espaces en particulier). 

Il est fortement recommandé d'utiliser la bibliothèque
[`readline`](https://tiswww.cwru.edu/php/chet/readline/rltop.html) pour
simplifier la lecture de la ligne de commande : cette bibliothèque offre
de nombreuses possibilités d'édition pendant la saisie de la ligne de
commande, de gestion de l'historique, de complétion automatique... Sans
entrer dans les détails, un usage basique de la fonction `char *readline
(const char *prompt)` fournit déjà un résultat très satisfaisant :

```c
char * ligne = readline("mon joli prompt $ ");  
/* alloue et remplit ligne avec une version "propre" de la ligne saisie,
 * ie nettoyée des éventuelles erreurs et corrections (et du '\n' final) */
add_history(ligne);
/* ajoute la ligne à l'historique, permettant sa réutilisation avec les
 * flèches du clavier */
```

L'affichage du prompt de `fsh` est réalisé **sur sa sortie erreur**.
Avec la bibliothèque `readline`, ce comportement s'obtient en indiquant
`rl_outstream = stderr;` avant le premier appel à la fonction
`readline()`.


#### Formatage du prompt

Le prompt est limité à 30 caractères (apparents, ie. sans compter les
indications de couleur), et est formé des éléments suivants :

- un code de bascule de couleur, `"\033[32m"` (vert) ou `"\033[91m"`
  (rouge) suivant que la dernière commande exécutée a réussi ou échoué;
- entre crochets, la **valeur de retour** de la dernière commande
  exécutée (0 par défaut pour le premier prompt) ou `"SIG"` si elle a été
  interrompue par un signal;
- une autre bascule de couleur, par exemple `"\033[34m"` (bleu) ou
  `"\033[36m"` (cyan);
- la référence du **répertoire courant**, éventuellement tronquée (par la
  gauche) pour respecter la limite de 30 caractères; dans ce cas la
  référence commencera par trois points (`"..."`);
- la bascule `"\033[00m"` (retour à la normale); 
- un dollar puis un espace (`"$ "`).

Pour que l'affichage s'adapte correctement à la géométrie de la fenêtre,
chaque bascule de couleur doit être entourée des balises `'\001'` et
`'\002'` (qui indiquent que la portion de chaîne qu'elles délimitent est
formée de caractères non imprimables et doit donc être ignorée dans le
calcul de la longueur du texte à afficher).

Par exemple (sans la coloration) :
```bash
[0]/home/titi$ cd /tmp/pas/trop/long
[0]/tmp/pas/trop/long$ cd mais/la/ca/depasse
[0]...ong/mais/la/ca/depasse$ pwd
/tmp/pas/trop/long/mais/la/ca/depasse
[0]...ong/mais/la/ca/depasse$ cd repertoire-inexistant
cd: no such file or directory: repertoire-inexistant
[1]...ong/mais/la/ca/depasse$ 
```


### Sensibilité aux signaux

`fsh` ignore les signaux `SIGINT` et `SIGTERM`, contrairement aux
processus exécutant des commandes externes.

Lorsque l'exécution d'une commande est interrompue par la réception d'un
signal, le prompt commence par la chaîne `"[SIG]"` en lieu et place de la
valeur de retour, qui est considérée comme valant 255 au cas où un appel 
à `exit` sans paramètre (ou un `ctrl-D`) suit.



## Modalités de réalisation (et de test)

Le projet est à faire par équipes de 3 étudiants, exceptionnellement 2 si
une situation particulière le justifie. La composition de chaque équipe
devra être envoyée par mail à Dominique Poulalhon avec pour sujet `[SY5]
équipe projet` **au plus tard le 10 novembre 2024**, avec copie à chaque
membre de l'équipe.

Chaque équipe doit créer un dépôt `git` **privé** sur le [gitlab de
l'UFR](https://moule.informatique.univ-paris-diderot.fr/) **dès la
constitution de l'équipe** et y donner accès en tant que `Reporter` à
tous les enseignants du cours de Système : Isabelle Fagnot, Guillaume
Geoffroy, Anne Micheli, Astyax Nourel et Dominique Poulalhon. Le dépôt
devra contenir un fichier `AUTHORS.md` donnant la liste des membres de
l'équipe (un par ligne, en précisant pour chacun, dans l'ordre : nom,
prénom, numéro étudiant et pseudo(s) sur le gitlab).

En plus du code source de votre programme, vous devez fournir un
`Makefile` tel que :
  - l'exécution de `make` à la racine du dépôt crée (dans ce même
    répertoire) l'exécutable `fsh`,
  - `make clean` efface tous les fichiers compilés,

ainsi qu'un fichier `ARCHITECTURE.md` expliquant la stratégie adoptée
pour répondre au sujet (notamment l'architecture logicielle, les
structures de données et les algorithmes implémentés).

**Le projet doit s'exécuter correctement sur lulu**.

Les seules interdictions strictes sont les suivantes : plagiat (d'un
autre projet ou d'une source extérieure à la licence, quelle qu'elle
soit), utilisation outrancière d'une IA générative de code, et
utilisation des fonctions de haut niveau masquant les appels système
étudiés en cours pour autre chose que l'interaction entre le shell et
l'utilisateur dans le terminal (notamment la gestion de la ligne de
commande avec `readline`, ou l'affichage de messages d'erreur avec
`perror()`).

Pour rappel, l'emprunt de code sans citer sa source est un
plagiat. L'emprunt de code en citant sa source est autorisé, mais bien
sûr vous n'êtes notés que sur ce que vous avez effectivement produit.
Donc si vous copiez l'intégralité de votre projet en le spécifiant
clairement, vous aurez quand même 0 (mais vous éviterez une demande de
convocation de la section disciplinaire). 



## Modalités de rendu

Le projet sera évalué en 3 phases : deux jalons intermédiaires, sans
soutenance, et un rendu final avec soutenance. Les deux jalons
intermédiaires seront évalués par des tests automatiques.

### Premier jalon jeudi 28 novembre

La version à évaluer devra être spécifiée à l'aide du tag `jalon-1` sur
la branche `main`. 

Pour créer le tag : se placer sur le commit à étiqueter et faire :
```
git tag jalon-1
git push origin --tags
```

Points testés :

 - existence du dépôt git
 - présence du fichier `AUTHORS.md`
 - compilation sans erreur avec `make` à la racine du dépôt
 - exécution de `fsh` à la racine du dépôt
 - commandes internes
 - exécution des commandes externes simples (avec arguments mais sans
   redirections)
 - conformité du prompt (y compris valeur de retour)
 - boucles `for` sans option, avec corps de boucle limité à une commande
   simple


### Deuxième jalon jeudi 19 décembre

La version à évaluer devra être spécifiée à l'aide du tag `jalon-2`.

Points testés : le projet complet **à l'exception** :

 - des pipelines formés de strictement plus d'une commande simple
 - des boucles `for` avec parallélisation (option `-p`)
 - de la gestion des signaux

Par exemple, on demande que cette ligne de commande-ci fonctionne correctement :

```
for F in dir { echo $F ; if test -d $F { for G in $F -A { echo $G } } }
```

mais pas forcément celle-là :
```
cat fichier | tr a A
```

### Rendu final début janvier

La version à évaluer devra être spécifiée à l'aide du tag `rendu-final`.

Le projet final devra être rendu à une date encore à définir mais au plus
tard le 10 janvier, pour des soutenances entre le 13 et le 17 janvier.


### Participation effective

**Les projets sont des travaux de groupe, pas des travaux à répartir
entre vous** ("je fais le projet de Prog fonctionnelle, tu fais celui de
Systèmes"). La participation effective d'un étudiant au projet de son
groupe sera évaluée grâce, entre autres, aux statistiques du dépôt git et
aux réponses aux questions pendant la soutenance, et les notes des
étudiants d'un même groupe pourront être modulées en cas de participation
déséquilibrée. 

**En particulier, un étudiant n'ayant aucun commit sur les aspects
réellement "système" du code et incapable de répondre de manière
satisfaisante sera noté "DEF".**