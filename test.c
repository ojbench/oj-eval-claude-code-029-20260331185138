#include <stdio.h>
#include <stdlib.h>

int main() {
    int *arr = malloc(10 * sizeof(int));
    if (!arr) {
        printf("malloc failed\n");
        return 1;
    }

    for (int i = 0; i < 10; i++) {
        arr[i] = i;
    }

    for (int i = 0; i < 10; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    free(arr);
    printf("Test passed!\n");
    return 0;
}
