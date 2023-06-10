#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

 

int main(int argc, char *argv[]) {
    unsigned char uuid[36];
    strncpy(uuid, argv[1], 36);
    unsigned char uid[] =  "11111111-1111-1111-0000-000000000000";
    unsigned char xored_uuid[36];

 

//    for(int i=0; i< 36; i++)
//         printf("%x", uuid[i]);
//    printf("\n");
    
    for (int i = 0; i < 36; ++i) {
        xored_uuid[i] = tolower(uuid[i]) ^ uid[i];
        printf("%c, %c, %02X \n", uuid[i],uid[i], xored_uuid[i]);
    }

 

    for(int i=0; i< 36; i++)
        printf("%02X", xored_uuid[i]);
    printf("\n");
    return 0;
}
