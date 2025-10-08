#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "main.h"

int main(int argc, char **argv)
{
    /* Checking ARGS */
    if (argc != 4)
    {
        printf("USAGE: %s <in-file> <out-file> [koi8r|cp1251| 8859-5]\n", argv[0]);
        printf("example: .\\main koi8.txt out-koi8.txt koi8r\n");
        return EXIT_FAILURE;
    }

   /* Checking input file */
    FILE *file = fopen(argv[1], "r");
    if (!file)
    {
        printf("Failed to open %s for reading!\n", argv[1]);
        return EXIT_FAILURE;
    }

    /* Checking output file */
    FILE *fout = fopen(argv[2], "w");
    if (!fout)
    {
        printf("Failed to open %s for writing!\n", argv[2]);
        return EXIT_FAILURE;
    }

    /* Checking if supported encoding specified. If get_unicode is not NULL, it will point to corresponding function */
    unifunc_t get_unicode = check_encoding(argv[3]);
    if (!get_unicode)
    {
        printf("Wrong encoding specified %s\nUse [koi8r|cp1251|8859-5]", argv[3]);
        return EXIT_FAILURE;
    }

    unsigned char utf8[4];
    int ch = 0, bytes = 0;

    while ((ch = fgetc(file)) != EOF)
    {
        // if ch < 0x80 just put character in file, if ch >= 0x80 - convert to utf8
        bytes = unicode_to_utf8(ch < 0x80 ? ch : get_unicode((unsigned char) ch - 0x80), utf8);
        for (int i = 0; i < bytes; i++)
        {
            fputc(utf8[i], fout);
        }
    }
     
    // closing files
    fclose(fout);
    fclose(file);
    return EXIT_SUCCESS;
}
/* This function return utf byte count*/
int unicode_to_utf8(unsigned int codepoint, unsigned char *utf8)
{
    if (codepoint < 0x80)
    {
        // 1 bytes UTF-8 format: 0xxxxxxx
        utf8[0] = codepoint;
        return 1;
    }
    if (codepoint < 0x800)
    {
        // 2 bytes UTF-8 format: 110xxxxx 10xxxxxx
        utf8[0] = 0xC0 | (codepoint >> 6);   // 0xC0 = 11000000 (first byte)
        utf8[1] = 0x80 | (codepoint & 0x3F); // 0x80 = 10000000, 0x3F = 00111111 (second byte)
        return 2;
    }
    if (codepoint < 0x10000)
    {
        // 3 bytes UTF-8 format: 1110xxxx 10xxxxxx 10xxxxxx
        utf8[0] = 0xE0 | (codepoint >> 12);         // 0xC0 = 11100000 (first byte)
        utf8[1] = 0x80 | ((codepoint >> 6) & 0x3F); // 0x80 = 10000000, 0x3F = 00111111 (second byte)
        utf8[2] = 0x80 | (codepoint & 0x3F);        // 0x80 = 10000000, 0x3F = 00111111 (thirs byte)
        return 3;
    }
    if (codepoint < 0x200000)
    {
        // 4 bytes UTF-8 format: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        utf8[0] = 0xF0 | (codepoint >> 21);          // 0xC0 = 11110000 (first byte)
        utf8[1] = 0x80 | ((codepoint >> 12) & 0x3F); // 0x80 = 10000000, 0x3F = 00111111 (second byte)
        utf8[2] = 0x80 | ((codepoint >> 6) & 0x3F);  // 0x80 = 10000000, 0x3F = 00111111 (thirs byte)
        utf8[3] = 0x80 | (codepoint & 0x3F);         // 0x80 = 10000000, 0x3F = 00111111 (thirs byte)
        return 4;
    }
    return -1;
}

/* This function returns a pointer to correct mapping function, or NULL*/
unifunc_t check_encoding(char *encoding)
{
    if (strcmp(encoding, "koi8r") == 0)
        return map_koi8r;
    if (strcmp(encoding, "cp1251") == 0)
        return map_cp1251;
    if (strcmp(encoding, "8859-5") == 0)
        return map_iso8859_5;
    return NULL;
}

/* Just mapping codes by corresponding table */
unsigned short map_koi8r(unsigned char ch)
{
    return koi8r_to_unicode[ch];
}

/* Just mapping codes by corresponding table */
unsigned short map_cp1251(unsigned char ch)
{
    return cp1251_to_unicode[ch];
}

/* Just mapping codes by corresponding table */
unsigned short map_iso8859_5(unsigned char ch)
{
    /* Because, range of 0x80-9F are terminal codes */
    return iso8859_5_to_unicode[ch - 32];
}