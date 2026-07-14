#include "../src/massmover.c"

int main(int argc, char** argv)
{
    char* data;
    struct ImportedServerGroup* groups = NULL;
    int groupCount = 0;
    int invalidCount = 0;
    int permissionCount = 0;
    int i;

    if (argc != 2) {
        return 2;
    }
    data = readJsonFile(argv[1]);
    if (!data || !parseServerGroupImport(data, &groups, &groupCount, &invalidCount)) {
        free(data);
        return 3;
    }
    free(data);
    for (i = 0; i < groupCount; i++) {
        permissionCount += groups[i].permissionCount;
    }
    printf("groups=%d permissions=%d invalid=%d\n", groupCount, permissionCount, invalidCount);
    groupImportState.groups = groups;
    groupImportState.groupCount = groupCount;
    clearGroupImportState();
    return 0;
}
