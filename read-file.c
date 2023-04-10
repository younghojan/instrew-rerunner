#include "common.h"
#include <dirent.h>

void get_files_in_dir(const char *dir_path)
{
    DIR *dir;
    struct dirent *ent;

    dir = opendir(dir_path);
    while ((ent = readdir(dir)) != NULL)
    {
        printf("%s\n", ent->d_name);
    }
    closedir(dir);
}