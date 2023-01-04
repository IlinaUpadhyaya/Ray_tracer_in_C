#ifndef FILE_UTILS_H
#define FILE_UTILS_H

typedef struct FileLines {
	int no_of_lines;
	char **lines;
	}FileLines_t;

FileLines_t *getLines(char *fn);

#endif