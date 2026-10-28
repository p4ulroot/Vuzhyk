#define WIN32_LEAN_AND_MEAN
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

#define WM_TRAYICON (WM_USER + 1)
#define TRAY_ICON_ID 1
#define DEFAULT_PORT 8765
#define MAX_PACKAGES 4096
#define MAX_PATH_LEN 4096

typedef struct {
    int port;
    int allow_agent;
    char site_packages[MAX_PATH_LEN];
    char seven_zip[MAX_PATH_LEN];
} ServerConfig;

typedef struct {
    char name[256];
} PackageEntry;

static ServerConfig g_config;
static HWND g_hwnd = NULL;
static NOTIFYICONDATAW g_nid;
static volatile LONG g_running = 1;
static HANDLE g_http_thread = NULL;
static SOCKET g_listen_socket = INVALID_SOCKET;

static void trim(char *s)
{
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = 0;
    char *p = s;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

static int read_ini_value(const char *path, const char *section, const char *key, char *out, size_t out_size)
{
    return GetPrivateProfileStringA(section, key, "", out, (DWORD)out_size, path) > 0;
}

static int load_config(ServerConfig *cfg)
{
    char appdata[MAX_PATH_LEN];
    char ini_path[MAX_PATH_LEN];

    if (GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH_LEN) == 0)
        return 0;

    _snprintf(ini_path, sizeof(ini_path) - 1, "%s\\Vuzhyk\\sync-server.ini", appdata);

    cfg->port = GetPrivateProfileIntA("server", "port", DEFAULT_PORT, ini_path);
    if (cfg->port <= 0 || cfg->port > 65535)
        cfg->port = DEFAULT_PORT;

    if (!read_ini_value(ini_path, "server", "site_packages", cfg->site_packages, sizeof(cfg->site_packages)))
        return 0;
    if (!read_ini_value(ini_path, "server", "seven_zip", cfg->seven_zip, sizeof(cfg->seven_zip)))
        return 0;

    cfg->allow_agent = GetPrivateProfileIntA("server", "allow_agent", 1, ini_path) != 0;

    return 1;
}

