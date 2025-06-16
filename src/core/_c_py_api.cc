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

#include "executor.h"
#include "co_spawn.h"
#include "ffi.h"
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fstream>
#include "log.h"
#include "locals.h"
#include "web_load.h"
#include "co_stub.h"
#include "logger.h"
#include "encoding.h"
#include "fvisible.h"
#include "regex.h"

std::shared_ptr<PluginLoader> g_pluginLoader;

std::map<std::string, JavaScript::Types> typeMap = {
    { "str",  JavaScript::Types::String  },
    { "bool", JavaScript::Types::Boolean },
    { "int",  JavaScript::Types::Integer }
};

MILLENNIUM PyObject* GetUserSettings(PyObject* self, PyObject* args)
{
    PyErr_SetString(PyExc_NotImplementedError, "get_user_settings is not implemented yet. It will likely be removed in the future.");
    return NULL;
}

MILLENNIUM PyObject* SetUserSettings(PyObject* self, PyObject* args)
{
    PyErr_SetString(PyExc_NotImplementedError, "set_user_settings_key is not implemented yet. It will likely be removed in the future.");
    return NULL;
}

MILLENNIUM PyObject* CallFrontendMethod(PyObject* self, PyObject* args, PyObject* kwargs)
{
    const char* methodName = NULL;
    PyObject* parameterList = NULL;

    static const char* keywordArgsList[] = { "method_name", "params", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|O", (char**)keywordArgsList, &methodName, &parameterList)) 
    {
        return NULL;
    }

    std::vector<JavaScript::JsFunctionConstructTypes> params;

    if (parameterList != NULL)
    {
        if (!PyList_Check(parameterList))
        {
            PyErr_SetString(PyExc_TypeError, "params must be a list");
            return NULL;
        }

        Py_ssize_t listSize = PyList_Size(parameterList);

        for (Py_ssize_t i = 0; i < listSize; ++i) 
        {
            PyObject* listItem = PyList_GetItem(parameterList, i);
            const std::string strValue  = PyUnicode_AsUTF8(PyObject_Str(listItem));
            const std::string valueType = Py_TYPE(listItem)->tp_name;

            try 
            {
                params.push_back({ strValue, typeMap[valueType] });
            }
            catch (const std::exception&) 
            {
                PyErr_SetString(PyExc_TypeError, "Millennium's IPC can only handle [bool, str, int]");
                return NULL;
            }
        }
    }

    PyObject* globals = PyModule_GetDict(PyImport_AddModule("__main__"));
    PyObject* pluginNameObj = PyRun_String("MILLENNIUM_PLUGIN_SECRET_NAME", Py_eval_input, globals, globals);

    if (pluginNameObj == nullptr || PyErr_Occurred()) 
    {
        LOG_ERROR("error getting plugin name, can't make IPC request. this is likely a millennium bug.");
        return NULL;
    }

    const std::string pluginName = PyUnicode_AsUTF8(PyObject_Str(pluginNameObj));
    const std::string script = JavaScript::ConstructFunctionCall(pluginName.c_str(), methodName, params);

    return JavaScript::EvaluateFromSocket(
        // Check the the frontend code is actually loaded aside from SteamUI
        fmt::format(
            "if (typeof window !== 'undefined' && typeof window.MillenniumFrontEndError === 'undefined') {{ window.MillenniumFrontEndError = class MillenniumFrontEndError extends Error {{ constructor(message) {{ super(message); this.name = 'MillenniumFrontEndError'; }} }} }}"
            "if (typeof PLUGIN_LIST === 'undefined' || !PLUGIN_LIST?.['{}']) throw new window.MillenniumFrontEndError('frontend not loaded yet!');\n\n{}", 
            pluginName, 
            script
        ) 
    );
}

MILLENNIUM PyObject* GetVersionInfo(PyObject* self, PyObject* args) 
{ 
    return PyUnicode_FromString(MILLENNIUM_VERSION);
}

MILLENNIUM PyObject* GetSteamPath(PyObject* self, PyObject* args) 
{
    return PyUnicode_FromString(SystemIO::GetSteamPath().string().c_str()); 
}

MILLENNIUM PyObject* GetInstallPath(PyObject* self, PyObject* args) 
{
    return PyUnicode_FromString(SystemIO::GetInstallPath().string().c_str()); 
}

MILLENNIUM PyObject* RemoveBrowserModule(PyObject* self, PyObject* args) 
{ 
    int moduleId;

    if (!PyArg_ParseTuple(args, "i", &moduleId)) 
    {
        return NULL;
    }

    bool success = false;
    const auto moduleList = WebkitHandler::get().m_hookListPtr;

    for (auto it = moduleList->begin(); it != moduleList->end();)
    {
        if (it->id == moduleId) 
        {
            it = moduleList->erase(it);
            success = true;
        } 
        else ++it;
    }
    return PyBool_FromLong(success);
}

MILLENNIUM unsigned long long AddBrowserModule(PyObject* args, WebkitHandler::TagTypes type) 
{
    const char* moduleItem;
    const char* regexSelector = ".*"; // Default value if no second parameter is provided

    // Parse arguments: moduleItem is required, regexSelector is optional
    if (!PyArg_ParseTuple(args, "s|s", &moduleItem, &regexSelector)) 
    {
        return 0;
    }

    g_hookedModuleId++;
    auto path = SystemIO::GetSteamPath() / "steamui" / moduleItem;
    auto webkitHandler = WebkitHandler::get();

    try 
    {
        webkitHandler.m_hookListPtr->push_back({ path.generic_string(), std::regex(regexSelector), type, g_hookedModuleId });
        m_whiteListedRegexPaths.push_back(EscapeRegex(path.generic_string()));
    } 
    catch (const std::regex_error& e) 
    {
        LOG_ERROR("Attempted to add a browser module with invalid regex: {} ({})", regexSelector, e.what());
        ErrorToLogger("executor", fmt::format("Failed to add browser module with invalid regex: {} ({})", regexSelector, e.what()));
        return 0;
    }

    return g_hookedModuleId;
}

MILLENNIUM PyObject* AddBrowserCss(PyObject* self, PyObject* args) 
{ 
    return PyLong_FromLong((long)AddBrowserModule(args, WebkitHandler::TagTypes::STYLESHEET)); 
}

MILLENNIUM PyObject* AddBrowserJs(PyObject* self, PyObject* args)  
{ 
    return PyLong_FromLong((long)AddBrowserModule(args, WebkitHandler::TagTypes::JAVASCRIPT)); 
}

/* 
This portion of the API is undocumented but you can use it. 
*/
MILLENNIUM PyObject* TogglePluginStatus(PyObject* self, PyObject* args)
{
    PyObject* input;  // Will only accept a list format
    PythonManager& manager = PythonManager::GetInstance();
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();
    
    if (!PyArg_ParseTuple(args, "O", &input))
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to parse parameters");
        return NULL;
    }
    
    // Only accept list format
    if (!PyList_Check(input))
    {
        PyErr_SetString(PyExc_TypeError, "Argument must be a list of dictionaries with 'plugin_name' and 'enabled' fields");
        return NULL;
    }
    
    // Dictionary to store plugin name to status mapping
    std::unordered_map<std::string, bool> pluginStatusMap;
    Py_ssize_t listSize = PyList_Size(input);
    
    for (Py_ssize_t i = 0; i < listSize; i++)
    {
        PyObject* item = PyList_GetItem(input, i);
        
        if (!PyDict_Check(item))
        {
            PyErr_SetString(PyExc_TypeError, "List items must be dictionaries");
            return NULL;
        }
        
        PyObject* nameObj = PyDict_GetItemString(item, "plugin_name");
        PyObject* enabledObj = PyDict_GetItemString(item, "enabled");
        
        if (!nameObj || !PyUnicode_Check(nameObj))
        {
            PyErr_SetString(PyExc_TypeError, "Each dictionary must have a 'plugin_name' string key");
            return NULL;
        }
        
        if (!enabledObj || !PyBool_Check(enabledObj))
        {
            PyErr_SetString(PyExc_TypeError, "Each dictionary must have an 'enabled' boolean key");
            return NULL;
        }
        
        const char* pluginName = PyUnicode_AsUTF8(nameObj);
        const bool newStatus = PyObject_IsTrue(enabledObj);
        
        pluginStatusMap[pluginName] = newStatus;
    }
    
    // Now handle all the plugin status changes
    bool hasEnableRequests = false;
    std::vector<std::string> pluginsToDisable;
    
    // Update settings and prepare lists
    for (const auto& entry : pluginStatusMap)
    {
        const std::string& pluginName = entry.first;
        const bool newStatus = entry.second;
        
        settingsStore->TogglePluginStatus(pluginName.c_str(), newStatus);
        
        if (newStatus)
        {
            hasEnableRequests = true;
            Logger.Log("requested to enable plugin [{}]", pluginName);
        }
        else
        {
            pluginsToDisable.push_back(pluginName);
            Logger.Log("requested to disable plugin [{}]", pluginName);
        }
    }
    
    // Handle the actual operations
    if (hasEnableRequests)
    {
        std::thread([&manager] { g_pluginLoader->StartBackEnds(manager); }).detach();
    }
    
    for (const auto& pluginName : pluginsToDisable)
    {
        std::thread([pluginName, &manager] { 
            manager.DestroyPythonInstance(pluginName.c_str()); 
        }).detach();
    }
    
    CoInitializer::ReInjectFrontendShims(g_pluginLoader, false);
    Py_RETURN_NONE;
}

