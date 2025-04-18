/**
 * ==================================================
 *   _____ _ _ _             _                     
 *  |     |_| | |___ ___ ___|_|_ _ _____           
 *  | | | | | | | -_|   |   | | | |     |          
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
 * 
 * ==================================================
 * 
 * Copyright (c) 2025 Project Millennium
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define UNICODE
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 
#include <winsock2.h>
#define _WINSOCKAPI_
#include <DbgHelp.h>
#endif
#include <filesystem>
#include <fstream>
#include <fmt/core.h>
#include <log.h>
#include "loader.h"
#include "co_spawn.h"
#include <serv.h>
#include <signal.h>
#include <cxxabi.h>
#include "terminal_pipe.h"
#include "executor.h"
#include <env.h>

#ifdef __linux__
extern "C" int IsSamePath(const char *path1, const char *path2);
#endif

/**
 * @brief Verify the environment to ensure that the CEF remote debugging is enabled.
 * .cef-enable-remote-debugging is a special file name that Steam uses to signal CEF to enable remote debugging.
 */
const static void VerifyEnvironment() 
{
    const auto filePath = SystemIO::GetSteamPath() / ".cef-enable-remote-debugging";

    // Steam's CEF Remote Debugger isn't exposed to port 8080
    if (!std::filesystem::exists(filePath)) 
    {
        std::ofstream(filePath).close();

        Logger.Log("Successfully enabled CEF remote debugging, you can now restart Steam...");
        std::exit(1);
    }

    // Check if the user has set a Steam.cfg file to block updates, this is incompatible with Millennium as Millennium relies on the latest version of Steam. 
    const auto steamUpdateBlock = SystemIO::GetSteamPath() / "Steam.cfg";

    const std::string errorMessage = fmt::format("Millennium is incompatible with your {} config. This is a file you likely created to block Steam updates. In order for Millennium to properly function, remove it.", steamUpdateBlock.string());

    if (std::filesystem::exists(steamUpdateBlock)) 
    {
        try
        {
            std::string steamConfig = SystemIO::ReadFileSync(steamUpdateBlock.string());

            std::vector<std::string> blackListedKeys = { 
                "BootStrapperInhibitAll",
                "BootStrapperForceSelfUpdate",
                "BootStrapperInhibitClientChecksum",
                "BootStrapperInhibitBootstrapperChecksum",
                "BootStrapperInhibitUpdateOnLaunch",
            };

            for (const auto& key : blackListedKeys) 
            {
                if (steamConfig.find(key) != std::string::npos) 
                {
                    throw SystemIO::FileException("Steam.cfg contains blacklisted keys");
                }
            }
        }
        catch(const SystemIO::FileException& e)
        {
            #ifdef _WIN32
            MessageBoxA(NULL, errorMessage.c_str(), "Startup Error", MB_ICONERROR | MB_OK);
            #endif

            LOG_ERROR(errorMessage);
        }
    }
}

#include <exception>

#ifdef _WIN32
#include <windows.h>
// Helper function to demangle C++ names for MinGW
std::string DemangleName(const char* mangledName) 
{
    int status = 0;
    std::unique_ptr<char, void(*)(void*)> demangled(abi::__cxa_demangle(mangledName, nullptr, nullptr, &status), std::free);
    
    if (status == 0 && demangled) 
    {
        return std::string(demangled.get());
    } 
    else 
    {
        // If demangling fails, return the original name
        return std::string(mangledName);
    }
}

