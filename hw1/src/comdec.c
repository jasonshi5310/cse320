#include "const.h"
#include "sequitur.h"
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

/*
 * You may modify this file and/or move the functions contained here
 * to other source files (except for main.c) as you wish.
 *
 * IMPORTANT: You MAY NOT use any array brackets (i.e. [ and ]) and
 * you MAY NOT declare any arrays or allocate any storage with malloc().
 * The purpose of this restriction is to force you to use pointers.
 * Variables to hold the pathname of the current file or directory
 * as well as other data have been pre-declared for you in const.h.
 * You must use those variables, rather than declaring your own.
 * IF YOU VIOLATE THIS RESTRICTION, YOU WILL GET A ZERO!
 *
 * IMPORTANT: You MAY NOT use floating point arithmetic or declare
 * any "float" or "double" variables.  IF YOU VIOLATE THIS RESTRICTION,
 * YOU WILL GET A ZERO!
 */

/**
 *  Converts utf hex to index
 *  @param utf the utf hex code
 *  @return the index in the rule_map
 */
int utf_to_i(int utf)
{
    if (0 <= utf && utf <= 0x7f) // <=U+007f
        return utf;
    else if (0xc280 <= utf && utf <= 0xdfbf) // <= U+07ff
    {
        //int i = utf & 0x1f3f;
        int fb = utf & 0x1f00;
        fb = fb >> 2;
        int sb = utf & 0x003f;
        int i = fb + sb;
        return i;
    }
    else if (0xe0a080 <= utf && utf <= 0xefbfbf) // <= U+FFFF
    {
        //int i = utf & 0x0f3f3f;
        int fb, sb, tb;
        fb = utf & 0x0f0000;
        sb = utf & 0x003f00;
        tb = utf & 0x00003f;
        fb >>= 4;
        sb >>= 2;
        int i = fb + sb + tb;
        return i;
    }
    else if (0xf0908080 <= utf && utf <= 0xf7bfbfbf )
    {
        //int i = utf & 0x073f3f3f;
        int fb, sb, tb, fourth;
        fb = utf & 0x07000000;
        sb = utf & 0x003f0000;
        tb = utf & 0x00003f00;
        fourth = utf & 0x003f;
        fb >>= 6;
        sb >>= 4;
        tb >>= 2;
        int i = fb + sb + tb +fourth;
        return i;
    }
    else return -1;
}


/**
 *  Convert integer to utf-8
 *  @param i the integer needed to be converted
 *  @return the utf-8 value of the input i
 */
int i_to_utf(int i)
{
    if (0 <= i && i <= 0x7f) // <=U+007f
        return i;
    else if(0x80 <= i && i<= 0x07ff)
    {
        int utf = 0xc080; // 1100 0000 1000 0000 b
        int f = i & 0x7c0;
        int s = i & 0x03f;
        f <<= 2; // sll 2 bits
        utf = utf + f + s;
        return utf;
    }
    else if(0x0800 <= i && i<= 0xffff)
    {
        int utf = 0xe08080; // 1110 0000 1000 0000 1000 0000 b
        int f = i & 0xf000;
        int s = i & 0x0fc0;
        int t = i & 0x003f;
        f <<= 4; // sll 4 bits
        s <<= 2; // sll 2 bits
        utf = utf + f + s + t;
        return utf;
    }
    else if(0x10000 <= i && i<= 0x1fffff)
    {
        // 1111 0000 1000 0000 1000 0000 1000 0000 b
        int utf = 0xf0808080;
        int f, s, t, fourth;
        f = i & 0x1c0000;
        s = i & 0x03f000;
        t = i & 0x000fc0;
        fourth = i &0x3f;
        f <<= 6;
        s <<= 4;
        t <<= 3;
        utf = utf + f + s + t + fourth;
        return utf;
    }
    return -1;
}


/**
 * Converts utf value to char(s) and put in a file
 *
 * @param utf the utf needed to convert
 * @param out the output file
 * @return the number of bytes witten
 */
