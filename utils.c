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


// Fonction pour vérifier si les parenthèses sont bien formées
bool areParenthesesWellFormed(const char *expression) {
    Stack stack;
    initStack(&stack);

    // parcours du texte
    for (int i = 0; expression[i] != '\0'; i++) {
        char current = expression[i];
        
        // si c'est un charactère qu'on veut on le met dans la stack
        if (current == '(' || current == '{ ' || current == '[') {
            push(&stack, current);
        } 
        else if (current == ')' || current == '} ' || current == ']') {
            char top;
            if (!pop(&stack, &top)) {
                return false; // Parenthèse fermante non correspondante
            }

            // Vérifier si la parenthèse ouvrante dépilée correspond à la parenthèse fermante
            if ((current == ')' && top != '(') ||
                (current == '}' && top != '{') ||
                (current == ']' && top != '[')) {
                return false;
            }
        }
    }

    return isEmpty(&stack); // on vérifie que c'est bien vide
}

int main() {
    const char *test1 = "if TEST { CMD_1 } else { CMD_2 }";
    const char *test2 = "{[(])}";
    const char *test3 = "{{[[(())]]}}";

    printf("Expression: %s\nResult: %d\n\n", test1, areParenthesesWellFormed(test1) );
    printf("Expression: %s\nResult: %d\n\n", test2, areParenthesesWellFormed(test2) );
    printf("Expression: %s\nResult: %d\n\n", test3, areParenthesesWellFormed(test3) );

}