void CaptureStackTrace(std::string& errorMessage, int maxFrames = 256)
{
    HANDLE process = GetCurrentProcess();
    DWORD options = SymGetOptions();
    options |= SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS;
    options &= ~SYMOPT_UNDNAME;
    SymSetOptions(options);
    
    if (!SymInitialize(process, NULL, TRUE)) 
    {
        DWORD error = GetLastError();
        errorMessage.append(fmt::format("\nFailed to initialize symbol handler: Error {}\n", error));
        return;
    }
    
    void* stack[maxFrames];
    USHORT frames = CaptureStackBackTrace(0, maxFrames, stack, NULL);
    
    constexpr int MAX_NAME_LENGTH = 1024;
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(1, sizeof(SYMBOL_INFO) + MAX_NAME_LENGTH * sizeof(CHAR));

    if (!symbol) 
    {
        errorMessage.append("\nFailed to allocate memory for symbols\n");
        SymCleanup(process);
        return;
    }
    
    symbol->MaxNameLen = MAX_NAME_LENGTH - 1;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    
    errorMessage.append(fmt::format("\nStack trace ({} frames):\n", frames));
    
    for (USHORT i = 0; i < frames; i++) {
        DWORD64 address = (DWORD64)(stack[i]);
        DWORD64 displacement = 0;
        
        BOOL symbolResult = SymFromAddr(process, address, &displacement, symbol);
        
        if (symbolResult) 
        {
            // Try to demangle it for MinGW-compiled code
            std::string demangledName;
            
            // Check if this looks like a C++ mangled name (typically starts with _Z for GCC/MinGW)
            if (symbol->Name[0] == '_' && (symbol->Name[1] == 'Z' || symbol->Name[1] == 'N')) 
            {
                demangledName = DemangleName(symbol->Name);
            } 
            else 
            {
                // Not a GCC-style mangled name, use as is
                demangledName = symbol->Name;
            }
            
            // Get file and line info
            DWORD lineDisplacement = 0;
            BOOL lineResult = SymGetLineFromAddr64(process, address, &lineDisplacement, &line);
            
            if (lineResult) 
            {
                errorMessage.append(fmt::format("#{}: {} in {} at {}:{} +{}\n", i, demangledName, line.FileName, line.LineNumber, lineDisplacement));
            } 
            else 
            {
                errorMessage.append(fmt::format("#{}: {} at 0x{:X} (no line info, error: {})\n", i, demangledName, address, GetLastError()));
            }   
        } 
        else 
        {
            errorMessage.append(fmt::format("#{}: 0x{:X} (unknown symbol, error: {})\n", i, address, GetLastError()));
        }
    }
    
    free(symbol);
    SymCleanup(process);
}
#endif

/**
 * @brief Custom terminate handler for Millennium.
 * This function is called when Millennium encounters a fatal error that it can't recover from.
 */
void OnTerminate() 
{
    #ifdef _WIN32
    if (IsDebuggerPresent()) __debugbreak();
    #endif

    auto const exceptionPtr = std::current_exception();
    std::string errorMessage = "Millennium has a fatal error that it can't recover from, check the logs for more details!\n";

    if (exceptionPtr) 
    {
        try 
        {
            int status;
            errorMessage.append(fmt::format("Terminating with uncaught exception of type `{}`", abi::__cxa_demangle(abi::__cxa_current_exception_type()->name(), 0, 0, &status)));
            std::rethrow_exception(exceptionPtr);
        }
        catch (const std::exception& e) 
        {
            errorMessage.append(fmt::format(" with `what()` = \"{}\"", e.what()));
        }
        catch (...) { }
    }

    #ifdef _WIN32
    // Capture and print stack trace
    CaptureStackTrace(errorMessage);
    MessageBoxA(NULL, errorMessage.c_str(), "Oops!", MB_ICONERROR | MB_OK);

    auto result = MessageBoxA(
        NULL, 
        "Would you like to open your logs folder?\nLook for a file called \"Standard Output_log.log\" "
        "in the logs folder, that will have important debug information.\n\n"
        "Please send this file to Millennium developers on our discord (steambrew.app/discord), it will help prevent this error from happening to you or others in the future.", 
        "Error Recovery",
        MB_ICONEXCLAMATION| MB_YESNO
    );

    if (result == IDYES) 
    {
        std::string logPath = (SystemIO::GetInstallPath() / "ext" / "logs").string();
        ShellExecuteA(NULL, "open", logPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

    #elif __linux__
    std::cerr << errorMessage << std::endl;
    #endif
}

/**
 * @brief Millennium's main method, called on startup on both Windows and Linux.
 */
const static void EntryMain() 
{
    #if _WIN32
    SetConsoleTitleA(std::string("Millennium@" + std::string(MILLENNIUM_VERSION)).c_str());
    SetupEnvironmentVariables();
    if (!IsDebuggerPresent()) 
    {
        std::set_terminate(OnTerminate); // Set custom terminate handler for easier debugging
    }
    #endif
    
    /** Handle signal interrupts (^C) */
    signal(SIGINT, [](int signalCode) { std::exit(128 + SIGINT); });

    #ifdef _WIN32
    /**
    * Windows requires a special environment setup to redirect stdout to a pipe.
    * This is necessary for the logger component to capture stdout from Millennium.
    * This is also necessary to update the updater module from cache.
    */
    WinUtils::SetupWin32Environment();  
    #endif 

    /**
     * Create an FTP server to allow plugins to be loaded from the host machine.
     */
    uint16_t ftpPort = Crow::CreateAsyncServer();

    const auto startTime = std::chrono::system_clock::now();
    VerifyEnvironment();

    std::shared_ptr<PluginLoader> loader = std::make_shared<PluginLoader>(startTime, ftpPort);
    SetPluginLoader(loader);

    PythonManager& manager = PythonManager::GetInstance();

    /** Start the python backends */
    std::thread(&PluginLoader::StartBackEnds, loader, std::ref(manager)).detach();

    /** Start the injection process into the Steam web helper */
    loader->StartFrontEnds();
}

__attribute__((constructor)) void __init_millennium() 
{
    #if defined(__linux__)
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        const char* pathPtr = path;

        // Check if the path is the same as the Steam executable
        if (!IsSamePath(pathPtr, fmt::format("{}/.local/share/Steam/ubuntu12_32/steam", std::getenv("HOME")).c_str()) != 0) {
            return;
        }
    } 
    else {
        perror("readlink");
        return;
    }
    #endif

    SetupEnvironmentVariables();
}

#ifdef _WIN32
std::unique_ptr<std::thread> g_millenniumThread;
/**
 * @brief Entry point for Millennium on Windows.
 * @param fdwReason The reason for calling the DLL.
 * @return True if the DLL was successfully loaded, false otherwise.
 */
int __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) 
    {
        case DLL_PROCESS_ATTACH: 
        {
            g_millenniumThread = std::make_unique<std::thread>(EntryMain);
            break;
        }
        case DLL_PROCESS_DETACH: 
        {
            WinUtils::RestoreStdout();
            // Logger.PrintMessage(" MAIN ", "Shutting Millennium down...", COL_MAGENTA);

            // g_threadTerminateFlag->flag.store(true);
            // Sockets::Shutdown();
            // g_millenniumThread->join();
            std::exit(0);
            break;
        }
    }

    return true;
}

