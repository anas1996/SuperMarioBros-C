/**
 * @file
 * @brief User configuration, read from an INI file.
 */
#ifndef CONFIGURATION_HPP
#define CONFIGURATION_HPP

#include <filesystem>
#include <string>

/**
 * How the picture is drawn.
 */
enum class VideoFilter
{
    Hd,    /**< Pixel art redrawn at high resolution with smooth edges */
    Pixel, /**< Sharp square pixels, as on an emulator */
    Crt    /**< Scanlines and phosphor stripes, like an old television */
};

/**
 * Program options. Each field can be set in the configuration file as "key = value"
 * within the INI section named in its comment.
 */
struct Configuration
{
    // [audio]
    bool audioEnabled = true;   /**< enabled */
    int audioFrequency = 48000; /**< frequency: sample rate in Hz */
    int audioVolume = 100;      /**< volume: 0-100 */

    // [game]
    int frameRate = 60;                                            /**< frame_rate: frames per second */
    std::string romFile = "Super Mario Bros. (JU) (PRG0) [!].nes"; /**< rom_file */

    // [video]
    VideoFilter filter = VideoFilter::Hd; /**< filter: hd, pixel or crt */
    bool fullscreen = false;              /**< fullscreen: start in fullscreen mode */
    bool integerScaling = false;          /**< integer_scaling: only scale by whole numbers */
    std::string paletteFile;              /**< palette_file: optional .pal file */
    int scale = 0;                        /**< scale: initial window size, as a multiple of 256x240 (0: fit the screen) */
    bool vsync = true;                    /**< vsync */

    /**
     * Load the configuration from a file. Options that are missing (or the whole file)
     * keep their default values; invalid values are reported and ignored.
     */
    static Configuration load(const std::filesystem::path& file);
};

#endif // CONFIGURATION_HPP
