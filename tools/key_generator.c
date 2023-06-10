#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

int main(int argc, char *argv[]) {
    unsigned char uuid[36];
    strncpy(uuid, argv[1], 36);
    unsigned char uid[] =  "abababab-cdcd-ffff-1234-567890abcdf0";
    unsigned char xored_uuid[36];

//    for(int i=0; i< 36; i++)
//         printf("%x", uuid[i]);
//    printf("\n");

    for (int i = 0; i < 36; ++i) {
        xored_uuid[i] = tolower(uuid[i]) ^ uid[i];
	printf("uuid: %x uid: %x xored_uuid: %x\n",tolower(uuid[i]),uid[i],xored_uuid[i]);
    }

    time_t t;
    srand((unsigned) time(&t));

    unsigned char *cypher[4] = {
                           "\t\t\t\t+---[RSA 2048]----+\n"
                           "\t\t\t\t|    .      ..oo..|\n"
                           "\t\t\t\t|   . . .  . .o.X.|\n"
                           "\t\t\t\t|    . . o.  ..+ B|\n"
                           "\t\t\t\t|   .   o.o  .+ ..|\n"
                           "\t\t\t\t|    ..o.S   o..  |\n"
                           "\t\t\t\t|   . ^o=      .  |\n"
                           "\t\t\t\t|    @.B...     . |\n"
                           "\t\t\t\t|   o.=. o. . .  .|\n"
                           "\t\t\t\t|    .oo  E. . .. |\n"
                           "\t\t\t\t+----[SHA256]-----+" ,
                           "\t\t\t\t+--[ RSA 2048]----+\n"
                           "\t\t\t\t|          .oo.   |\n"
                           "\t\t\t\t|         .  o.E  |\n"
                           "\t\t\t\t|        + .  o   |\n"
                           "\t\t\t\t|     . = = .     |\n"
                           "\t\t\t\t|      = S = .    |\n"
                           "\t\t\t\t|     o + = +     |\n"
                           "\t\t\t\t|      . o + o .  |\n"
                           "\t\t\t\t|           . o   |\n"
                           "\t\t\t\t|                 |\n"
                           "\t\t\t\t+-----------------+" ,
                           "\t\t\t\t+---[RSA 2048]----+\n"
                           "\t\t\t\t|            . +o |\n"
                           "\t\t\t\t|           . =o  |\n"
                           "\t\t\t\t|          . o o. |\n"
                           "\t\t\t\t|           + +.  |\n"
                           "\t\t\t\t|        S o + *+o|\n"
                           "\t\t\t\t|      .. . . *.B*|\n"
                           "\t\t\t\t|      .o. . o ==+|\n"
                           "\t\t\t\t|      .o.= + +.Eo|\n"
                           "\t\t\t\t|      ...oX++o.o=|\n"
                           "\t\t\t\t+----[SHA256]-----+" ,
                           "\t\t\t\t+---[RSA 2048]----+\n"
                           "\t\t\t\t|                 |\n"
                           "\t\t\t\t|                 |\n"
                           "\t\t\t\t|                 |\n"
                           "\t\t\t\t|. . .            |\n"
                           "\t\t\t\t|.* *    S      . |\n"
                           "\t\t\t\t|* B o. ..       o|\n"
                           "\t\t\t\t|.X ....=  .    .o|\n"
                           "\t\t\t\t|= +oo.o *.....o.o|\n"
                           "\t\t\t\t| o.o+o.++*+=*O+E |\n"
                           "\t\t\t\t+----[SHA256]-----+"
    };
    printf("%s\n", cypher[rand() % 4]);
    printf("Please copy this key (only the red part): \n\t-----> ");
    printf("\033[0;31m");                                    \
    for(int i=0; i< 36; i++)
        printf("%02X", xored_uuid[i]);
    printf("\033[0m");
    printf(" <-----\n");
    return 0;
}