MILLENNIUM PyObject* EmitReadyMessage(PyObject* self, PyObject* args) 
{ 
    PyObject* globals = PyModule_GetDict(PyImport_AddModule("__main__"));
    PyObject* pluginNameObj = PyRun_String("MILLENNIUM_PLUGIN_SECRET_NAME", Py_eval_input, globals, globals);

    if (pluginNameObj == nullptr || PyErr_Occurred()) 
    {
        LOG_ERROR("error getting plugin name, can't make IPC request. this is likely a millennium bug.");
        return NULL;
    }

    const std::string pluginName = PyUnicode_AsUTF8(PyObject_Str(pluginNameObj));

    CoInitializer::BackendCallbacks& backendHandler = CoInitializer::BackendCallbacks::getInstance();
    backendHandler.BackendLoaded({ pluginName, CoInitializer::BackendCallbacks::BACKEND_LOAD_SUCCESS });

    return PyBool_FromLong(true);
}

MILLENNIUM PyObject* GetPluginLogs(PyObject* self, PyObject* args) 
{
    nlohmann::json logData = nlohmann::json::array();
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();

    std::vector<SettingsStore::PluginTypeSchema> plugins = settingsStore->ParseAllPlugins();

    for (auto& logger : g_loggerList) 
    {
        nlohmann::json logDataItem;

        for (auto [message, logLevel] : logger->CollectLogs()) 
        {
            logDataItem.push_back({
                { "message", Base64Encode(message) },
                { "level", logLevel }
            });
        }

        std::string pluginName = logger->GetPluginName(false);

        for (auto& plugin : plugins) 
        {
            if (plugin.pluginJson.contains("name") && plugin.pluginJson["name"] == logger->GetPluginName(false)) 
            {
                pluginName = plugin.pluginJson.value("common_name", pluginName);
                break;
            }
        }

        // Handle package manager plugin
        if (pluginName == "pipx") 
        {
            pluginName = "Package Manager";
        }

        logData.push_back({
            { "name", pluginName },
            { "logs", logDataItem }
        });
    }

    return PyUnicode_FromString(logData.dump().c_str());
}