int utf_to_char (int utf, FILE* out)
{
    if (0 <= utf && utf <= 0x7f) // <=U+007f
    {
        fputc(utf, out);
        return 1;
    }
    else if (0xc280 <= utf && utf <= 0xdfbf) // <= U+07ff
    {
        //int i = utf & 0x1f3f;
        int fb = utf & 0xff00;
        fb = fb >> 8;
        fputc(fb, out);
        int sb = utf & 0x00ff;
        fputc(sb, out);
        return 2;
    }
    else if (0xe0a080 <= utf && utf <= 0xefbfbf) // <= U+FFFF
    {
        //int i = utf & 0x0f3f3f;
        int fb, sb, tb;
        fb = utf & 0xff0000;
        sb = utf & 0x00ff00;
        tb = utf & 0x0000ff;
        fb >>= 16;
        sb >>= 8;
        fputc(fb, out);
        fputc(sb, out);
        fputc(tb, out);
        return 3;
    }
    else if (0xf0908080 <= utf && utf <= 0xf7bfbfbf )
    {
        //int i = utf & 0x073f3f3f;
        int fb, sb, tb, fourth;
        fb = utf & 0xff000000;
        sb = utf & 0x00ff0000;
        tb = utf & 0x0000ff00;
        fourth = utf & 0x00ff;
        fb >>= 24;
        sb >>= 16;
        tb >>= 8;
        fputc(fb, out);
        fputc(sb, out);
        fputc(tb, out);
        fputc(fourth, out);
        return 4;
    }
    else return -1;
}

/**
 *  Write a block of rules to the output file.
 *  @param mr the main rule in this block
 *  @param out the output file
 *  @return number of bytes written
 */
int shrink (SYMBOL* mr, FILE* out)
{
    int count = 0;
    // write main rule first
    int value = mr -> value;
    value = i_to_utf(value);
    int vi = utf_to_char(value, out);
    if (vi == -1)
    {
        return EOF;
    }
    count += vi;
    SYMBOL* s = mr -> next;
    while (s != mr)
    {
        value = s -> value;
        value = i_to_utf(value);
        vi = utf_to_char(value, out);
        if (vi == -1)
        {
            return EOF;
        }
        count += vi;
        s = s-> next;
    }
    if ((mr -> nextr) == mr)
    {
        return count;
    }
    fputc(0x85,out); // end of main rule
    count++;
    s = mr -> nextr;
    while (s != mr)
    {
        value = s -> value;
        value = i_to_utf(value);
        vi = utf_to_char(value, out);
        if (vi == -1)
        {
            return EOF;
        }
        count += vi; // rule head
        SYMBOL* n = s -> next;
        while (n != s)
        {
            value = n -> value;
            value = i_to_utf(value);
            vi = utf_to_char(value, out);
            if (vi == -1)
            {
                return EOF;
            }
            count += vi;
            n = n -> next;
        }
        s = s-> nextr;
        if (s != mr)
            {fputc(0x85, out); count++;}
    }
    return count;
}

/**
 * Main compression function.
 * Reads a sequence of bytes from a specified input stream, segments the
 * input data into blocks of a specified maximum number of bytes,
 * uses the Sequitur algorithm to compress each block of input to a list
 * of rules, and outputs the resulting compressed data transmission to a
 * specified output stream in the format detailed in the header files and
 * assignment handout.  The output stream is flushed once the transmission
 * is complete.
 *
 * The maximum number of bytes of uncompressed data represented by each
 * block of the compressed transmission is limited to the specified value
 * "bsize".  Each compressed block except for the last one represents exactly
 * "bsize" bytes of uncompressed data and the last compressed block represents
 * at most "bsize" bytes.
 *
 * @param in  The stream from which input is to be read.
 * @param out  The stream to which the block is to be written.
 * @param bsize  The maximum number of bytes read per block.
 * @return  The number of bytes written, in case of success,
 * otherwise EOF.
 */
