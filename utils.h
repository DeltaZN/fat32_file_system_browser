//
// Created by georgii on 24.02.2021.
//

#ifndef LAB1_UTILS_H
#define LAB1_UTILS_H
int parse(const char *cmd, char **args);
int check_directory(const char *path);
int startsWith(const char *str, const char *pre);
void append_path_part(char *path, const char *part);
char *get_line(void);
void remove_ending_symbol(char *str, char sym);
void remove_until(char *str, char sym);
#endif //LAB1_UTILS_H