MILLENNIUM PyObject* AddProxyPattern(PyObject* self, PyObject* args)
{
    const char* regexPath;
    
    if (!PyArg_ParseTuple(args, "s", &regexPath)) {
        return NULL;
    }
    
    try {
        Logger.Log("Adding whitelisted regex path: {}", regexPath);
        m_whiteListedRegexPaths.push_back(regexPath);
    }
    catch (const std::regex_error& e) 
    {
        LOG_ERROR("Failed to add whitelisted regex path with invalid regex: {} ({})", regexPath, e.what());
        ErrorToLogger("executor", fmt::format("Failed to add whitelisted regex path with invalid regex: {} ({})", regexPath, e.what()));
        return NULL;
    }

    Py_RETURN_NONE;
}

MILLENNIUM PyObject* RemoveProxyPattern(PyObject* self, PyObject* args)
{
    const char* regexPath;
    
    if (!PyArg_ParseTuple(args, "s", &regexPath)) {
        return NULL;
    }
    
    try {
        m_whiteListedRegexPaths.erase(std::remove(m_whiteListedRegexPaths.begin(), m_whiteListedRegexPaths.end(), regexPath), m_whiteListedRegexPaths.end());
    }
    catch (const std::regex_error& e) 
    {
        LOG_ERROR("Failed to remove whitelisted regex path with invalid regex: {} ({})", regexPath, e.what());
        ErrorToLogger("executor", fmt::format("Failed to remove whitelisted regex path with invalid regex: {} ({})", regexPath, e.what()));
        return NULL;
    }

    Py_RETURN_NONE;
}