int compress(FILE *in, FILE *out, int bsize) {
    // To be implemented.
    if (in == NULL)
        return EOF;
    if (out == NULL)
        return EOF;
    int nw = 0; // number of bytes written
    //printf("line strat");
    fputc(0x81, out);
    nw++;
    while (!feof(in))
    {
        int nr = 0; // number of bytes read
        // initial rules, symbols, dig_hash, main rule
        init_rules();
        init_symbols();
        init_digram_hash();
        SYMBOL* mr = new_rule(next_nonterminal_value++);
        add_rule(mr); // add main rule
        while (nr < bsize)
        {
            int b = fgetc(in);
            if (b== -1) // EOT
            {
                break;
            }
            SYMBOL* this = mr -> prev;
            SYMBOL* next = new_symbol(b, NULL);
            insert_after(this, next);
            check_digram(this);
            nr++;
        }
        if (nr == 0)  break;
        fputc(0x83, out);
        //shrink here
        int vv = shrink(mr, out);
        if (vv == EOF)
        {
            return EOF;
        }
        nw += vv;
        fputc(0x84, out);
        nw += 2;
    }
    fputc(0x82, out);
    nw++;
    return nw;
}

/**
 * Expand the main rule at the end of a block
 *
 * @param mr the main rule to expend
 * @param out The stream to which the uncompressed data is to be written.
 * @return the int written into the file
 */
int expand(SYMBOL* mr, FILE *out)
{
    int count = 0;
    SYMBOL* s = mr -> next;
    while (s != mr)
    {
        int value = s -> value;
        if (IS_TERMINAL(s))
        {
            fputc(value, out);
            count++;
        }
        else
        {
            SYMBOL* r = *(rule_map+value);
            if (r==NULL)
            {
                return EOF;
            }
            count += expand(r, out);
        }
        s = s -> next;
    }
    return count;
}


/**
 * Main decompression function.
 * Reads a compressed data transmission from an input stream, expands it,
 * and and writes the resulting decompressed data to an output stream.
 * The output stream is flushed once writing is complete.
 *
 * @param in  The stream from which the compressed block is to be read.
 * @param out  The stream to which the uncompressed data is to be written.
 * @return  The number of bytes written, in case of success, otherwise EOF.
 */
