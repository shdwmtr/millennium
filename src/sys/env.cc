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

/**
 * @file env.cc
 * 
 * @brief This file is responsible for setting up environment variables that are used throughout the application.
 */

#include "env.h"
#include <string> 
#include <locals.h>
#include <fmt/core.h>
#include <log.h>
#include <stdlib.h>
#include <unistd.h>
#include "fvisible.h"

const void SetupEnvironmentVariables();

void SetEnv(const std::string& key, const std::string& value) 
{
    #ifdef _WIN32
    _putenv_s(key.c_str(), value.c_str());  // Windows
    #else
    setenv(key.c_str(), value.c_str(), 1);  // Linux/macOS (1 = overwrite existing)
    #endif
}
    
void UnsetEnv(const std::string& key) 
{
    #ifdef _WIN32
    _putenv_s(key.c_str(), "");  // Windows: Setting to empty removes it
    #else
    unsetenv(key.c_str());  // Linux/macOS
    #endif
}

std::string GetEnv(std::string key)
{
    #ifdef _WIN32
    static bool hasSetup = false;

    if (!hasSetup) 
    {
        hasSetup = true;
        SetupEnvironmentVariables();
    }
    #endif

    char* szVariable = getenv(key.c_str());

    if (szVariable == nullptr)
    {
        return {};
    }

    std::string strVariable(szVariable);
    return strVariable;
}

std::string GetEnvWithFallback(std::string key, std::string fallback)
{
    std::string var = GetEnv(key);
    return var.empty() ? fallback : var;
}

/**
 * @brief Set up environment variables used throughout the application.
 */
const void SetupEnvironmentVariables()
{
    std::map<std::string, std::string> environment = {
        { "MILLENNIUM__VERSION",     MILLENNIUM_VERSION },
        { "MILLENNIUM__STEAM_PATH",   SystemIO::GetSteamPath()  .string() }
    };

    #ifdef _WIN32
    std::map<std::string, std::string> environment_windows = {
        { "MILLENNIUM__PLUGINS_PATH",   SystemIO::GetInstallPath().string() + "/plugins" },
        { "MILLENNIUM__CONFIG_PATH",    SystemIO::GetInstallPath().string() + "/ext" },
        { "MILLENNIUM__LOGS_PATH",      SystemIO::GetInstallPath().string() + "/ext/logs" },
        { "MILLENNIUM__DATA_LIB",       SystemIO::GetInstallPath().string() + "/ext/data" },
        { "MILLENNIUM__PYTHON_ENV",     SystemIO::GetInstallPath().string() + "/ext/data/cache" },
        { "MILLENNIUM__SHIMS_PATH",     SystemIO::GetInstallPath().string() + "/ext/data/shims" },
        { "MILLENNIUM__ASSETS_PATH",    SystemIO::GetInstallPath().string() + "/ext/data/assets" },
        { "MILLENNIUM__INSTALL_PATH",   SystemIO::GetInstallPath().string() }
    };
    environment.insert(environment_windows.begin(), environment_windows.end());
    #elif __linux__
    const std::string homeDir   = GetEnv("HOME");
    const std::string configDir = GetEnvWithFallback("XDG_CONFIG_HOME", fmt::format("{}/.config", homeDir));
    const std::string dataDir   = GetEnvWithFallback("XDG_DATA_HOME", fmt::format("{}/.local/share", homeDir));
    const std::string stateDir  = GetEnvWithFallback("XDG_STATE_HOME", fmt::format("{}/.local/state", homeDir));
    const static std::string pythonEnv = fmt::format("{}/millennium/.venv", dataDir);
    const std::string pythonEnvBin = fmt::format("{}/bin/python3.11", pythonEnv);

    if (access(pythonEnvBin.c_str(), F_OK) == -1) {
        std::system(fmt::format("{}/bin/python3.11 -m venv {} --system-site-packages --symlinks", MILLENNIUM__PYTHON_ENV, pythonEnv).c_str());
    }

    std::map<std::string, std::string> environment_unix = {
        { "MILLENNIUM_RUNTIME_PATH", "/usr/lib/millennium/libmillennium_x86.so" },
        { "LIBPYTHON_RUNTIME_PATH",  LIBPYTHON_RUNTIME_PATH },

        { "MILLENNIUM__STEAM_EXE_PATH", fmt::format("{}/.steam/steam/ubuntu12_32/steam",     homeDir) },
        { "MILLENNIUM__PLUGINS_PATH",   fmt::format("{}/millennium/plugins",    dataDir) },
        { "MILLENNIUM__CONFIG_PATH",    fmt::format("{}/millennium",            configDir) },
        { "MILLENNIUM__LOGS_PATH",      fmt::format("{}/millennium/logs",       stateDir) },
        { "MILLENNIUM__DATA_LIB",       "/usr/share/millennium" },
        { "MILLENNIUM__SHIMS_PATH",     "/usr/share/millennium/shims" },
        { "MILLENNIUM__ASSETS_PATH",    "/usr/share/millennium/assets" },
        
        { "MILLENNIUM__UPDATE_SCRIPT_PROMPT", MILLENNIUM__UPDATE_SCRIPT_PROMPT }, /** The script the user will run to update millennium. */
        
        { "MILLENNIUM__PYTHON_ENV",             pythonEnv },
        { "LIBPYTHON_RUNTIME_BIN_PATH",         pythonEnvBin },
        { "LIBPYTHON_BUILTIN_MODULES_PATH",     fmt::format("{}/lib/python3.11",             MILLENNIUM__PYTHON_ENV) },
        { "LIBPYTHON_BUILTIN_MODULES_DLL_PATH", fmt::format("{}/lib/python3.11/lib-dynload", MILLENNIUM__PYTHON_ENV) }
    };
    environment.insert(environment_unix.begin(), environment_unix.end());
    #endif

    for (const auto& [key, value] : environment)
    {
        #ifdef __linux__
        #define RED "\033[31m"
        #define RESET "\033[0m"

        std::cout << fmt::format("{}={}", key, value) << std::endl;
        #endif
        SetEnv(key, value);
    }
}