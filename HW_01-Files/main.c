#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#define ZIP_SIGNATURE 0x504b0304 // "PK\03\04" in little-endian

int extract_files(FILE *fin, FILE *fout);

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("USAGE: %s <file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    FILE *fin = fopen(argv[1], "rb");
    printf("%s\n", argv[1]);

    char fout_name[256];
    sprintf(fout_name,"%s_extracted.txt",argv[1]);
    FILE *fout = fopen(fout_name, "wb");
    if (!fin)
    {
        printf("Failed to open %s for reading!\n", argv[1]);
        return EXIT_FAILURE;
    }
    if (!extract_files(fin, fout))
    {
        printf("NOT A RARJPG FILE <%s>\n", argv[1]);
    }
    fclose(fin);
    fclose(fout);
    return EXIT_SUCCESS;
}

int extract_files(FILE *fin, FILE *fout)
{
    // reading a file by 8k buffers
    unsigned char buffer[8192];
    char f_name[256];
    // PKZIP signature is 4 bytes 0x504b0304, uint32_t is sufficient handle it
    uint32_t window = 0;
    int32_t len = 0; // file name length
    size_t bytes;    // count of bytes successfully read from file
    size_t offset = 0;
    long total_files = 0;

    fseek(fin, 0, SEEK_SET);
    // read by 8192 bytes from file in cycle
    while ((bytes = fread(buffer, 1, sizeof(buffer), fin)) > 0)
    {
        // default offset
        offset = 3;
        // moving a 4 byte window over the buffer
        for (size_t i = 0; i < bytes; i++)
        {
            window <<= 8;
            window |= buffer[i] & 0xFF;

            // if window is equal to signature, we are on header position
            if (window == ZIP_SIGNATURE)
            {
                // if length and signature placed not in the same buffer
                if (i + 24 > bytes)
                {
                    offset = 24;
                    break;
                }
                // len field is shifted from current position by 23 bytes
                len = buffer[i + 23] | buffer[i + 24];
                // if file name and signature placed not in the same buffer
                if (i + 27 + len > bytes)
                {
                    offset = i + 27 + len - bytes;
                    break;
                }

                for (int n = 0; n < len; n++)
                {
                    // reading file name to variable. File name is shifted by 27 bytes from current position
                    f_name[n] = buffer[i + 27 + n];
                }
                // tail symbol
                f_name[len] = '\0';

                // if we get here, we found a record, so, increase counter
                total_files++;

                printf("%s\n", f_name);
                fprintf(fout, "%s\n", f_name);
            }
        }

        // Signature could be not aligned with buffers
        // if we had read more than 3 bytes, we can skip signature at border of two buffers
        // if first part of signature located at first buffer, but second part at next buffer
        // So, we need to roll back at least by 3 bytes (default offset)
        if (bytes > offset)
        {
            fseek(fin, -offset, SEEK_CUR);
        }
    }

    printf("Total records = %ld\n", total_files);
    fprintf(fout, "Total records = %ld\n", total_files);

    return total_files ? 1 : 0;
}