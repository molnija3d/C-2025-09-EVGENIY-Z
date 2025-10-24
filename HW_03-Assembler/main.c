#include <stdio.h>
#include <stdlib.h>

typedef struct list_t
{
    long int value;
    struct list_t *next;
} list_t;

long int data[6] = {4, 8, 15, 16, 23, 42};
const unsigned int data_length = 6;
list_t *f(list_t *, list_t *, int (*)(long int));
void m(list_t *, void (*)(long int));
int p(long int);

list_t *add_element(list_t *list, long int data);
void print_int(long int data);
void free_list(list_t *list);

int main(void)
{

    list_t *list = NULL, *filtered = NULL;
    for (long int i = data_length; i > 0; i--)
    {
        list = add_element(list, data[i - 1]);
    }

    m(list, print_int);
    puts("");
    filtered = f(list, NULL, p);
    m(filtered, print_int);
    puts("");
    
    free_list(list);
    free_list(filtered);

    return 0;
}

list_t *f(list_t *list, list_t *store, int (*func)(long int))
{

    if (list)
    {
        if (func(list->value))
        {
            store = add_element(store, list->value);
        }
        store = f(list->next, store, func);
    }
    return store;
}

void m(list_t *list, void (*func)(long int))
{
    if (list)
    {
        func(list->value);
        m(list->next, func);
    }
}

void print_int(long int value)
{
    printf("%ld ", value);
}

list_t *add_element(list_t *next, long int data)
{
    list_t *node = malloc(sizeof(list_t));
    if (node)
    {
        node->value = data;
        node->next = next;
    }
    else
    {
        abort();
    }
    return node;
}

int p(long int a) 
{
    return a & 0x1;
}

void free_list(list_t *list)
{
    if (list)
    { 
        free_list(list->next);
        free(list);
    }
}