int decompress(FILE *in, FILE *out) {
    // To be implemented.
    int noob = 0;//0 means nothing read yet; otherwise none zero
    int noow = 0; // num of bytes written
    int if_end_of_block; // if the current block ends
    SYMBOL* prev_sym;
    if(in== NULL)
        return EOF;
    if(out==NULL)
        return EOF;
    int nofs = 0; // num of symbol in a rule
    while(!(feof(in)))
    {
        int b = fgetc(in);
        if (noob == 0 && b != 0x81) // no proper sof
        {
            return EOF;
        }
        if (noob != 0 && b == 0x81) // another sof
        {
            return EOF;
        }
        if (b == 0x82) // End of transmission
        {
            int by = fgetc(in);
            if (by != EOF)
                return EOF;
            break;
        }
        if (b == -1) // end of file before eot
        {
            return EOF;
        }
        int fb = b & 0xf0;
        if (b == 0x81) // if start of the program
        {
            init_rules();
            init_symbols();
            int mr = fgetc(in);
            if (mr == 0x82) {
                // check later
                return 0;
            }
            if (mr != 0x83) return EOF;
            mr = fgetc(in); // fisrt byte
            if (mr == 0x84) continue;
            if (mr == 0x85 || mr == 0x81 || mr == 0x82
                || mr== 0x83)
                return EOF;
            int check = mr;
            check = check & 0xf0;
            if (mr < 0xc4) return EOF;
            int mrl = fgetc(in); // second byte
            if (mr==-1 || mrl == -1)
            {
                return EOF;
            }
            if (!((mrl & 0xc0) == 0x80)) return EOF;
            mr <<= 8;
            mr = mr+mrl;
            if (check >= 0xe0)
            {
                mrl = fgetc(in);
                if (mrl == -1)
                {
                    return EOF;
                }
                if (!((mrl & 0xc0) == 0x80)) return EOF;
                mr = mr << 8;
                mr = mr + mrl; //add the thrid byte
            }
            if (check >= 0xf0)
            {
                mrl = fgetc(in);
                if (mrl == -1)
                {
                    return EOF;
                }
                if (!((mrl & 0xc0) == 0x80)) return EOF;
                mr = mr << 8;
                mr = mr + mrl; //add the fourth byte
            }
            mr = utf_to_i(mr);
            if (mr == -1) return EOF;
            SYMBOL* nr = new_rule(mr);
            add_rule(nr);
            prev_sym = nr;
            if_end_of_block = 0;
        }
        else if ( b == 0x83) // beginning of a block
        {
            if(!if_end_of_block)
                return EOF;
            init_rules();
            init_symbols();
            int r = fgetc(in);
            if (r == 0x84) continue;
            if (r == 0x85 || r == 0x81 || r == 0x82 || r==0x83)
                return EOF;
            if (r < 0xc4) return EOF;
            int check = r & 0xf0;
            int rl = fgetc(in);
            if (r==-1 || rl == -1)
            {
                return EOF;
            }
            if (!((rl&0xc0) == 0x80)) return EOF;
            r <<= 8;
            r = r+rl;
            if (check >= 0xe0)
            {
                rl = fgetc(in);
                if (rl == -1)
                {
                    return EOF;
                }
                if (!((rl&0xc0) == 0x80)) return EOF;
                r = r << 8;
                r = r + rl; //add the thrid byte
            }
            if (check >= 0xf0)
            {
                rl = fgetc(in);
                if (r==-1 || rl == -1)
                {
                    return EOF;
                }
                if (!((rl&0xc0) == 0x80)) return EOF;
                r = r << 8;
                r = r + rl; //add the fourth byte
            }
            r = utf_to_i(r);
            if (r == -1) return EOF;
            SYMBOL* nr = new_rule(r);
            add_rule(nr);
            prev_sym = nr;
            if_end_of_block = 0;
        }
        else if( b == 0x84) // end of a block
        {
            // expend from main rule in this block
            if (nofs == 0) return EOF;
            SYMBOL* h = prev_sym -> next;
            // Only one symbol, one rule
            if (nofs == 1 && !IS_TERMINAL(prev_sym) && (h -> nextr) == h)
            {
                return EOF;
            }
            int res = expand((h->nextr), out);
            if (res == EOF)
            {
                return EOF;
            }
            noow += res;
            if_end_of_block = -1;
            nofs = 0;
        }
        else if (b == 0x85) // end of a rule
        {
            int r = fgetc(in);
            if (r==0x81|| r==0x82|| r==0x83 || r==0x84 || r==0x85)
                return EOF;
            if (r < 0xc4) return EOF;
            int rl = fgetc(in);
            if (r==-1 || rl == -1)
            {
                return EOF;
            }
            if (!((rl&0xc0) == 0x80)) return EOF;
            int check = r & 0xf0;
            r <<= 8;
            r = r+rl;
            if (check >= 0xe0)
            {
                rl = fgetc(in);
                if (rl == -1)
                {
                    return EOF;
                }
                if (!((rl&0xc0) == 0x80)) return EOF;
                r = r << 8;
                r = r + rl; //add the thrid byte
            }
            if (check >= 0xf0)
            {
                rl = fgetc(in);
                if (r==-1 || rl == -1)
                {
                    return EOF;
                }
                if (!((rl&0xc0) == 0x80)) return EOF;
                r = r << 8;
                r = r + rl; //add the fourth byte
            }
            r = utf_to_i(r);
            if (r == -1) return EOF;
            SYMBOL* nr = new_rule(r);
            add_rule(nr);
            prev_sym = nr;
        }
        else if (fb >= 0xc0)
        {
            if (if_end_of_block)
            {
                return EOF;
            }
            int check = fb;
            int sb = fgetc(in);
            if (fb==-1 || sb == -1)
            {
                return EOF;
            }
            if (!((sb&0xc0) == 0x80)) return EOF;
            if (b < 0xc2) return EOF;
            fb = b << 8;
            fb = fb + sb; // add the second byte
            if (check >= 0xe0)
            {
                sb = fgetc(in);
                if (sb == -1)
                {
                    return EOF;
                }
                if (!((sb&0xc0) == 0x80)) return EOF;
                fb = fb << 8;
                fb = fb + sb; //add the thrid byte
            }
            if (check >= 0xf0)
            {
                sb = fgetc(in);
                if (sb == -1)
                {
                    return EOF;
                }
                if (!((sb&0xc0) == 0x80)) return EOF;
                fb = fb << 8;
                fb = fb + sb; //add the fourth byte
            }
            fb = utf_to_i(fb);
            if (fb == -1) return EOF;
            if (fb == (prev_sym -> next -> value))
            {
                return EOF;
            }
            SYMBOL* cur_sym = new_symbol(fb, NULL);
            cur_sym-> next = prev_sym -> next;
            cur_sym->prev = prev_sym;
            prev_sym -> next = cur_sym;
            cur_sym -> next -> prev = cur_sym;
            prev_sym = cur_sym;
            nofs++;
        }
        else
        {
            if (if_end_of_block)
            {
                return EOF;
            }
            if (b > 0x7f) return EOF;
            b = utf_to_i(b);
            if (b == -1) return EOF;
            if (fb == (prev_sym -> next -> value))
            {
                return EOF;
            }
            SYMBOL* cur_sym = new_symbol(b, NULL);
            cur_sym-> next = prev_sym -> next;
            cur_sym->prev = prev_sym;
            prev_sym -> next = cur_sym;
            cur_sym -> next -> prev = cur_sym;
            prev_sym = cur_sym;
            nofs++;
        }
        noob++;
    }
    return noow;
}