// Helper to convert month abbreviation to number
std::string getMonthNumber(const std::string& monthAbbr) 
{
    static std::map<std::string, std::string> monthMap 
    {
        {"Jan", "01"}, {"Feb", "02"}, {"Mar", "03"}, {"Apr", "04"},
        {"May", "05"}, {"Jun", "06"}, {"Jul", "07"}, {"Aug", "08"},
        {"Sep", "09"}, {"Oct", "10"}, {"Nov", "11"}, {"Dec", "12"}
    };
    return monthMap[monthAbbr];
}

std::string getBuildTimestamp() 
{
#ifdef __DATE__
    std::string date = __DATE__;
#else
    std::string date = "Jan 01 1970";
#endif

#ifdef __TIME__
    std::string time = __TIME__;
#else
    std::string time = "00:00:00";
#endif

    std::string month = getMonthNumber(date.substr(0, 3));
    std::string day = date.substr(4, 2);
    std::string year = date.substr(7, 4);

    if (day[0] == ' ') day[0] = '0';
    return year + "-" + month + "-" + day + "T" + time;
}

MILLENNIUM PyObject* GetBuildDate(PyObject* self, PyObject* args)
{
    return PyUnicode_FromString(getBuildTimestamp().c_str());
}

/** 
 * Method API for the Millennium module
 * This is injected individually into each plugins Python backend, enabling them to interop with Millennium's internal API.
 */
MILLENNIUM PyMethodDef* GetMillenniumModule()
{
    static PyMethodDef moduleMethods[] = 
    {
        /** Called *one time* after your plugin has finished bootstrapping. Its used to let Millennium know what plugins crashes/loaded etc.  */
        { "ready",                 EmitReadyMessage,                METH_NOARGS,  NULL },

        /** Add a CSS file to the browser webkit hook list */
        { "add_browser_css",       AddBrowserCss,                   METH_VARARGS, NULL },
        /** Add a JavaScript file to the browser webkit hook list */
        { "add_browser_js",        AddBrowserJs,                    METH_VARARGS, NULL },
        /** Remove a CSS or JavaScript file, passing the ModuleID provided from add_browser_js/css */
        { "remove_browser_module", RemoveBrowserModule,             METH_VARARGS, NULL },

        { "get_user_settings",     GetUserSettings,                 METH_NOARGS,  NULL },
        { "set_user_settings_key", SetUserSettings,                 METH_VARARGS, NULL },
        /** Get the version of Millennium. In semantic versioning format. */
        { "version",               GetVersionInfo,                  METH_NOARGS,  NULL },
        /** Get the path to the Steam directory */
        { "steam_path",            GetSteamPath,                    METH_NOARGS,  NULL },
        /** Get the path to the Millennium install directory */
        { "get_install_path",      GetInstallPath,                  METH_NOARGS,  NULL },
        /** Get all the current stored logs from all loaded and previously loaded plugins during this instance */
        { "get_plugin_logs" ,      GetPluginLogs,                   METH_NOARGS, NULL },
        /** Add a regex path to the proxy whitelist */
        { "add_proxy_pattern",     AddProxyPattern,                 METH_VARARGS, NULL },
        /** Remove a regex path from the proxy whitelist */
        { "remove_proxy_pattern",  RemoveProxyPattern,              METH_VARARGS, NULL },

        /** Call a JavaScript method on the frontend. */
        { "call_frontend_method",  (PyCFunction)CallFrontendMethod, METH_VARARGS | METH_KEYWORDS, NULL },
        /** 
         * @note Internal Use Only 
         * Used to toggle the status of a plugin, used in the Millennium settings page.
        */
        { "change_plugin_status",  TogglePluginStatus,              METH_VARARGS, NULL },

        /** For internal use, but can be used if its useful */
        { "__internal_get_build_date",  GetBuildDate,               METH_VARARGS, NULL },
        {NULL, NULL, 0, NULL} // Sentinel
    };

    return moduleMethods;
}

MILLENNIUM void SetPluginLoader(std::shared_ptr<PluginLoader> pluginLoader) 
{
    g_pluginLoader = pluginLoader;
}