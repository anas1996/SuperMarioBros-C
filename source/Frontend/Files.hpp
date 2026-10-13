/**
 * @file
 * @brief Locating the files the game reads.
 */
#ifndef FILES_HPP
#define FILES_HPP

#include <filesystem>
#include <string>

/**
 * Convert a file name from the command line or the configuration file to a path. Names are
 * UTF-8; one that is not (e.g. from a configuration file saved in a legacy encoding) is used
 * in the system's native encoding instead. Never throws: a name that cannot be converted
 * either way gives an empty path.
 */
std::filesystem::path toPath(const std::string& name);

/**
 * Find a file by name. A relative name is looked up in the working directory, then next to the
 * executable (where the ROM image and configuration file usually are when the game is started
 * from a file manager). If it is in neither place, the path in the working directory is returned.
 * Never throws (see toPath()).
 */
std::filesystem::path findFile(const std::string& name);

/**
 * A path as UTF-8 text, for messages.
 */
std::string displayName(const std::filesystem::path& path);

#endif // FILES_HPP
