// winwery.c
// Applies winstate.json to configure a Windows system

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>
#include <direct.h>
#include "cJSON.h"
#include <srrestoreptapi.h>  // Add this include (needs linking with srclient.lib or use LoadLibrary + GetProcAddress)
#include <shlobj.h>  // for SHGetFolderPath

#pragma comment(lib, "srclient.lib") // If using MSVC; for mingw you may need to link srclient or use dynamic load

#define WINSTATE_VERSION "1.0.0"
#define BUILD_DATE __DATE__ " " __TIME__

char config_path[MAX_PATH];
SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, config_path);
strcat(config_path, "\\winstate.json");

void create_restore_point(const char* desc) {
    RESTOREPOINTINFO rpInfo = {0};
    STATEMGRSTATUS smStatus = {0};

    rpInfo.dwEventType = BEGIN_SYSTEM_CHANGE;
    rpInfo.dwRestorePtType = APPLICATION_INSTALL;
    rpInfo.llSequenceNumber = 0;
    strncpy(rpInfo.szDescription, desc, sizeof(rpInfo.szDescription) - 1);

    if (!SRSetRestorePoint(&rpInfo, &smStatus)) {
        printf("Failed to create restore point: error %ld\n", GetLastError());
        return;
    }

    rpInfo.dwEventType = END_SYSTEM_CHANGE;
    if (!SRSetRestorePoint(&rpInfo, &smStatus)) {
        printf(" Failed to end restore point\n");
        return;
    }

    printf("System Restore Point created: %s\n", desc);
}

void reboot_system() {
    // Enable the shutdown privilege
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;
    
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          &hToken)) {
        printf(" Could not open process token\n");
        return;
    }

    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
    if (GetLastError() != ERROR_SUCCESS) {
        printf(" Could not adjust token privileges\n");
        CloseHandle(hToken);
        return;
    }

    CloseHandle(hToken);

    // Initiate system shutdown and reboot
    if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE,
                      SHTDN_REASON_MAJOR_OPERATINGSYSTEM | SHTDN_REASON_MINOR_UPGRADE |
                      SHTDN_REASON_FLAG_PLANNED)) {
        printf(" Failed to reboot system\n");
    } else {
        printf("♻️ System reboot initiated\n");
    }
}


void run_command(const char* cmd) {
    printf("Running: %s\n", cmd);
    system(cmd);
}

void apply_registry(cJSON* registry) {
    if (!registry) return;
    int len = cJSON_GetArraySize(registry);
    for (int i = 0; i < len; i++) {
        cJSON* entry = cJSON_GetArrayItem(registry, i);
        const char* path = cJSON_GetObjectItem(entry, "path")->valuestring;
        const char* name = cJSON_GetObjectItem(entry, "name")->valuestring;
        const char* type = cJSON_GetObjectItem(entry, "type")->valuestring;
        const char* value = cJSON_GetObjectItem(entry, "value")->valuestring;

        char cmd[1024];
        sprintf(cmd, "reg add \"%s\" /v \"%s\" /t %s /d \"%s\" /f",
                path, name, type, value);
        run_command(cmd);
    }
}
void apply_wallpaper(cJSON* config) {
    cJSON* wall = cJSON_GetObjectItem(config, "wallpaper");
    if (wall && wall->valuestring && strlen(wall->valuestring) > 0) {
        SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, (PVOID)wall->valuestring, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
        printf(" Wallpaper set to: %s\n", wall->valuestring);
    }
}

void apply_reg_files(cJSON* regfiles) {
    if (!regfiles) return;
    int len = cJSON_GetArraySize(regfiles);
    for (int i = 0; i < len; i++) {
        const char* file = cJSON_GetArrayItem(regfiles, i)->valuestring;
        char cmd[512];
        sprintf(cmd, "reg import \"%s\"", file);
        run_command(cmd);
    }
}

