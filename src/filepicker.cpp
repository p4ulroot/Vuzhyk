#ifdef _WIN32
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <shlobj.h>
#include <objbase.h>
#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>

struct ComInit {
    bool ok = false;
    ComInit() : ok(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {}
    ~ComInit() { if (ok) CoUninitialize(); }
};

static int runFilePicker(int argc, char *argv[])
{
    ComInit com;

    // Парсим аргументы командной строки
    std::string extension; // Обязательный параметр
    std::string initialDir; // Опциональный параметр
    bool isSave = false;
    bool isDirectory = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-e" || arg == "--ext" || arg == "--extension") {
            if (i + 1 < argc) {
                extension = argv[++i];
            }
        } else if (arg == "-d" || arg == "--directory") {
            if (i + 1 < argc) {
                initialDir = argv[++i];
            }
        } else if (arg == "-s" || arg == "--save") {
            isSave = true;
        } else if (arg == "--dir") {
            isDirectory = true;
        }
    }
    
    // Расширение обязательно
    if (extension.empty()) {
        return 1; // Завершаем с ошибкой, если расширение не указано
    }
    
    // Нормализуем расширение (добавляем точку, если её нет)
    if (extension[0] != '.') {
        extension = "." + extension;
    }
    
    // Создаем описание фильтра на основе расширения
    std::string filterDesc;
    if (extension == ".py") {
        filterDesc = "Python Files";
    } else if (extension == ".pyrob.json") {
        filterDesc = "Pyrob Project";
    } else if (extension == ".json") {
        filterDesc = "JSON Files";
    } else {
        // Генерируем описание из расширения
        filterDesc = extension.substr(1) + " Files";
        // Делаем первую букву заглавной
        if (!filterDesc.empty()) {
            filterDesc[0] = std::toupper(filterDesc[0]);
        }
    }
    
    // Формируем паттерн фильтра
    std::string pattern = "*" + extension;
    
    std::string resultPath;
    
    if (isDirectory) {
        // Используем SHBrowseForFolderW для выбора папки (Unicode версия)
        BROWSEINFOW bi = {0};
        bi.hwndOwner = nullptr;
        bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS;
        bi.lpszTitle = L"Select Folder";
        
        if (!initialDir.empty()) {
            // Устанавливаем начальную директорию
            int dirSize = MultiByteToWideChar(CP_UTF8, 0, initialDir.c_str(), -1, nullptr, 0);
            std::vector<wchar_t> dirWide(dirSize);
            MultiByteToWideChar(CP_UTF8, 0, initialDir.c_str(), -1, dirWide.data(), dirSize);
            bi.pidlRoot = nullptr; // Можно установить через SHParseDisplayName если нужно
        }
        
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (pidl != nullptr) {
            wchar_t path[MAX_PATH];
            if (SHGetPathFromIDListW(pidl, path)) {
                // Конвертируем wide string в UTF-8
                int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
                std::vector<char> utf8(size);
                WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8.data(), size, nullptr, nullptr);
                resultPath = utf8.data();
            }
            CoTaskMemFree(pidl);
        }
    } else if (isSave) {
        // Используем GetSaveFileName для сохранения
        OPENFILENAMEW ofn = {0};
        wchar_t szFile[MAX_PATH] = {0};
        
        // Сохраняем dirWide в более широкой области видимости
        std::vector<wchar_t> dirWide;
        
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = nullptr;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = nullptr;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = nullptr;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        
        // Устанавливаем фильтр на основе расширения
        // Формат для Windows: "Description\0Pattern\0All Files\0*.*\0\0"
        int descSize = MultiByteToWideChar(CP_UTF8, 0, filterDesc.c_str(), -1, nullptr, 0);
        int patternSize = MultiByteToWideChar(CP_UTF8, 0, pattern.c_str(), -1, nullptr, 0);
        int allFilesDescSize = MultiByteToWideChar(CP_UTF8, 0, "All Files", -1, nullptr, 0);
        int allFilesPatternSize = MultiByteToWideChar(CP_UTF8, 0, "*.*", -1, nullptr, 0);
        
        std::vector<wchar_t> filterWide(descSize + patternSize + allFilesDescSize + allFilesPatternSize + 1);
        int offset = 0;
        MultiByteToWideChar(CP_UTF8, 0, filterDesc.c_str(), -1, filterWide.data() + offset, descSize);
        offset += descSize;
        MultiByteToWideChar(CP_UTF8, 0, pattern.c_str(), -1, filterWide.data() + offset, patternSize);
        offset += patternSize;
        MultiByteToWideChar(CP_UTF8, 0, "All Files", -1, filterWide.data() + offset, allFilesDescSize);
        offset += allFilesDescSize;
        MultiByteToWideChar(CP_UTF8, 0, "*.*", -1, filterWide.data() + offset, allFilesPatternSize);
        offset += allFilesPatternSize;
        filterWide[offset] = L'\0'; // Финальный null терминатор
        
        ofn.lpstrFilter = filterWide.data();
        
        // Устанавливаем начальную директорию
        if (!initialDir.empty()) {
            // Нормализуем путь: заменяем прямые слеши на обратные
            std::string normalizedDir = initialDir;
            std::replace(normalizedDir.begin(), normalizedDir.end(), '/', '\\');
            
            // Убираем завершающий обратный слеш, если есть (Windows API не требует его)
            if (!normalizedDir.empty() && normalizedDir.back() == '\\') {
                normalizedDir.pop_back();
            }
            
            int dirSize = MultiByteToWideChar(CP_UTF8, 0, normalizedDir.c_str(), -1, nullptr, 0);
            dirWide.resize(dirSize);
            MultiByteToWideChar(CP_UTF8, 0, normalizedDir.c_str(), -1, dirWide.data(), dirSize);
            ofn.lpstrInitialDir = dirWide.data();
        }
        
        if (GetSaveFileNameW(&ofn)) {
            // Конвертируем wide string в UTF-8
            int size = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, nullptr, 0, nullptr, nullptr);
            std::vector<char> utf8(size);
            WideCharToMultiByte(CP_UTF8, 0, szFile, -1, utf8.data(), size, nullptr, nullptr);
            resultPath = utf8.data();
        }
    } else {
        // Используем GetOpenFileName для открытия файла
        OPENFILENAMEW ofn = {0};
        wchar_t szFile[MAX_PATH] = {0};
        
        // Сохраняем dirWide в более широкой области видимости
        std::vector<wchar_t> dirWide;
        
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = nullptr;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = nullptr;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = nullptr;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        
        // Устанавливаем фильтр на основе расширения
        // Формат для Windows: "Description\0Pattern\0All Files\0*.*\0\0"
        int descSize = MultiByteToWideChar(CP_UTF8, 0, filterDesc.c_str(), -1, nullptr, 0);
        int patternSize = MultiByteToWideChar(CP_UTF8, 0, pattern.c_str(), -1, nullptr, 0);
        int allFilesDescSize = MultiByteToWideChar(CP_UTF8, 0, "All Files", -1, nullptr, 0);
        int allFilesPatternSize = MultiByteToWideChar(CP_UTF8, 0, "*.*", -1, nullptr, 0);
        
        std::vector<wchar_t> filterWide(descSize + patternSize + allFilesDescSize + allFilesPatternSize + 1);
        int offset = 0;
        MultiByteToWideChar(CP_UTF8, 0, filterDesc.c_str(), -1, filterWide.data() + offset, descSize);
        offset += descSize;
        MultiByteToWideChar(CP_UTF8, 0, pattern.c_str(), -1, filterWide.data() + offset, patternSize);
        offset += patternSize;
        MultiByteToWideChar(CP_UTF8, 0, "All Files", -1, filterWide.data() + offset, allFilesDescSize);
        offset += allFilesDescSize;
        MultiByteToWideChar(CP_UTF8, 0, "*.*", -1, filterWide.data() + offset, allFilesPatternSize);
        offset += allFilesPatternSize;
        filterWide[offset] = L'\0'; // Финальный null терминатор
        
        ofn.lpstrFilter = filterWide.data();
        
        // Устанавливаем начальную директорию
        if (!initialDir.empty()) {
            // Нормализуем путь: заменяем прямые слеши на обратные
            std::string normalizedDir = initialDir;
            std::replace(normalizedDir.begin(), normalizedDir.end(), '/', '\\');
            
            // Убираем завершающий обратный слеш, если есть (Windows API не требует его)
            if (!normalizedDir.empty() && normalizedDir.back() == '\\') {
                normalizedDir.pop_back();
            }
            
            int dirSize = MultiByteToWideChar(CP_UTF8, 0, normalizedDir.c_str(), -1, nullptr, 0);
            dirWide.resize(dirSize);
            MultiByteToWideChar(CP_UTF8, 0, normalizedDir.c_str(), -1, dirWide.data(), dirSize);
            ofn.lpstrInitialDir = dirWide.data();
        }
        
        if (GetOpenFileNameW(&ofn)) {
            // Конвертируем wide string в UTF-8
            int size = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, nullptr, 0, nullptr, nullptr);
            std::vector<char> utf8(size);
            WideCharToMultiByte(CP_UTF8, 0, szFile, -1, utf8.data(), size, nullptr, nullptr);
            resultPath = utf8.data();
        }
    }
    
    // Выводим результат в stdout
    if (!resultPath.empty()) {
        std::cout << resultPath;
        return 0;
    }
    
    return 1;
}

int main(int argc, char *argv[])
{
    return runFilePicker(argc, argv);
}

static std::vector<std::string> getCommandLineArgs()
{
    std::vector<std::string> args;
    LPWSTR cmdLine = GetCommandLineW();
    int nArgs = 0;
    LPWSTR* argvW = CommandLineToArgvW(cmdLine, &nArgs);
    if (!argvW || nArgs < 1) return args;
    for (int i = 0; i < nArgs; i++) {
        int len = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) continue;
        std::string s(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, &s[0], len, nullptr, nullptr);
        args.push_back(std::move(s));
    }
    LocalFree(argvW);
    return args;
}

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    std::vector<std::string> args = getCommandLineArgs();
    std::vector<char*> argvPtrs;
    for (auto& s : args) argvPtrs.push_back(&s[0]);
    int argc = static_cast<int>(argvPtrs.size());
    char** argv = argvPtrs.empty() ? nullptr : argvPtrs.data();
    return runFilePicker(argc, argv);
}

#else
// Для не-Windows платформ используем простой fallback
#include <iostream>
int main(int argc, char *argv[]) {
    std::cout << "";
    return 1;
}
#endif
