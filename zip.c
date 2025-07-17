#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#define MAX_FILES 256

uint32_t crc32_table[256];

void init_crc32() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
}

uint32_t calc_crc32(const uint8_t *buf, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        crc = (crc >> 8) ^ crc32_table[(crc ^ buf[i]) & 0xFF];
    return ~crc;
}

void write_u16(FILE *f, uint16_t val) {
    fputc(val & 0xFF, f);
    fputc((val >> 8) & 0xFF, f);
}

void write_u32(FILE *f, uint32_t val) {
    fputc(val & 0xFF, f);
    fputc((val >> 8) & 0xFF, f);
    fputc((val >> 16) & 0xFF, f);
    fputc((val >> 24) & 0xFF, f);
}

int main() {
    char input[8192];
    char *files[MAX_FILES];
    int num_files = 0;

    printf("\033c\033[43;30m\nFiles to zip (separated by space): ");
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\r\n")] = 0;

    char *token = strtok(input, " ");
    while (token && num_files < MAX_FILES) {
        files[num_files++] = strdup(token);
        token = strtok(NULL, " ");
    }

    init_crc32();

    FILE *zip = fopen("output.zip", "wb");
    if (!zip) {
        perror("Error creating ZIP file");
        return 1;
    }

    long offsets[MAX_FILES];
    uint32_t crcs[MAX_FILES];
    uint32_t sizes[MAX_FILES];

    for (int i = 0; i < num_files; i++) {
        FILE *f = fopen(files[i], "rb");
        if (!f) {
            perror(files[i]);
            continue;
        }

        fseek(f, 0, SEEK_END);
        size_t fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        uint8_t *buffer = malloc(fsize);
        fread(buffer, 1, fsize, f);
        fclose(f);

        crcs[i] = calc_crc32(buffer, fsize);
        sizes[i] = fsize;
        offsets[i] = ftell(zip);

        // --- Local File Header ---
        fwrite("PK\x03\x04", 1, 4, zip);  // signature
        write_u16(zip, 20);  // version needed
        write_u16(zip, 0);   // flags
        write_u16(zip, 0);   // method (0 = store)
        write_u16(zip, 0); write_u16(zip, 0); // time, date
        write_u32(zip, crcs[i]);
        write_u32(zip, fsize);
        write_u32(zip, fsize);
        write_u16(zip, strlen(files[i])); // filename length
        write_u16(zip, 0); // extra length
        fwrite(files[i], 1, strlen(files[i]), zip);
        fwrite(buffer, 1, fsize, zip);

        free(buffer);
    }

    long cd_start = ftell(zip);

    for (int i = 0; i < num_files; i++) {
        fwrite("PK\x01\x02", 1, 4, zip);   // central dir header
        write_u16(zip, 0x0014);            // version made by (FAT)
        write_u16(zip, 20);                // version needed
        write_u16(zip, 0);                 // flags
        write_u16(zip, 0);                 // method
        write_u16(zip, 0); write_u16(zip, 0); // time, date
        write_u32(zip, crcs[i]);
        write_u32(zip, sizes[i]);
        write_u32(zip, sizes[i]);
        write_u16(zip, strlen(files[i]));  // name length
        write_u16(zip, 0);                 // extra
        write_u16(zip, 0);                 // comment
        write_u16(zip, 0);                 // disk number
        write_u16(zip, 0);                 // internal attr
        write_u32(zip, 0);                 // external attr
        write_u32(zip, offsets[i]);        // offset to local header
        fwrite(files[i], 1, strlen(files[i]), zip);
    }

    long cd_end = ftell(zip);
    long cd_size = cd_end - cd_start;

    // --- End of Central Directory ---
    fwrite("PK\x05\x06", 1, 4, zip);
    write_u16(zip, 0);       // disk
    write_u16(zip, 0);       // disk CD starts
    write_u16(zip, num_files);
    write_u16(zip, num_files);
    write_u32(zip, cd_size);
    write_u32(zip, cd_start);
    write_u16(zip, 0);       // comment length

    fclose(zip);
    printf("ZIP criado com sucesso: output.zip\n");

    // Liberar nomes
    for (int i = 0; i < num_files; i++) free(files[i]);
    return 0;
}