void apply_copy(cJSON* copies) {
    if (!copies) return;
    int len = cJSON_GetArraySize(copies);
    for (int i = 0; i < len; i++) {
        cJSON* entry = cJSON_GetArrayItem(copies, i);
        const char* from = cJSON_GetObjectItem(entry, "from")->valuestring;
        const char* to = cJSON_GetObjectItem(entry, "to")->valuestring;
        char cmd[1024];
        sprintf(cmd, "copy /Y \"%s\" \"%s\"", from, to);
        run_command(cmd);
    }
}

void apply_hostname(cJSON* config) {
    if (!config) return;
    cJSON* host = cJSON_GetObjectItem(config, "hostname");
    if (host && host->valuestring && strlen(host->valuestring) > 0) {
        char cmd[256];
        sprintf(cmd, "wmic computersystem where name=\"%%COMPUTERNAME%%\" call rename name=\"%s\"", host->valuestring);
        run_command(cmd);
    }
}

void apply_timezone(cJSON* config) {
    if (!config) return;
    cJSON* tz = cJSON_GetObjectItem(config, "timezone");
    if (tz && tz->valuestring && strlen(tz->valuestring) > 0) {
        char cmd[256];
        sprintf(cmd, "tzutil /s \"%s\"", tz->valuestring);
        run_command(cmd);
    }
}

void apply_startup(cJSON* startup) {
    if (!startup) return;
    char appdata[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_STARTUP, NULL, 0, appdata);

    int len = cJSON_GetArraySize(startup);
    for (int i = 0; i < len; i++) {
        const char* script = cJSON_GetArrayItem(startup, i)->valuestring;
        char dest[MAX_PATH];
        sprintf(dest, "%s\\%d_startup.ps1", appdata, i);

        char cmd[1024];
        sprintf(cmd, "copy \"%s\" \"%s\"", script, dest);
        run_command(cmd);
    }
}

void apply_hooks(cJSON* hooks) {
    if (!hooks) return;
    cJSON* pre = cJSON_GetObjectItem(hooks, "pre_apply");
    if (pre && pre->valuestring && strlen(pre->valuestring) > 0) {
        char cmd[512];
        sprintf(cmd, "powershell -ExecutionPolicy Bypass -File \"%s\"", pre->valuestring);
        run_command(cmd);
    }
}

void apply_git_repo(cJSON* config) {
    if (!config) return;
    cJSON* repo = cJSON_GetObjectItem(config, "git_repo");
    if (repo && repo->valuestring && strlen(repo->valuestring) > 0) {
        char cmd[1024];
        sprintf(cmd, "git clone \"%s\" temp_conf", repo->valuestring);
        run_command(cmd);
    }
}

void install_package_list(cJSON* list, const char* tool) {
    if (!list) return;
    int len = cJSON_GetArraySize(list);
    for (int i = 0; i < len; i++) {
        const char* pkg = cJSON_GetArrayItem(list, i)->valuestring;
        char cmd[256];
        sprintf(cmd, "%s install -y %s", tool, pkg);
        run_command(cmd);
    }
}

// Creates default winstate.json if missing
int create_default_config() {
    FILE* f = fopen(config_path, "r");
    if (f) {
        fclose(f);
        printf("%s already exists\n", config_path);
        return 0;
    }

    f = fopen(config_path, "w");
    if (!f) {
        printf(" Failed to create %s\n", config_path);
        return 1;
    }

    const char* default_json = 
        "{\n"
        "  \"registry\": [],\n"
        "  \"reg_files\": [],\n"
        "  \"copy\": [],\n"
        "  \"startup\": [],\n"
        "  \"hostname\": \"\",\n"
        "  \"timezone\": \"\",\n"
        "  \"hooks\": {},\n"
        "  \"git_repo\": \"\",\n"
        "  \"choco\": [],\n"
        "  \"scoop\": [],\n"
        "  \"winget\": []\n"
        "}\n";

    fputs(default_json, f);
    fclose(f);

    printf(" Created default %s\n", config_path);
    return 0;
}

