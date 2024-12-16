#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Fonction pour splitter une chaîne de caractères en fonction d'un délimiteur
char **str_split(char *a_str, const char a_delim);

// Fonction pour libérer la mémoire allouée pour un tableau de chaînes de caractères
void free_split(char **splited);

// Fonction pour supprimer les espaces en début et en fin de chaîne et réduire les espaces multiples entre les mots à un seul espace
char *trim_and_reduce_spaces(const char *str);

#endif // STRING_UTIL_H