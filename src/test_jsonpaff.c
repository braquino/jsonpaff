#include "jsonpaff.h"
#include <stdio.h>

int main()
{
    char result[200];
    result[0] = 0;
    int error = getJSONPath("{\"hello\": \"world\"}", "$.hello", (char*)&result);
    if (error == 0) {
        printf("result: %s\n", result);
    } else {
        printf("error code %d\n", error);
        return 1;
    }
}