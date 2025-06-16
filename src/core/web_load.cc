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

#include "web_load.h"
#include <nlohmann/json.hpp>
#include "loader.h"
#include "ffi.h"
#include "encoding.h"
#include "http.h"
#include <unordered_set>
#include "csp_bypass.h"
#include "url_parser.h"
#include "env.h"
#include "serv.h"
#include "fvisible.h"
#include "regex.h"
unsigned long long g_hookedModuleId;

// Millennium will not load JavaScript into the following URLs to favor user safety.
// This is a list of URLs that may have sensitive information or are not safe to load JavaScript into.
static const std::vector<std::string> g_blackListedUrls = {
    "https://checkout\\.steampowered\\.com/.*"
};

// Millennium will not hook the following URLs to favor user safety. (Neither JavaScript nor CSS will be injected into these URLs.)
static const std::unordered_set<std::string> g_doNotHook = {
    R"(https?:\/\/(?:[\w-]+\.)*paypal\.com\/[^\s"']*)",
    R"(https?:\/\/(?:[\w-]+\.)*paypalobjects\.com\/[^\s"']*)",
    R"(https?:\/\/(?:[\w-]+\.)*recaptcha\.net\/[^\s"']*)",
};

std::vector<std::string> m_whiteListedRegexPaths = {};

MILLENNIUM WebkitHandler WebkitHandler::get() 
{
    static WebkitHandler webkitHandler;
    return webkitHandler;
}

MILLENNIUM void WebkitHandler::Init()
{
    m_whiteListedRegexPaths.insert(m_whiteListedRegexPaths.end(), {
        "^plugins\\/",
        "^steamui\\/",
        EscapeRegex((SystemIO::GetSteamPath() / "steamui" / "skins").generic_string()),
        EscapeRegex((SystemIO::GetSteamPath() / "ext" / "data" / "shims").generic_string()),
        EscapeRegex(GetEnv("MILLENNIUM__PLUGINS_PATH")),
    });

    this->SetupGlobalHooks();
}

MILLENNIUM void WebkitHandler::SetupGlobalHooks() 
{
    Sockets::PostGlobal({
        { "id", 3242 }, { "method", "Fetch.enable" },
        { "params", {
            { "patterns", {
                { { "urlPattern", "*" }, { "resourceType", "Document" }, { "requestStage", "Response" } },     
                /** Maintain backwards compatibility for themes that explicitly rely on this url */
                { { "urlPattern", fmt::format("{}*", this->m_oldHookAddress      ) }, { "requestStage", "Request" } },
                { { "urlPattern", fmt::format("{}*", this->m_javaScriptVirtualUrl) }, { "requestStage", "Request" } },
                { { "urlPattern", fmt::format("{}*", this->m_styleSheetVirtualUrl) }, { "requestStage", "Request" } }
            }
        }}}
    });
}

MILLENNIUM bool WebkitHandler::IsGetBodyCall(nlohmann::basic_json<> message) 
{
    std::string requestUrl = message["params"]["request"]["url"].get<std::string>();

    return requestUrl.find(this->m_javaScriptVirtualUrl) != std::string::npos
        || requestUrl.find(this->m_styleSheetVirtualUrl) != std::string::npos
        || requestUrl.find(this->m_oldHookAddress)       != std::string::npos;
}

MILLENNIUM std::filesystem::path WebkitHandler::ConvertToLoopBack(std::string requestUrl)
{
    size_t jsPos  = requestUrl.find(this->m_javaScriptVirtualUrl);
    size_t cssPos = requestUrl.find(this->m_styleSheetVirtualUrl);
    size_t oldPos = requestUrl.find(this->m_oldHookAddress);

    if (jsPos != std::string::npos)
    {
        requestUrl.erase(jsPos, std::string(this->m_javaScriptVirtualUrl).length());
    }
    else if (cssPos != std::string::npos)
    {
        requestUrl.erase(cssPos, std::string(this->m_styleSheetVirtualUrl).length());
    }
    else if (oldPos != std::string::npos)
    {
        requestUrl.erase(oldPos, std::string(this->m_oldHookAddress).length());
    }

    return std::filesystem::path(PathFromUrl(requestUrl));
}

