#include <stdio.h>
#include <stdlib.h>

#include "const.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif


int main(int argc, char **argv)
{
    if(validargs(argc, argv))
        USAGE(*argv, EXIT_FAILURE);
    debug("Options: 0x%x", global_options);
    if(global_options & 1)
        USAGE(*argv, EXIT_SUCCESS);
    else if((global_options & 0xffff) == 2){
        int bs = (global_options>>16);
        int result;
        result = compress(stdin, stdout, (bs*1024));
        if (result == 0) return EXIT_SUCCESS;
        if (result == EOF)
            return EXIT_FAILURE;
    }
    else if(global_options & 4){
        int result = decompress(stdin, stdout);
        if (result == 0) return EXIT_SUCCESS;
        if (result == EOF)
        {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
