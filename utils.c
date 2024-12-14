#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_SIZE 1024

typedef struct Stack {
    char data[MAX_SIZE];
    int top;
} Stack;

void initStack(Stack *stack) {
    stack->top = 0;
}

bool isEmpty(Stack *stack) {
    return stack->top == 0;
}

bool isFull(Stack *stack) {
    return stack->top == MAX_SIZE;
}

bool push(Stack *stack, char c) {
    if (isFull(stack)) {
        return false;
    }
    stack->data[stack->top] = c;
    stack->top++;
    return true;
}

bool pop(Stack *stack, char *c) {
    if (isEmpty(stack)) {
        return false;
    }
    stack->top--;
    *c = stack->data[stack->top];
    return true;
}

// Fonction pour vérifier si les espaces sont bien placés autour des accolades et crochets
bool hasProperSpacing(char *expression) {
    for (int i = 0; expression[i] != '\0'; i++) {
        if (expression[i] == '{' || expression[i] == '}') {
            if (i > 0 && expression[i - 1] != ' ') {
                return false; // Pas d'espace avant '{' ou '}'
            }
            if (expression[i + 1] != ' ' && expression[i + 1] != '\0') {
                return false; // Pas d'espace après '{' ou '}'
            }
        } else if (expression[i] == '[' || expression[i] == ']') {
            if (i > 0 && expression[i - 1] != ' ') {
                return false; // Pas d'espace avant '[' ou ']'
            }
            if (expression[i + 1] != ' ' && expression[i + 1] != '\0') {
                return false; // Pas d'espace après '[' ou ']'
            }
        }
    }
    return true;
}


// Fonction pour vérifier si les parenthèses sont bien formées
bool areParenthesesWellFormed(char *expression) {
    Stack stack;
    initStack(&stack);

    // parcours du texte
    for (int i = 0; expression[i] != '\0'; i++) {
        char current = expression[i];

        // Empiler les accolades et crochets ouvrants
        if (current == '{' || current == '[') {
            if (!push(&stack, current)) {
                return false; // Dépassement de la capacité de la pile
            }
        } 
        // Dépiler et vérifier les accolades et crochets fermants
        else if (current == '}' || current == ']') {
            char top;
            if (!pop(&stack, &top)) {
                return false; // Accolade ou crochet fermant sans correspondance
            }

            // Vérifier si l'accolade ou le crochet ouvrant dépilé correspond
            if ((current == '}' && top != '{') ||
                (current == ']' && top != '[')) {
                return false;
            }
        }
    }

    return isEmpty(&stack); // on vérifie que c'est bien vide
}

// vérifie qu'une expression est valide (bien formé)
bool validateExpression(char *expression) {
    return areParenthesesWellFormed(expression) && hasProperSpacing(expression);
}



/*
int main() {
    char *test1 = "if TEST { CMD_1 } else { CMD_2 }";
    char *test2 = "if TEST{ CMD_1 } else{ CMD_2 }"; // Mauvais espacement
    char *test3 = "if TEST { CMD_1 } else {CMD_2}"; // Mauvais espacement

    printf("Expression: %s\nResult: %d\n\n", test1, validateExpression(test1));
    printf("Expression: %s\nResult: %d\n\n", test2, validateExpression(test2));
    printf("Expression: %s\nResult: %d\n\n", test3, validateExpression(test3));

    return 0;
}
*/