MILLENNIUM void WebkitHandler::RetrieveRequestFromDisk(nlohmann::basic_json<> message)
{
    std::filesystem::path localFilePath = this->ConvertToLoopBack(message["params"]["request"]["url"]);

    // Check if the file path is whitelisted
    bool isWhitelisted = false;
    for (const auto& pathRegex : m_whiteListedRegexPaths) {
        if (std::regex_search(localFilePath.string(), std::regex(pathRegex))) {
            isWhitelisted = true;
            break;
        }
    }

    if (!isWhitelisted) {
        std::string errorMessage = fmt::format("Access denied: path '{}' is not in the whitelist", localFilePath.string());
        LOG_ERROR(errorMessage);
        Sockets::PostGlobal({
            { "id", 63453 },
            { "method", "Fetch.fulfillRequest" },
            { "params", {
                { "responseCode", 403 },
                { "requestId", message["params"]["requestId"] },
                { "responseHeaders", nlohmann::json::array({
                    { {"name", "Access-Control-Allow-Origin"}, {"value", "*"} },
                    { {"name", "Content-Type"}, {"value", "text/plain"} }
                })},
                { "responsePhrase", errorMessage },
                { "body", Base64Encode(errorMessage) }
            }}
        });
        return;
    }


    std::string fileContent;
    std::ifstream localFileStream(localFilePath);

    bool bFailedRead = !localFileStream.is_open();
    if (bFailedRead)
    {
        LOG_ERROR("failed to retrieve file '{}' info from disk.", localFilePath.string());
    }

    uint16_t    responseCode    = bFailedRead ? 404 : 200;
    std::string responseMessage = bFailedRead ? "millennium couldn't read " + localFilePath.string() : "OK millennium";
    eFileType   fileType        = EvaluateFileType(localFilePath.string());

    if (IsBinaryFile(fileType)) 
    {
        try
        {
            fileContent = Base64Encode(SystemIO::ReadFileBytesSync(localFilePath.string()));
        }
        catch(const std::exception& error)
        {
            LOG_ERROR("Failed to read file bytes from disk: {}", error.what());
            bFailedRead = true; /** Force fail even if the file exists. */
        }
        
    } 
    else 
    {
        fileContent = Base64Encode(std::string(
            std::istreambuf_iterator<char>(localFileStream),
            std::istreambuf_iterator<char>()
        ));
    }

    const auto responseHeaders = nlohmann::json::array
    ({
        { {"name", "Access-Control-Allow-Origin"}, {"value", "*"} },
        { {"name", "Content-Type"}, {"value", fileTypes[fileType]} }
    });

    Sockets::PostGlobal
    ({
        { "id", 63453 },
        { "method", "Fetch.fulfillRequest" },
        { "params", {
            { "responseCode", responseCode },
            { "requestId", message["params"]["requestId"] },
            { "responseHeaders", responseHeaders },
            { "responsePhrase", responseMessage },
            { "body", fileContent }
        }}
    });
}

MILLENNIUM void WebkitHandler::GetResponseBody(nlohmann::basic_json<> message)
{
    const RedirectType statusCode = message["params"]["responseStatusCode"].get<RedirectType>();

    const auto ContinueOriginalRequest = ([message]() {
        Sockets::PostGlobal({
            { "id", 0 },
            { "method", "Fetch.continueRequest" },
            { "params", { { "requestId", message["params"]["requestId"] } }}
        });
    });

    // Check if the request URL is a do-not-hook URL.
    for (const auto& doNotHook : g_doNotHook) 
    {
        if (std::regex_match(message["params"]["request"]["url"].get<std::string>(), std::regex(doNotHook))) 
        {
            ContinueOriginalRequest();
            return;
        }
    }

    // If the status code is a redirect, we just continue the request. 
    if (statusCode == REDIRECT || statusCode == MOVED_PERMANENTLY || statusCode == FOUND || statusCode == TEMPORARY_REDIRECT || statusCode == PERMANENT_REDIRECT)
    {
        ContinueOriginalRequest();
    }
    else
    {
        hookMessageId -= 1;
        m_requestMap->push_back({ hookMessageId, message["params"]["requestId"], message["params"]["resourceType"], message });

        Sockets::PostGlobal({
            { "id", hookMessageId },
            { "method", "Fetch.getResponseBody" },
            { "params", { { "requestId", message["params"]["requestId"] } }}
        });
    }
}

