//
// Created by gosha on 15.02.2021.
//

int startsWith(const char *str, const char *pre) {
    size_t lenpre = strlen(pre),
            lenstr = strlen(str);
    return lenstr < lenpre ? 0 : memcmp(pre, str, lenpre) == 0;
}

void append_path_part(char *path, const char *part) {
    strcat(path, "/");
    strcat(path, part);
}

char *get_line(void) {
    char *line = malloc(100), *linep = line;
    size_t lenmax = 100, len = lenmax;
    int c;

    if (line == NULL)
        return NULL;

    for (;;) {
        c = fgetc(stdin);
        if (c == EOF)
            break;

        if (--len == 0) {
            len = lenmax;
            char *linen = realloc(linep, lenmax *= 2);

            if (linen == NULL) {
                free(linep);
                return NULL;
            }
            line = linen + (line - linep);
            linep = linen;
        }

        if ((*line++ = c) == '\n')
            break;
    }
    *line = '\0';
    return linep;
}

void remove_ending_symbol(char *str, char sym) {
    for (int i = 0;; i++) {
        if (str[i] == sym)
            str[i] = 0;
        if (str[i] == 0)
            break;
    }
}

void remove_until(char *str, char sym) {
    int len = strlen(str);
    for (int i = len; i >= 0; i--) {
        if (str[i] != sym)
            str[i] = 0;
        else {
            str[i] = 0;
            break;
        }
    }
}
