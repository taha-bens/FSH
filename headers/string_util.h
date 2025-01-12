#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define RESET_COLOR "\033[00m"
#define RED_COLOR "\033[91m"
#define MAX_LENGTH_PROMPT 30 + sizeof(RED_COLOR) + sizeof(RESET_COLOR)


// Fonction pour splitter une chaîne de caractères en fonction d'un délimiteur
char **str_split(char *a_str, const char a_delim);

// Fonction pour libérer la mémoire allouée pour un tableau de chaînes de caractères
void free_split(char **splited);

// Fonction pour supprimer les espaces en début et en fin de chaîne et réduire les espaces multiples entre les mots à un seul espace
char *trim_and_reduce_spaces(const char *str);

// Fonction pour renvoyer si une chaine de caractères marque la fin d'une commande
int is_special_char(char *c);
int is_redirection_char(char *c);

char *substitute_variables(const char *str, int *last_return_value);


#endif // STRING_UTIL_H