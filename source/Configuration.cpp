#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>

#include "Configuration.hpp"

namespace
{

std::string trim(const std::string& text)
{
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    const auto begin = std::find_if_not(text.begin(), text.end(), isSpace);
    const auto end = std::find_if_not(text.rbegin(), text.rend(), isSpace).base();
    return begin < end ? std::string(begin, end) : std::string();
}

std::string toLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
    return text;
}

bool parse(const std::string& text, bool& value)
{
    const std::string lower = toLower(text);
    if (lower == "1" || lower == "true" || lower == "yes" || lower == "on")
    {
        value = true;
        return true;
    }
    if (lower == "0" || lower == "false" || lower == "no" || lower == "off")
    {
        value = false;
        return true;
    }
    return false;
}

bool parse(const std::string& text, int& value, int minimum, int maximum)
{
    try
    {
        std::size_t length = 0;
        const int parsed = std::stoi(text, &length);
        if (length != text.size() || parsed < minimum || parsed > maximum)
        {
            return false;
        }
        value = parsed;
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool parse(const std::string& text, std::string& value)
{
    // Allow (but do not require) quotes around strings
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
    {
        value = text.substr(1, text.size() - 2);
    }
    else
    {
        value = text;
    }
    return true;
}

bool parse(const std::string& text, VideoFilter& value)
{
    const std::string lower = toLower(text);
    if (lower == "hd")
    {
        value = VideoFilter::Hd;
    }
    else if (lower == "pixel")
    {
        value = VideoFilter::Pixel;
    }
    else if (lower == "crt")
    {
        value = VideoFilter::Crt;
    }
    else
    {
        return false;
    }
    return true;
}

using Option = std::function<bool(const std::string&)>;

template <typename T>
Option bind(T& field)
{
    return [&field](const std::string& text) { return parse(text, field); };
}

Option bind(int& field, int minimum, int maximum)
{
    return [&field, minimum, maximum](const std::string& text) { return parse(text, field, minimum, maximum); };
}

} // namespace

Configuration Configuration::load(const std::filesystem::path& file)
{
    Configuration config;

    std::ifstream stream(file);
    if (!stream)
    {
        // No configuration file: use the defaults
        return config;
    }

    const std::map<std::string, Option> options = {
        {"audio.enabled", bind(config.audioEnabled)},
        {"audio.frequency", bind(config.audioFrequency, 8000, 192000)},
        {"audio.volume", bind(config.audioVolume, 0, 100)},
        {"game.frame_rate", bind(config.frameRate, 1, 1000)},
        {"game.rom_file", bind(config.romFile)},
        {"video.fullscreen", bind(config.fullscreen)},
        {"video.integer_scaling", bind(config.integerScaling)},
        {"video.palette_file", bind(config.paletteFile)},
        {"video.scale", bind(config.scale, 0, 16)},
        {"video.filter", bind(config.filter)},
        {"video.scanlines", [&config](const std::string& text) {
            // Older setting: the CRT effect on (1) or sharp pixels (0)
            bool scanlines = false;
            if (!parse(text, scanlines))
            {
                return false;
            }
            config.filter = scanlines ? VideoFilter::Crt : VideoFilter::Pixel;
            return true;
        }},
        {"video.vsync", bind(config.vsync)},
    };

    std::string section;
    std::string line;
    const std::string fileName = file.u8string();
    for (int lineNumber = 1; std::getline(stream, line); lineNumber++)
    {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#')
        {
            continue;
        }

        if (line.front() == '[' && line.back() == ']')
        {
            section = toLower(trim(line.substr(1, line.size() - 2)));
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
        {
            std::cerr << fileName << ":" << lineNumber << ": expected \"key = value\"\n";
            continue;
        }

        const std::string key = section + "." + toLower(trim(line.substr(0, equals)));
        const std::string value = trim(line.substr(equals + 1));
        const auto option = options.find(key);
        if (option == options.end())
        {
            std::cerr << fileName << ":" << lineNumber << ": unknown option \"" << key << "\"\n";
        }
        else if (!option->second(value))
        {
            std::cerr << fileName << ":" << lineNumber << ": invalid value \"" << value << "\" for \"" << key << "\"\n";
        }
        else
        {
            std::cout << "Configuration: " << key << " = " << value << "\n";
        }
    }

    return config;
}