int apply_config() {
    FILE* f = fopen(config_path, "r");
    if (!f) {
        printf("Could not open %s\n", config_path);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* data = malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON* config = cJSON_Parse(data);
    free(data);
    if (!config) {
        printf("Invalid JSON\n");
        return 1;
    }

    apply_hooks(cJSON_GetObjectItem(config, "hooks"));
    apply_git_repo(config);
    apply_reg_files(cJSON_GetObjectItem(config, "reg_files"));
    apply_registry(cJSON_GetObjectItem(config, "registry"));
    apply_copy(cJSON_GetObjectItem(config, "copy"));
    apply_startup(cJSON_GetObjectItem(config, "startup"));
    apply_hostname(config);
    apply_timezone(config);
    apply_wallpaper(config);
    install_package_list(cJSON_GetObjectItem(config, "choco"), "choco");
    install_package_list(cJSON_GetObjectItem(config, "scoop"), "scoop");
    install_package_list(cJSON_GetObjectItem(config, "winget"), "winget");

    cJSON_Delete(config);
    create_restore_point("WinState v" WINSTATE_VERSION " applied");
    reboot_system();
    return 0;
void apply_wallpaper(cJSON* config) {
    cJSON* wall = cJSON_GetObjectItem(config, "wallpaper");
    if (wall && wall->valuestring && strlen(wall->valuestring) > 0) {
        SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, (PVOID)wall->valuestring, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
        printf(" Wallpaper set to: %s\n", wall->valuestring);
    }
}
}
// Utility: read entire file into string
char* read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);
    return data;
}

// Utility: save cJSON to file
int save_config(cJSON* config) {
    char* str = cJSON_Print(config);
    FILE* f = fopen(config_path, "w");
    if (!f) {
        free(str);
        return 0;
    }
    fputs(str, f);
    fclose(f);
    free(str);
    return 1;
}

// Add wallpaper (replace or add)
void add_wallpaper(cJSON* config, const char* path) {
    cJSON* wallpaper = cJSON_GetObjectItem(config, "wallpaper");
    if (wallpaper) {
        cJSON_SetValuestring(wallpaper, path);
    } else {
        cJSON_AddStringToObject(config, "wallpaper", path);
    }
    printf("Added wallpaper: %s\n", path);
}

// Add startup script to array
void add_startup_script(cJSON* config, const char* path) {
    cJSON* startup = cJSON_GetObjectItem(config, "startup");
    if (!startup) {
        startup = cJSON_CreateArray();
        cJSON_AddItemToObject(config, "startup", startup);
    }
    cJSON_AddItemToArray(startup, cJSON_CreateString(path));
    printf("Added startup script: %s\n", path);
}

// Add hostname (replace)
void add_hostname(cJSON* config, const char* name) {
    cJSON* hostname = cJSON_GetObjectItem(config, "hostname");
    if (hostname) {
        cJSON_SetValuestring(hostname, name);
    } else {
        cJSON_AddStringToObject(config, "hostname", name);
    }
    printf("Set hostname: %s\n", name);
}

// Add copy operation
void add_copy(cJSON* config, const char* src, const char* dest) {
    cJSON* copy = cJSON_GetObjectItem(config, "copy");
    if (!copy) {
        copy = cJSON_CreateArray();
        cJSON_AddItemToObject(config, "copy", copy);
    }
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "from", src);
    cJSON_AddStringToObject(obj, "to", dest);
    cJSON_AddItemToArray(copy, obj);
    printf("Added copy from '%s' to '%s'\n", src, dest);
}

// Add package to tool array (choco, scoop, winget)
void add_package(cJSON* config, const char* tool, const char* pkg) {
    cJSON* arr = cJSON_GetObjectItem(config, tool);
    if (!arr) {
        arr = cJSON_CreateArray();
        cJSON_AddItemToObject(config, tool, arr);
    }
    cJSON_AddItemToArray(arr, cJSON_CreateString(pkg));
    printf("Added package '%s' to %s\n", pkg, tool);
}

