#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "FileUtils.h"


FileLines_t *getLines(char *fn) {
    FILE *fp;
    fp = fopen(fn, "r");
    if (!fp) perror(fn), exit(1);
    FileLines_t *file_lines = (FileLines_t *) malloc(sizeof(FileLines_t));
    memset(file_lines, 0, sizeof(FileLines_t));

    char *line = (char *) malloc(sizeof(char)); // 1 char space
    char *line_pointer = line;

    // initially allocate space for one pointer in array
    file_lines->lines = (char **) malloc(sizeof(char *));
    file_lines->no_of_lines = 0;
    *(file_lines->lines) = line;

    int char_no = 0; // next write location
    int chars_in_line = 1;

    char c;
    while ((c = (char) getc(fp)) != EOF) {
        if (char_no == chars_in_line) {
            chars_in_line += 1;
            char *tmp = (char *) realloc(line, chars_in_line * sizeof(char));
            if (tmp != NULL) {
                line = tmp;
            } else {
                exit(EXIT_FAILURE);
            }
            line_pointer = line + char_no;
        }
        if (c == '\n') {
            *line_pointer = 0; // NULL terminate line
            *(file_lines->lines +
              file_lines->no_of_lines) = line; // store line since we will now start new line

            // create and add new line
            line = (char *) malloc(sizeof(char));
            chars_in_line = 1;
            line_pointer = line;
            char_no = 0;

            // now we add the new line
            file_lines->no_of_lines += 1;
            char **tmp = (char **) realloc(file_lines->lines, (size_t) (file_lines->no_of_lines *
                                                                        sizeof(*file_lines)));
            if (tmp != NULL) {
                file_lines->lines = tmp;
            } else {
                exit(EXIT_FAILURE);
            }
        } else {
            *line_pointer++ = c;
            ++char_no;
        }
    }
    fclose(fp);
    return file_lines;
}
