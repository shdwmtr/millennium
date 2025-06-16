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

#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <mutex>
#include <regex>

extern unsigned long long g_hookedModuleId;

extern std::vector<std::string> m_whiteListedRegexPaths;

class WebkitHandler 
{
public:
    static WebkitHandler get();

    enum TagTypes {
        STYLESHEET,
        JAVASCRIPT
    };

    struct HookType {
        std::string path;
        std::regex urlPattern;
        TagTypes type;
        unsigned long long id;
    };

    enum RedirectType {
        REDIRECT = 301,
        MOVED_PERMANENTLY = 302,
        FOUND = 303,
        TEMPORARY_REDIRECT = 307,
        PERMANENT_REDIRECT = 308
    };

    std::shared_ptr<std::vector<HookType>> m_hookListPtr = std::make_shared<std::vector<HookType>>();

    void Init();

    void DispatchSocketMessage(nlohmann::basic_json<> message);
    void SetupGlobalHooks();

    void SetIPCPort(uint16_t ipcPort) { m_ipcPort = ipcPort; }
    void SetFTPPort(uint16_t ftpPort) { m_ftpPort = ftpPort; }

private:
    uint16_t m_ipcPort, m_ftpPort;
    long long hookMessageId = -69;

    /** Maintain backwards compatibility for themes that explicitly rely on this url */
    const char* m_oldHookAddress       = "https://pseudo.millennium.app/";

    /** New hook URLS (1/21/2025) */
    const char* m_javaScriptVirtualUrl = "https://js.millennium.app/";
    const char* m_styleSheetVirtualUrl = "https://css.millennium.app/";

    bool IsGetBodyCall(nlohmann::basic_json<> message);

    std::string HandleCssHook(std::string body);
    std::string HandleJsHook(std::string body);

    const std::string PatchDocumentContents(std::string requestUrl, std::string original);
    void HandleHooks(nlohmann::basic_json<> message);

    void RetrieveRequestFromDisk(nlohmann::basic_json<> message);
    void GetResponseBody(nlohmann::basic_json<> message);

    std::filesystem::path ConvertToLoopBack(std::string requestUrl);

    struct WebHookItem {
        long long id;
        std::string requestId;
        std::string type;
        nlohmann::basic_json<> message;
    };

    std::shared_ptr<std::vector<WebHookItem>> m_requestMap = std::make_shared<std::vector<WebHookItem>>();
};