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

/**
 * Enum representing the different types of files.
 * 
 * @enum {number}
 * @readonly
 * @property {number} StyleSheet - Represents a CSS file.
 * @property {number} JavaScript - Represents a JavaScript file.
 * @property {number} Json - Represents a JSON file.
 * @property {number} Python - Represents a Python file.
 * @property {number} Other - Represents other file types.
 */
enum eFileType
{
    css,
    js,
    json,
    py,
    ttf,
    otf,
    woff,
    woff2,
    png,
    jpeg,
    jpg,
    gif,
    webp,
    svg,
    html,
    unknown,
};

/**
 * A map that associates each file type from the `eFileType` enum to its corresponding MIME type.
 * 
 * - `StyleSheet` maps to "text/css"
 * - `JavaScript` maps to "application/javascript"
 * - `Json` maps to "application/json"
 * - `Python` maps to "text/x-python"
 * - `Other` maps to "text/plain"
 */
static std::map<eFileType, std::string> fileTypes
{
    { eFileType::css,     "text/css"               },
    { eFileType::js,      "application/javascript" },
    { eFileType::json,    "application/json"       },
    { eFileType::py,      "text/x-python"          },
    { eFileType::ttf,     "font/ttf"               },
    { eFileType::otf,     "font/otf"               },
    { eFileType::woff,    "font/woff"              },
    { eFileType::woff2,   "font/woff2"             },
    { eFileType::png,     "image/png"              },
    { eFileType::jpeg,    "image/jpeg"             },
    { eFileType::jpg,     "image/jpeg"             },
    { eFileType::gif,     "image/gif"              },
    { eFileType::webp,    "image/webp"             },
    { eFileType::svg,     "image/svg+xml"          },
    { eFileType::html,    "text/html"              },
    { eFileType::unknown, "text/plain"             },

};

/**
 * Checks if the file type is a binary file.
 * 
 * @param {eFileType} fileType - The type of the file to check.
 * @returns {boolean} - `true` if the file type is binary, `false` otherwise.
 */
static constexpr bool IsBinaryFile(eFileType fileType)
{
    return fileType == eFileType::ttf
        || fileType == eFileType::otf
        || fileType == eFileType::woff
        || fileType == eFileType::woff2
        || fileType == eFileType::png
        || fileType == eFileType::jpeg
        || fileType == eFileType::jpg
        || fileType == eFileType::gif
        || fileType == eFileType::webp
        || fileType == eFileType::svg
        || fileType == eFileType::html
        || fileType == eFileType::unknown;
}

/**
 * Evaluates the file type based on the file extension.
 *
 * @param {std::filesystem::path} filePath - The path to the file to evaluate.
 * @returns {eFileType} - The type of the file, which could be one of the `eFileType` values.
 * 
 * The function checks the file extension and returns the corresponding file type:
 * - `.css` → `eFileType::StyleSheet`
 * - `.js` → `eFileType::JavaScript`
 * - `.json` → `eFileType::Json`
 * - `.py` → `eFileType::Python`
 * - For any other extension, `eFileType::Other` is returned.
 */
static const eFileType EvaluateFileType(std::filesystem::path filePath)
{
    const std::string extension = filePath.extension().string();

    if      (extension == ".css"  ) { return eFileType::css;   }
    else if (extension == ".js"   ) { return eFileType::js;    }
    else if (extension == ".json" ) { return eFileType::json;  }
    else if (extension == ".py"   ) { return eFileType::py;    }
    else if (extension == ".ttf"  ) { return eFileType::ttf;   }
    else if (extension == ".otf"  ) { return eFileType::otf;   }
    else if (extension == ".woff" ) { return eFileType::woff;  }
    else if (extension == ".woff2") { return eFileType::woff2; }
    else if (extension == ".png"  ) { return eFileType::png;   }
    else if (extension == ".jpeg" ) { return eFileType::jpeg;  }
    else if (extension == ".jpg"  ) { return eFileType::jpg;   }
    else if (extension == ".gif"  ) { return eFileType::gif;   }
    else if (extension == ".webp" ) { return eFileType::webp;  }
    else if (extension == ".svg"  ) { return eFileType::svg;   }
    else if (extension == ".html") { return eFileType::html;  }

    else
    {
        return eFileType::unknown;
    }
}

namespace Crow 
{
    uint16_t CreateAsyncServer();
}
