#include <exception>

#include <SDL.h>

#include "Files.hpp"

std::filesystem::path toPath(const std::string& name)
{
    try
    {
        return std::filesystem::u8path(name);
    }
    catch (const std::exception&)
    {
        // Not UTF-8 (only Windows checks): try the native encoding, as the name was written
    }
    try
    {
        return std::filesystem::path(name);
    }
    catch (const std::exception&)
    {
        return std::filesystem::path();
    }
}

std::filesystem::path findFile(const std::string& name)
{
    const std::filesystem::path path = toPath(name);
    std::error_code error;
    if (path.empty() || path.is_absolute() || std::filesystem::exists(path, error))
    {
        return path;
    }

    if (char* basePath = SDL_GetBasePath())
    {
        const std::filesystem::path executableDirectory = toPath(basePath);
        SDL_free(basePath);
        if (!executableDirectory.empty() && std::filesystem::exists(executableDirectory / path, error))
        {
            return executableDirectory / path;
        }
    }
    return path;
}

std::string displayName(const std::filesystem::path& path)
{
    try
    {
        std::error_code error;
        const std::filesystem::path absolute = std::filesystem::absolute(path, error);
        return (error ? path : absolute).u8string();
    }
    catch (const std::exception&)
    {
        return "?";
    }
}
