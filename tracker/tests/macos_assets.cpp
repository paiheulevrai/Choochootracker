#include "corelib_assets.h"
#include "corelib_file.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    if (assetsInit() != 0) return 1;
    char directory[4096];
    if (fileGetDefaultDirectory(directory, sizeof(directory)) != 0) return 1;
    printf("workspace=%s\n", directory);
    if (!getcwd(directory, sizeof(directory))) return 1;
    printf("cwd=%s\n", directory);
    return 0;
}