// Interactive registry entry prompt
void add_registry_prompt(cJSON* config) {
    cJSON* registry = cJSON_GetObjectItem(config, "registry");
    if (!registry) {
        registry = cJSON_CreateArray();
        cJSON_AddItemToObject(config, "registry", registry);
    }

    while (1) {
        char path[512];
        char name[128];
        char type[32];
        char value[512];
        char yn[8];

        printf("Add registry entry:\n");

        printf(" Registry path (e.g. HKCU\\Software\\MyApp): ");
        if (!fgets(path, sizeof(path), stdin)) break;
        path[strcspn(path, "\r\n")] = 0;
        if (strlen(path) == 0) break;

        printf(" Value name: ");
        if (!fgets(name, sizeof(name), stdin)) break;
        name[strcspn(name, "\r\n")] = 0;

        printf(" Value type (e.g. DWORD, String): ");
        if (!fgets(type, sizeof(type), stdin)) break;
        type[strcspn(type, "\r\n")] = 0;

        printf(" Value data: ");
        if (!fgets(value, sizeof(value), stdin)) break;
        value[strcspn(value, "\r\n")] = 0;

        cJSON* obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "path", path);
        cJSON_AddStringToObject(obj, "name", name);
        cJSON_AddStringToObject(obj, "type", type);
        cJSON_AddStringToObject(obj, "value", value);

        cJSON_AddItemToArray(registry, obj);
        printf("Added registry entry: %s\\%s\n", path, name);

        printf("Add another? (y/n): ");
        if (!fgets(yn, sizeof(yn), stdin)) break;
        if (yn[0] != 'y' && yn[0] != 'Y') break;
    }
}

int add_command(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage:\n");
        printf("  add wall <path>\n");
        printf("  add stup <path>\n");
        printf("  add hostname <name>\n");
        printf("  add copy <src> <dest>\n");
        printf("  add pkg <choco|scoop|winget> <pkg>\n");
        printf("  add reg\n");
        return 1;
    }

    char* data = read_file(config_path);
    if (!data) {
        printf("Could not open %s, please run 'init' first.\n", config_path);
        return 1;
    }

    cJSON* config = cJSON_Parse(data);
    free(data);
    if (!config) {
        printf("Invalid JSON in %s\n", config_path);
        return 1;
    }

    const char* type = argv[2];

    if (strcmp(type, "wall") == 0) {
        if (argc < 4) {
            printf("Missing wallpaper path\n");
            cJSON_Delete(config);
            return 1;
        }
        add_wallpaper(config, argv[3]);
    }
    else if (strcmp(type, "stup") == 0) {
        if (argc < 4) {
            printf("Missing startup script path\n");
            cJSON_Delete(config);
            return 1;
        }
        add_startup_script(config, argv[3]);
    }
    else if (strcmp(type, "hostname") == 0) {
        if (argc < 4) {
            printf("Missing hostname\n");
            cJSON_Delete(config);
            return 1;
        }
        add_hostname(config, argv[3]);
    }
    else if (strcmp(type, "copy") == 0) {
        if (argc < 5) {
            printf("Missing source or destination path\n");
            cJSON_Delete(config);
            return 1;
        }
        add_copy(config, argv[3], argv[4]);
    }
    else if (strcmp(type, "pkg") == 0) {
        if (argc < 5) {
            printf("Missing package tool or package name\n");
            cJSON_Delete(config);
            return 1;
        }
        if (strcmp(argv[3], "choco") && strcmp(argv[3], "scoop") && strcmp(argv[3], "winget")) {
            printf("Invalid package tool. Use choco, scoop, or winget.\n");
            cJSON_Delete(config);
            return 1;
        }
        add_package(config, argv[3], argv[4]);
    }
    else if (strcmp(type, "reg") == 0) {
        printf("Entering interactive registry entry mode. Press Enter without input to stop.\n");
        add_registry_prompt(config);
    }
    else {
        printf("Unknown add type '%s'\n", type);
        cJSON_Delete(config);
        return 1;
    }

    if (!save_config(config)) {
        printf("Failed to save %s\n", config_path);
        cJSON_Delete(config);
        return 1;
    }

    cJSON_Delete(config);
    printf("Configuration updated successfully.\n");
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s [init|apply|add]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "init") == 0) {
        return create_default_config();
    } else if (strcmp(argv[1], "apply") == 0) {
        return apply_config();
    } else if (strcmp(argv[1], "add") == 0) {
        return add_command(argc, argv);
    } else {
        printf("Unknown command '%s'\n", argv[1]);
        return 1;
    }
}