MILLENNIUM const std::string WebkitHandler::PatchDocumentContents(std::string requestUrl, std::string original) 
{
    std::string patched = original;
    std::optional<std::string> millenniumPreloadPath = SystemIO::GetMillenniumPreloadPath();

    if (!millenniumPreloadPath.has_value()) 
    {
        LOG_ERROR("Missing webkit preload module. Please re-install Millennium.");
        #ifdef _WIN32
        MessageBoxA(NULL, "Missing webkit preload module. Please re-install Millennium.", "Millennium", MB_ICONERROR);
        #endif
        return patched;
    }

    std::vector<std::string> scriptModules;
    std::string cssShimContent, scriptModuleArray;
    std::string linkPreloadsArray;

    for (auto& hookItem : *m_hookListPtr) 
    {
        if (hookItem.type == TagTypes::STYLESHEET) 
        {
            if (!std::regex_match(requestUrl, hookItem.urlPattern)) 
                continue;

            cssShimContent.append(fmt::format("<link rel=\"stylesheet\" href=\"{}\">\n", UrlFromPath("https://css.millennium.app/", hookItem.path))); 
        }
        else if (hookItem.type == TagTypes::JAVASCRIPT) 
        {
            if (!std::regex_match(requestUrl, hookItem.urlPattern)) 
                continue;

            auto jsPath = UrlFromPath(this->m_javaScriptVirtualUrl, hookItem.path);
            scriptModules.push_back(jsPath);
            linkPreloadsArray.append(fmt::format("<link rel=\"modulepreload\" href=\"{}\" fetchpriority=\"high\">\n", jsPath));
        }
    }

    for (int i = 0; i < scriptModules.size(); i++)
    {
        scriptModuleArray.append(fmt::format("\"{}\"{}", scriptModules[i], (i == scriptModules.size() - 1 ? "" : ",")));
    }

    const std::string ftpPath = UrlFromPath(m_javaScriptVirtualUrl, millenniumPreloadPath.value_or(std::string()));
    const std::string scriptContent = fmt::format("(new module.default).StartPreloader({}, [{}]);", m_ipcPort, scriptModuleArray);

    linkPreloadsArray.insert(0, fmt::format("<link rel=\"modulepreload\" href=\"{}\" fetchpriority=\"high\">\n", ftpPath));

    std::string importScript = fmt::format("import('{}').then(module => {{ {} }}).catch(error => window.location.reload())", ftpPath, scriptContent);
    std::string shimContent = fmt::format("{}<script type=\"module\" async id=\"millennium-injected\">{}</script>\n{}", linkPreloadsArray, importScript, cssShimContent);

    for (const auto& blackListedUrl : g_blackListedUrls)        
    {
        if (std::regex_match(requestUrl, std::regex(blackListedUrl))) 
        {
            shimContent = cssShimContent; // Remove all queried JavaScript from the page. 
        }
    }

    if (patched.find("<head>") == std::string::npos) 
    {
        return patched;
    }

    return patched.replace(patched.find("<head>"), 6, "<head>" + shimContent);
}

MILLENNIUM void WebkitHandler::HandleHooks(nlohmann::basic_json<> message)
{
    for (auto requestIterator = m_requestMap->begin(); requestIterator != m_requestMap->end();)
    {
        try 
        {
            auto [id, requestId, type, response] = (*requestIterator);

            if (message["id"] != id || (message["id"] == id && !message["result"]["base64Encoded"]))
            {
                requestIterator++;
                continue;
            }

            const std::string patchedContent = this->PatchDocumentContents(response["params"]["request"]["url"], Base64Decode(message["result"]["body"]));

            BypassCSP();

            const int responseCode = response["params"].value("responseStatusCode", 200);
            const std::string responseMessage = response["params"].value("responseStatusText", "OK");

            Sockets::PostGlobal({
                { "id", 63453 },
                { "method", "Fetch.fulfillRequest" },
                { "params", {
                    { "requestId", requestId },
                    { "responseCode", responseCode },
                    { "responseHeaders", response["params"]["responseHeaders"] },
                    { "responsePhrase", responseMessage.length() > 0 ? responseMessage : "OK" },
                    { "body", Base64Encode(patchedContent) }
                }}
            });

            requestIterator = m_requestMap->erase(requestIterator);
        }
        catch (const nlohmann::detail::exception& ex) 
        {
            LOG_ERROR("error hooking WebKit -> {}\n\n{}", ex.what(), message.dump(4));
            requestIterator = m_requestMap->erase(requestIterator);
        }
        catch (const std::exception& ex) 
        {
            LOG_ERROR("error hooking WebKit -> {}\n\n{}", ex.what(), message.dump(4));
            requestIterator = m_requestMap->erase(requestIterator);
        }
    }
}

MILLENNIUM void WebkitHandler::DispatchSocketMessage(nlohmann::basic_json<> message)
{
    static std::chrono::time_point lastExceptionTime = std::chrono::system_clock::now();

    try 
    {
        if (message["method"] == "Fetch.requestPaused")
        {
            switch (this->IsGetBodyCall(message))
            {
                case true:  { this->RetrieveRequestFromDisk(message); break; }
                case false: { this->GetResponseBody(message);         break; }
            }
        }

        this->HandleHooks(message);
    }
    catch (const nlohmann::detail::exception& ex) 
    {
        if (std::chrono::system_clock::now() - lastExceptionTime > std::chrono::seconds(5)) 
        {
            LOG_ERROR("error dispatching socket message -> {}", ex.what());
            lastExceptionTime = std::chrono::system_clock::now();
        }
    }
    catch (const std::exception& ex) 
    {
        if (std::chrono::system_clock::now() - lastExceptionTime > std::chrono::seconds(5)) 
        {
            LOG_ERROR("error dispatching socket message -> {}", ex.what());
            lastExceptionTime = std::chrono::system_clock::now();
        }
    }
}
