#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#define SIZE 4096

// Alloue et initialise un buffer pour le chemin
char *initialiser_buffer() {
    char *res = malloc(SIZE);
    if (res == NULL) {
        perror("malloc");
        return NULL;
    }
    res[0] = '\0';
    return res;
}

// Ouvre et renvoie le descripteur de fichier du répertoire parent
int ouvrir_repertoire_parent() {
    int parent_fd = open("..", O_RDONLY);
    if (parent_fd == -1) {
        perror("open parent");
    }
    return parent_fd;
}

// Trouve le nom du répertoire à partir de son inode et de son périphérique
char *trouver_nom_repertoire(DIR *dir, struct stat *current_stat, int parent_fd) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        struct stat entry_stat;
        if (fstatat(parent_fd, entry->d_name, &entry_stat, 0) == -1) {
            perror("fstatat entry");
            continue;
        }
        // Comparer l'inode et le périphérique
        if (entry_stat.st_ino == current_stat->st_ino && entry_stat.st_dev == current_stat->st_dev) {
            char *name = malloc(strlen(entry->d_name) + 1);
            if (name == NULL) {
                perror("malloc for name");
                return NULL;
            }
            strcpy(name, entry->d_name);
            return name;
        }
    }
    fprintf(stderr, "Failed to find directory entry\n");
    return NULL;
}

// Met à jour le chemin avec le nom du répertoire trouvé
int mettre_a_jour_chemin(char **res, char *name, size_t *res_len) {
    size_t name_len = strlen(name);
    if (*res_len + name_len + 2 >= SIZE) {
        fprintf(stderr, "Buffer size exceeded, truncating path\n");
        return -1;
    }
    memmove(*res + name_len + 1, *res, *res_len + 1);
    memcpy(*res + 1, name, name_len);
    (*res)[0] = '/';
    *res_len += name_len + 1;
    return 0;
}

// Fonction principale pour obtenir le chemin absolu
char *chemin_du_repertoire() {
    char *res = initialiser_buffer();
    // On mémorise le descripteur de fichier courant pour pouvoir revenir en arrière à la fin et éviter un effet de bord
    int current_fd = open(".", O_RDONLY);
    if (res == NULL) return NULL;

    size_t res_len = 0;
    struct stat current_stat, parent_stat;

    if (lstat(".", &current_stat) == -1) {
        perror("lstat current");
        free(res);
        return NULL;
    }

    while (1) {
        int parent_fd = ouvrir_repertoire_parent();
        if (parent_fd == -1) {
            free(res);
            return NULL;
        }

        if (fstat(parent_fd, &parent_stat) == -1) {
            perror("fstat parent");
            close(parent_fd);
            free(res);
            return NULL;
        }

        // Si on atteint la racine du système de fichiers
        if (current_stat.st_ino == parent_stat.st_ino && current_stat.st_dev == parent_stat.st_dev) {
            close(parent_fd);
            break;
        }

        DIR *dir = opendir("..");
        if (dir == NULL) {
            perror("opendir");
            close(parent_fd);
            free(res);
            return NULL;
        }

        char *name = trouver_nom_repertoire(dir, &current_stat, parent_fd);
        closedir(dir);
        if (name == NULL) {
            close(parent_fd);
            free(res);
            return NULL;
        }

        if (mettre_a_jour_chemin(&res, name, &res_len) == -1) {
            free(name);
            close(parent_fd);
            break;
        }
        
        free(name);
        if (fchdir(parent_fd) == -1) {
            perror("fchdir");
            close(parent_fd);
            free(res);
            return NULL;
        }

        close(parent_fd);
        current_stat = parent_stat;
    }

    if (res[0] == '\0') {
        strcpy(res, "/");
    }

    // Revenir au répertoire courant
    if (fchdir(current_fd) == -1) {
        perror("fchdir current");
        free(res);
        return NULL;
    }
    close(current_fd);

    return res;
}

// Pour ce faire rien de plus simple, il suffit de trouver le dernier '/' dans le chemin absolu
char* nom_repertoire_courant() {
    char *chemin = chemin_du_repertoire();
    if (chemin == NULL) {
        return NULL;
    }

    // Utiliser strtok pour trouver le dernier segment du chemin
    char *nom = NULL;
    char *token = strtok(chemin, "/");
    while (token != NULL) {
        nom = token;
        token = strtok(NULL, "/");
    }

    char *resultat = strdup(nom ? nom : "/");
    free(chemin);
    return resultat;
}

// Commande pwd
int pwd() {
    char *chemin = chemin_du_repertoire();
    write(STDOUT_FILENO, chemin, strlen(chemin));
    write(STDOUT_FILENO, "\n", 1);
    return 0;
}