static void url_decode(char *s)
{
    char *src = s, *dst = s;
    while (*src) {
        if (*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            char hex[3] = { src[1], src[2], 0 };
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            ++src;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = 0;
}

static int str_ieq(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        ++a; ++b;
    }
    return *a == 0 && *b == 0;
}

/* Приводит имя пакета к виду pip (нижний регистр, дефисы -> подчёркивания). */
static void normalize_pkg_name(char *name)
{
    char *dst = name;
    for (char *src = name; *src; ++src) {
        char c = (char)tolower((unsigned char)*src);
        if (c == '-')
            c = '_';
        *dst++ = c;
    }
    *dst = 0;
}

static char normalize_pkg_char(char c)
{
    c = (char)tolower((unsigned char)c);
    return c == '-' ? '_' : c;
}

static int entry_matches_pkg(const char *entry, const char *pkg_name)
{
    size_t pkg_len = strlen(pkg_name);
    size_t i = 0;
    size_t j = 0;

    while (i < pkg_len && entry[j]) {
        if (normalize_pkg_char(pkg_name[i]) != normalize_pkg_char(entry[j]))
            return 0;
        ++i;
        ++j;
    }

    if (i < pkg_len)
        return 0;

    if (entry[j] == 0)
        return 1;
    return entry[j] == '-' || entry[j] == '.' || entry[j] == '_';
}

static int read_metadata_name(const char *dist_info_path, char *out_name, size_t out_size)
{
    char meta_path[MAX_PATH_LEN];
    _snprintf(meta_path, sizeof(meta_path) - 1, "%s\\METADATA", dist_info_path);

    FILE *f = fopen(meta_path, "rb");
    if (!f) {
        _snprintf(meta_path, sizeof(meta_path) - 1, "%s\\PKG-INFO", dist_info_path);
        f = fopen(meta_path, "rb");
    }
    if (!f) return 0;

    char line[512];
    out_name[0] = 0;
    while (fgets(line, sizeof(line), f)) {
        if (_strnicmp(line, "Name:", 5) == 0) {
            char *val = line + 5;
            trim(val);
            strncpy(out_name, val, out_size - 1);
            out_name[out_size - 1] = 0;
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static int list_packages(PackageEntry *entries, int max_entries)
{
    char search_path[MAX_PATH_LEN];
    WIN32_FIND_DATAA fd;
    HANDLE hFind;
    int count = 0;

    _snprintf(search_path, sizeof(search_path) - 1, "%s\\*.dist-info", g_config.site_packages);
    hFind = FindFirstFileA(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        /* pip stores package metadata in *.dist-info directories */
        {
            size_t name_len = strlen(fd.cFileName);
            if (name_len < 10)
                continue;
            if (_stricmp(fd.cFileName + name_len - 10, ".dist-info") != 0)
                continue;
        }

        char dist_path[MAX_PATH_LEN];
        _snprintf(dist_path, sizeof(dist_path) - 1, "%s\\%s", g_config.site_packages, fd.cFileName);

        char name[256];
        if (!read_metadata_name(dist_path, name, sizeof(name)))
            continue;
        normalize_pkg_name(name);

        int dup = 0;
        for (int i = 0; i < count; ++i) {
            if (str_ieq(entries[i].name, name)) { dup = 1; break; }
        }
        if (dup) continue;

        strncpy(entries[count].name, name, sizeof(entries[count].name) - 1);
        entries[count].name[sizeof(entries[count].name) - 1] = 0;
        ++count;
        if (count >= max_entries) break;
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    return count;
}

static int collect_package_paths(const char *pkg_name, char paths[][MAX_PATH_LEN], int max_paths)
{
    char search_path[MAX_PATH_LEN];
    WIN32_FIND_DATAA fd;
    HANDLE hFind;
    int count = 0;
    char normalized_name[256];

    strncpy(normalized_name, pkg_name, sizeof(normalized_name) - 1);
    normalized_name[sizeof(normalized_name) - 1] = 0;
    normalize_pkg_name(normalized_name);

    _snprintf(search_path, sizeof(search_path) - 1, "%s\\*", g_config.site_packages);
    hFind = FindFirstFileA(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        if (!entry_matches_pkg(fd.cFileName, normalized_name))
            continue;

        _snprintf(paths[count], MAX_PATH_LEN - 1, "%s\\%s", g_config.site_packages, fd.cFileName);
        ++count;
        if (count >= max_paths) break;
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    return count;
}

#define CLIENT_THREAD_STACK (2 * 1024 * 1024)

typedef struct {
    SOCKET client;
} ClientParams;

static void normalize_http_path(char *path)
{
    char *query = strchr(path, '?');
    if (query)
        *query = 0;

    size_t len = strlen(path);
    while (len > 0 && (path[len - 1] == '\r' || path[len - 1] == '\n'))
        path[--len] = 0;
}

static int send_all(SOCKET client, const char *data, int len)
{
    int sent_total = 0;
    while (sent_total < len) {
        int sent = send(client, data + sent_total, len - sent_total, 0);
        if (sent <= 0)
            return 0;
        sent_total += sent;
    }
    return 1;
}

static int is_7za_success(DWORD exit_code)
{
    return exit_code <= 1;
}

static int run_hidden_process(char *cmdline, const char *cwd, DWORD *exit_code_out)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 2;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, cwd, &si, &pi))
        return 0;

    WaitForSingleObject(pi.hProcess, 120000);
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code_out)
        *exit_code_out = exit_code;
    return 1;
}

static int verify_zip_signature(const char *zip_path)
{
    FILE *zip_probe = fopen(zip_path, "rb");
    if (!zip_probe)
        return 0;
    unsigned char sig[4] = {0};
    const int read_ok = (int)fread(sig, 1, 4, zip_probe) == 4;
    fclose(zip_probe);
    if (!read_ok)
        return 0;
    return sig[0] == 'P' && sig[1] == 'K' &&
           ((sig[2] == 3 && sig[3] == 4) || (sig[2] == 5 && sig[3] == 6) || (sig[2] == 7 && sig[3] == 8));
}

static int test_zip_with_7za(const char *zip_path)
{
    char cmdline[MAX_PATH_LEN * 2];
    DWORD exit_code = 2;

    _snprintf(cmdline, sizeof(cmdline) - 1, "\"%s\" t -tzip \"%s\"", g_config.seven_zip, zip_path);
    if (!run_hidden_process(cmdline, NULL, &exit_code))
        return 0;
    return is_7za_success(exit_code);
}

static const char *path_relative_to_site_packages(const char *full_path)
{
    const size_t site_root_len = strlen(g_config.site_packages);
    if (site_root_len > 0 &&
        _strnicmp(full_path, g_config.site_packages, site_root_len) == 0 &&
        full_path[site_root_len] == '\\') {
        return full_path + site_root_len + 1;
    }
    return full_path;
}

static int create_package_zip(const char *pkg_name, char *zip_path, size_t zip_path_size)
{
    char (*paths)[MAX_PATH_LEN] = (char (*)[MAX_PATH_LEN])malloc(64 * MAX_PATH_LEN);
    char temp_dir[MAX_PATH_LEN];
    char list_path[MAX_PATH_LEN];
    char cmdline[MAX_PATH_LEN * 4];
    FILE *list_file;
    DWORD exit_code = 2;
    int path_count;
    int i;

    if (!paths)
        return 0;

    if (g_config.seven_zip[0] == 0 || GetFileAttributesA(g_config.seven_zip) == INVALID_FILE_ATTRIBUTES) {
        free(paths);
        return 0;
    }

    path_count = collect_package_paths(pkg_name, paths, 64);
    if (path_count == 0) {
        free(paths);
        return 0;
    }

    if (GetTempPathA(MAX_PATH_LEN, temp_dir) == 0) {
        free(paths);
        return 0;
    }

    _snprintf(zip_path, zip_path_size - 1, "%svuzhyk_sync_%s_%lu.zip",
              temp_dir, pkg_name, (unsigned long)GetTickCount());
    _snprintf(list_path, sizeof(list_path) - 1, "%svuzhyk_sync_%s_%lu.lst",
              temp_dir, pkg_name, (unsigned long)GetTickCount());

    list_file = fopen(list_path, "wb");
    if (!list_file) {
        free(paths);
        return 0;
    }

    for (i = 0; i < path_count; ++i) {
        fprintf(list_file, "%s\r\n", path_relative_to_site_packages(paths[i]));
    }
    fclose(list_file);
    free(paths);

    DeleteFileA(zip_path);

    _snprintf(cmdline, sizeof(cmdline) - 1, "\"%s\" a -tzip -mx=1 -y \"%s\" @\"%s\"",
              g_config.seven_zip, zip_path, list_path);
    if (!run_hidden_process(cmdline, g_config.site_packages, &exit_code)) {
        DeleteFileA(list_path);
        return 0;
    }
    DeleteFileA(list_path);

    if (!is_7za_success(exit_code))
        return 0;
    if (GetFileAttributesA(zip_path) == INVALID_FILE_ATTRIBUTES)
        return 0;
    if (!verify_zip_signature(zip_path))
        return 0;
    if (!test_zip_with_7za(zip_path))
        return 0;

    return 1;
}

static void send_http_response(SOCKET client, int status, const char *status_text,
                               const char *content_type, const char *body, size_t body_len)
{
    char header[1024];
    int header_len = _snprintf(header, sizeof(header) - 1,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        status, status_text, content_type, body_len);

    send_all(client, header, header_len);
    if (body && body_len > 0)
        send_all(client, body, (int)body_len);
}

static void send_file_response(SOCKET client, const char *file_path)
{
    HANDLE hFile = CreateFileA(file_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        send_http_response(client, 404, "Not Found", "text/plain", "Not Found", 9);
        return;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size) || size.QuadPart <= 0 || size.QuadPart > 512LL * 1024 * 1024) {
        CloseHandle(hFile);
        send_http_response(client, 500, "Internal Server Error", "text/plain", "File too large", 14);
        return;
    }

    char header[512];
    int header_len = _snprintf(header, sizeof(header) - 1,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/zip\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        (long long)size.QuadPart);
    send_all(client, header, header_len);

    char buffer[65536];
    DWORD read_bytes = 0;
    while (ReadFile(hFile, buffer, sizeof(buffer), &read_bytes, NULL) && read_bytes > 0) {
        if (!send_all(client, buffer, (int)read_bytes)) {
            CloseHandle(hFile);
            return;
        }
    }
    CloseHandle(hFile);
}

static void handle_client(SOCKET client)
{
    char request[8192];
    int received = recv(client, request, sizeof(request) - 1, 0);
    if (received <= 0) {
        closesocket(client);
        return;
    }
    request[received] = 0;

    char method[16] = {0};
    char path[2048] = {0};
    char query_string[2048] = {0};
    sscanf(request, "%15s %2047s", method, path);

    {
        char *query = strchr(path, '?');
        if (query) {
            strncpy(query_string, query + 1, sizeof(query_string) - 1);
            query_string[sizeof(query_string) - 1] = 0;
        }
    }
    normalize_http_path(path);

    if (_stricmp(method, "GET") != 0) {
        send_http_response(client, 405, "Method Not Allowed", "text/plain", "Method Not Allowed", 18);
        closesocket(client);
        return;
    }

    if (_stricmp(path, "/api/v1/status") == 0) {
        char body[80];
        int n = _snprintf(body, sizeof(body),
                          "{\"ok\":true,\"allowAgent\":%s}",
                          g_config.allow_agent ? "true" : "false");
        if (n < 0) {
            n = 0;
        }
        send_http_response(client, 200, "OK", "application/json", body, (size_t)n);
    } else if (_stricmp(path, "/api/v1/packages") == 0) {
        PackageEntry *entries = (PackageEntry *)malloc((size_t)MAX_PACKAGES * sizeof(PackageEntry));
        if (!entries) {
            send_http_response(client, 500, "Internal Server Error", "text/plain", "OOM", 3);
            closesocket(client);
            return;
        }
        int count = list_packages(entries, MAX_PACKAGES);
        size_t cap = 64;
        char *json = (char *)malloc(cap);
        if (!json) {
            free(entries);
            send_http_response(client, 500, "Internal Server Error", "text/plain", "OOM", 3);
            closesocket(client);
            return;
        }
        size_t len = 0;
        json[len++] = '[';
        int json_ok = 1;
        for (int i = 0; i < count; ++i) {
            char item[512];
            int item_len = _snprintf(item, sizeof(item) - 1, "%s\"%s\"", i ? "," : "", entries[i].name);
            if (len + (size_t)item_len + 2 >= cap) {
                cap *= 2;
                char *njson = (char *)realloc(json, cap);
                if (!njson) {
                    free(json);
                    json_ok = 0;
                    break;
                }
                json = njson;
            }
            memcpy(json + len, item, item_len);
            len += item_len;
        }
        if (json_ok) {
            json[len++] = ']';
            send_http_response(client, 200, "OK", "application/json", json, len);
            free(json);
        } else {
            send_http_response(client, 500, "Internal Server Error", "text/plain", "OOM", 3);
        }
        free(entries);
    } else if (_stricmp(path, "/api/v1/package") == 0) {
        char pkg_name[256] = {0};
        if (query_string[0]) {
            char *name_param = strstr(query_string, "name=");
            if (name_param) {
                name_param += 5;
                char *amp = strchr(name_param, '&');
                if (amp) {
                    size_t n = (size_t)(amp - name_param);
                    if (n >= sizeof(pkg_name)) n = sizeof(pkg_name) - 1;
                    memcpy(pkg_name, name_param, n);
                    pkg_name[n] = 0;
                } else {
                    strncpy(pkg_name, name_param, sizeof(pkg_name) - 1);
                }
                url_decode(pkg_name);
                normalize_pkg_name(pkg_name);
            }
        }

        if (pkg_name[0] == 0) {
            send_http_response(client, 400, "Bad Request", "text/plain", "Missing name", 12);
        } else {
            char zip_path[MAX_PATH_LEN];
            if (create_package_zip(pkg_name, zip_path, sizeof(zip_path))) {
                send_file_response(client, zip_path);
                DeleteFileA(zip_path);
            } else {
                send_http_response(client, 404, "Not Found", "text/plain", "Package not found", 17);
            }
        }
    } else {
        send_http_response(client, 404, "Not Found", "text/plain", "Not Found", 9);
    }

    closesocket(client);
}

static DWORD WINAPI client_thread_proc(LPVOID param)
{
    ClientParams *work = (ClientParams *)param;
    if (work) {
        handle_client(work->client);
        free(work);
    }
    return 0;
}

static DWORD WINAPI http_server_thread(LPVOID param)
{
    (void)param;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    g_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen_socket == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    int yes = 1;
    setsockopt(g_listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

    struct sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((u_short)g_config.port);

    if (bind(g_listen_socket, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR ||
        listen(g_listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(g_listen_socket);
        g_listen_socket = INVALID_SOCKET;
        WSACleanup();
        return 1;
    }

    while (InterlockedCompareExchange(&g_running, 1, 1) == 1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(g_listen_socket, &readfds);

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        SOCKET ready = select(0, &readfds, NULL, NULL, &tv);
        if (ready == SOCKET_ERROR || ready == 0)
            continue;

        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        SOCKET client = accept(g_listen_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client == INVALID_SOCKET)
            continue;

        ClientParams *work = (ClientParams *)malloc(sizeof(ClientParams));
        if (!work) {
            closesocket(client);
            continue;
        }
        work->client = client;

        HANDLE client_thread = CreateThread(
            NULL,
            CLIENT_THREAD_STACK,
            client_thread_proc,
            work,
            STACK_SIZE_PARAM_IS_A_RESERVATION,
            NULL);
        if (!client_thread) {
            free(work);
            closesocket(client);
            continue;
        }
        CloseHandle(client_thread);
    }

    if (g_listen_socket != INVALID_SOCKET) {
        closesocket(g_listen_socket);
        g_listen_socket = INVALID_SOCKET;
    }
    WSACleanup();
    return 0;
}

static void stop_http_server(void)
{
    InterlockedExchange(&g_running, 0);
    if (g_listen_socket != INVALID_SOCKET) {
        closesocket(g_listen_socket);
        g_listen_socket = INVALID_SOCKET;
    }
    if (g_http_thread) {
        WaitForSingleObject(g_http_thread, 5000);
        CloseHandle(g_http_thread);
        g_http_thread = NULL;
    }
}

static void show_tray_menu(void)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L"\x0412\x044B\x043A\x043B\x044E\x0447\x0438\x0442\x044C"); /* Выключить */

    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, g_hwnd, NULL);
    DestroyMenu(menu);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            show_tray_menu();
        } else if (lParam == WM_LBUTTONDBLCLK) {
            /* no-op */
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            stop_http_server();
            PostQuitMessage(0);
        }
        return 0;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        stop_http_server();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

static HICON load_app_icon(void)
{
    wchar_t exe_path[MAX_PATH_LEN];
    GetModuleFileNameW(NULL, exe_path, MAX_PATH_LEN);
    wchar_t *slash = wcsrchr(exe_path, L'\\');
    if (slash) {
        wcscpy(slash + 1, L"vuzhyk.ico");
        HICON icon = (HICON)LoadImageW(NULL, exe_path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        if (icon) return icon;
        if (slash) {
            wcscpy(slash + 1, L"..\\icons\\logo\\vuzhyk.ico");
            icon = (HICON)LoadImageW(NULL, exe_path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
            if (icon) return icon;
        }
    }
    return LoadIconW(NULL, IDI_APPLICATION);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)pCmdLine;
    (void)nCmdShow;

    if (!load_config(&g_config))
        return 1;

    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"VuzhykLibrarySyncServer";
    RegisterClassW(&wc);

    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"Vuzhyk Library Sync", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    if (!g_hwnd)
        return 1;

    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = TRAY_ICON_ID;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = load_app_icon();
    wcscpy(g_nid.szTip, L"Vuzhyk \x2014 \x0441\x0438\x043D\x0445\x0440\x043E\x043D\x0438\x0437\x0430\x0446\x0438\x044F \x0431\x0438\x0431\x043B\x0438\x043E\x0442\x0435\x043A"); /* синхронизация библиотек */
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    g_http_thread = CreateThread(NULL, 0, http_server_thread, NULL, 0, NULL);
    if (!g_http_thread) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    stop_http_server();
    return (int)msg.wParam;
}