int strToInt(char *p)
{
    //char *tp = p;
    // Convert str to int, char by char
    int a = 0;
    while (*p != '\0')
    {
        if (*p < 48 || *p > 57)
            return -1;
        a = a*10;
        a = a + *p - 48;
        p++;
    }
    return a;
}

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the selected program options will be set in the
 * global variable "global_options", where they will be accessible
 * elsewhere in the program.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * Refer to the homework document for the effects of this function on
 * global variables.
 * @modifies global variable "global_options" to contain a bitmap representing
 * the selected options.
 */
int validargs(int argc, char **argv)
{
    // To be implemented.
    // pointer for the arguments
    global_options = 0;
    if (argc == 1) return -1;
    char *p = *(argv+1);
    if (*p == '-' && *(p+1) =='h'&& *(p+2) == '\0')
    {
        global_options = global_options | 1;
        return 0;
    }
    else if (*p == '-' && *(p+1) == 'c' && *(p+2) == '\0')
    {
        if (argc != 2 && argc != 4) {return -1;}
        if (argc == 4)
        {
            // pointer for the second argu
            char *bp = *(argv+2);
            if (*bp == '-' && *(bp+1)=='b'&& *(bp+2) == '\0')
            {
                // check num here
                int a = strToInt(*(argv+3));
                if (a<1 || a> 1024)
                    return -1;
                a <<= 16;
                global_options = a;
            }
            else return -1;
        }
        if (argc == 2)
        {
            global_options = (0x0400 << 16);
        }
        global_options = global_options | 2;
        return 0;
    }
    else if (*p == '-' && *(p+1) == 'd'&& *(p+2) == '\0')
    {
        // change the nums of args here
        if (argc != 2) return -1;
        global_options = global_options | 4;
        return 0;
    }
    else {return -1;}
    //return -1;
}