#elif __linux__
#include <stdio.h>
#include <stdlib.h>
#include "helpers.h"
#include <dlfcn.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <stdexcept>

extern "C"
{
    /* Trampoline for the real main() */
    static int (*fnMainOriginal)(int, char **, char **);
    std::unique_ptr<std::thread> g_millenniumThread;

    /** 
     * Since this isn't an executable, and is "preloaded", the kernel doesn't implicitly load dependencies, so we need to manually. 
    */
    static constexpr const char* __LIBPYTHON_RUNTIME_PATH = LIBPYTHON_RUNTIME_PATH;

    /* Our fake main() that gets called by __libc_start_main() */
    int MainHooked(int argc, char **argv, char **envp)
    {
        Logger.Log("Hooked main() with PID: {}", getpid());
        Logger.Log("Loading python libraries from {}", __LIBPYTHON_RUNTIME_PATH);

        if (!dlopen(__LIBPYTHON_RUNTIME_PATH, RTLD_LAZY | RTLD_GLOBAL)) 
        {
            LOG_ERROR("Failed to load python libraries: {},\n\nThis is likely because it was not found on disk, try reinstalling Millennium.", dlerror());
        }

        #ifdef MILLENNIUM_SHARED
        {
            /** Start Millennium on a new thread to prevent I/O blocking */
            g_millenniumThread = std::make_unique<std::thread>(EntryMain);
            int steam_main = fnMainOriginal(argc, argv, envp);
            Logger.Log("Hooked Steam entry returned {}", steam_main);

            g_threadTerminateFlag->flag.store(true);
            Sockets::Shutdown();
            g_millenniumThread->join();

            Logger.Log("Shutting down Millennium...");
            return steam_main;
        }
        #else
        {
            g_threadTerminateFlag->flag.store(true);
            g_millenniumThread = std::make_unique<std::thread>(EntryMain);
            g_millenniumThread->join();
            return 0;
        }
        #endif
    }

    void RemoveFromLdPreload() 
    {
        const char* ldPreload = getenv("LD_PRELOAD");
        if (!ldPreload)  
        {
            fprintf(stderr, "LD_PRELOAD is not set.\n");
            return;
        }

        char* ldPreloadStr = strdup(ldPreload);
        if (!ldPreloadStr) 
        {
            perror("strdup");
            return;
        }

        char* token, *rest = ldPreloadStr;
        size_t tokenCount = 0, tokenArraySize = 8; 
        char** tokens = (char**)malloc(tokenArraySize * sizeof(char*));

        if (!tokens) 
        {
            perror("malloc");
            free(ldPreloadStr);
            return;
        }

        while ((token = strtok_r(rest, " ", &rest))) 
        {
            if (strcmp(token, GetEnv("MILLENNIUM_RUNTIME_PATH").c_str()) != 0) 
            {
                if (tokenCount >= tokenArraySize) 
                {
                    tokenArraySize *= 2;
                    char** temp = (char**)realloc(tokens, tokenArraySize * sizeof(char*));
                    if (!temp) 
                    {
                        perror("realloc");
                        free(ldPreloadStr);
                        free(tokens);
                        return;
                    }
                    tokens = temp;
                }
                tokens[tokenCount++] = token;
            }
        }

        size_t newSize = 0;
        for (size_t i = 0; i < tokenCount; ++i) 
        {
            newSize += strlen(tokens[i]) + 1;
        }

        char* updatedLdPreload = (char*)malloc(newSize > 0 ? newSize : 1);
        if (!updatedLdPreload) 
        {
            perror("malloc");
            free(ldPreloadStr);
            free(tokens);
            return;
        }

        updatedLdPreload[0] = '\0';
        for (size_t i = 0; i < tokenCount; ++i) 
        {
            if (i > 0) 
            {
                strcat(updatedLdPreload, " ");
            }
            strcat(updatedLdPreload, tokens[i]);
        }

        printf("Updating LD_PRELOAD from [%s] to [%s]\n", ldPreloadStr, updatedLdPreload);

        if (setenv("LD_PRELOAD", updatedLdPreload, 1) != 0) 
        {
            perror("setenv");
        }

        free(ldPreloadStr);
        free(updatedLdPreload);
        free(tokens);
    }
    #ifdef MILLENNIUM_SHARED

    int IsSamePath(const char *path1, const char *path2) 
    {
        char realpath1[PATH_MAX], realpath2[PATH_MAX];
        struct stat stat1, stat2;

        // Get the real paths for both paths (resolves symlinks)
        if (realpath(path1, realpath1) == NULL) 
        {
            perror("realpath failed for path1");
            return 0;  // Error in resolving path
        }
        if (realpath(path2, realpath2) == NULL) 
        {
            perror("realpath failed for path2");
            return 0;  // Error in resolving path
        }

        // Compare resolved paths
        if (strcmp(realpath1, realpath2) != 0) 
        {
            return 0;  // Paths are different
        }

        // Check if both paths are symlinks and compare symlink targets
        if (lstat(path1, &stat1) == 0 && lstat(path2, &stat2) == 0) 
        {
            if (S_ISLNK(stat1.st_mode) && S_ISLNK(stat2.st_mode)) 
            {
                // Both are symlinks, compare the target paths
                char target1[PATH_MAX], target2[PATH_MAX];
                ssize_t len1 = readlink(path1, target1, sizeof(target1) - 1);
                ssize_t len2 = readlink(path2, target2, sizeof(target2) - 1);

                if (len1 == -1 || len2 == -1) 
                {
                    perror("readlink failed");
                    return 0;
                }

                target1[len1] = '\0';
                target2[len2] = '\0';

                // Compare the symlink targets
                if (strcmp(target1, target2) != 0) 
                {
                    return 0;  // Symlinks point to different targets
                }
            }
        }

        return 1;  // Paths are the same, including symlinks to each other
    }

    /*
    * Trampoline for __libc_start_main() that replaces the real main
    * function with our hooked version.
    */
    int __libc_start_main(
        int (*main)(int, char **, char **), int argc, char **argv,
        int (*init)(int, char **, char **), void (*fini)(void), void (*rtld_fini)(void), void *stack_end)
    {
        Logger.Log("Hooked main() with PID: {}", getpid());

        /* Save the real main function address */
        fnMainOriginal = main;

        /* Get the address of the real __libc_start_main() */
        decltype(&__libc_start_main) orig = (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

        /** not loaded in a invalid child process */
        if (!IsSamePath(argv[0], GetEnv("MILLENNIUM__STEAM_EXE_PATH").c_str()))
        {
            return orig(main, argc, argv, init, fini, rtld_fini, stack_end);
        }

        Logger.Log("Hooked __libc_start_main() {}", argv[0]);

        /* Remove the Millennium library from LD_PRELOAD */
        RemoveFromLdPreload();
        /* Log that we've loaded Millennium */
        Logger.Log("Loaded Millennium on {}, system architecture {}", GetLinuxDistro(), GetSystemArchitecture());
        /* ... and call it with our custom main function */
        return orig(MainHooked, argc, argv, init, fini, rtld_fini, stack_end);
    }
    #endif
}

int main(int argc, char **argv, char **envp)
{
    return MainHooked(argc, argv, envp);
}
#endif
