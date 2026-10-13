/**
 * @file
 * @brief Entry point: loads the ROM image and runs the game.
 */
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <vector>

#include <SDL.h>

#include "Frontend/AudioOutput.hpp"
#include "Frontend/Display.hpp"
#include "Frontend/Input.hpp"
#include "SMB/SMBEngine.hpp"

#include "Configuration.hpp"
#include "Constants.hpp"

namespace
{

/**
 * Load the graphics (CHR) data from a Super Mario Bros. ROM image.
 *
 * @throws std::runtime_error if the file cannot be read or is not the right ROM image.
 */
std::vector<uint8_t> loadChr(const std::string& fileName)
{
    constexpr std::size_t HEADER_SIZE = 16;
    constexpr std::size_t PRG_SIZE = 2 * 0x4000;

    // File names are UTF-8 (SDL passes command-line arguments as UTF-8 on Windows)
    std::ifstream file(std::filesystem::u8path(fileName), std::ios::binary);
    if (!file)
    {
        throw std::runtime_error(
            "Unable to open the ROM image \"" + fileName + "\".\n\n"
            "Drop a Super Mario Bros. ROM image onto the game's executable, pass its path as an "
            "argument, or set game.rom_file in " + CONFIG_FILE_NAME + ".");
    }

    const std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // iNES header with 2 PRG banks (16 KiB each) and 1 CHR bank (8 KiB)
    if (rom.size() < HEADER_SIZE + PRG_SIZE + SMBEngine::CHR_SIZE ||
        std::memcmp(rom.data(), "NES\x1a", 4) != 0 || rom[4] != 2 || rom[5] != 1)
    {
        throw std::runtime_error("\"" + fileName + "\" is not a Super Mario Bros. ROM image.");
    }

    const auto chr = rom.begin() + HEADER_SIZE + PRG_SIZE;
    return std::vector<uint8_t>(chr, chr + SMBEngine::CHR_SIZE);
}

/**
 * Report an error that stops the game. It is also shown in a message box, since the console
 * is not visible (or closes immediately) when the game is started from a file manager.
 */
void showError(const std::string& message)
{
    std::cerr << message << "\n";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, APP_TITLE, message.c_str(), nullptr);
}

/**
 * Keeps the game running at a steady frame rate.
 */
class FrameTimer
{
public:
    explicit FrameTimer(int frameRate) :
        frequency(static_cast<double>(SDL_GetPerformanceFrequency())),
        ticksPerFrame(frequency / frameRate),
        nextFrame(static_cast<double>(SDL_GetPerformanceCounter()))
    {
    }

    /**
     * Wait until it is time for the next frame.
     */
    void wait()
    {
        nextFrame += ticksPerFrame;
        const double now = static_cast<double>(SDL_GetPerformanceCounter());
        if (now < nextFrame)
        {
            SDL_Delay(static_cast<Uint32>((nextFrame - now) * 1000.0 / frequency));
        }
        else if (now - nextFrame > ticksPerFrame)
        {
            // Running behind (e.g. the window was being dragged): carry on from now rather
            // than running at super-speed to catch up
            nextFrame = now;
        }
    }

private:
    double frequency;
    double ticksPerFrame;
    double nextFrame;
};

void run(const Configuration& config, const std::vector<uint8_t>& chr)
{
    Display display(config, chr.data());

    std::optional<AudioOutput> audio;
    if (config.audioEnabled)
    {
        audio.emplace(config.audioFrequency, config.audioVolume);
    }
    const bool audioOpen = audio && audio->isOpen();

    SMBEngine engine(chr.data(), audioOpen ? config.audioFrequency : 0, config.frameRate);
    if (!config.paletteFile.empty())
    {
        if (const auto palette = loadPalette(config.paletteFile))
        {
            engine.setPalette(*palette);
        }
    }
    engine.reset();

    Input input;
    FrameTimer timer(config.frameRate);
    std::vector<uint32_t> frame(RENDER_WIDTH * RENDER_HEIGHT);

    bool running = true;
    bool paused = false;
    bool pausedByFocusLoss = false;
    const auto setPaused = [&](bool pause) {
        paused = pause;
        pausedByFocusLoss = false;
        if (audioOpen)
        {
            audio->setPaused(pause);
        }
    };

    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            input.handleEvent(event);

            if (event.type == SDL_QUIT)
            {
                running = false;
            }
            else if (event.type == SDL_KEYDOWN && !event.key.repeat)
            {
                switch (event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                case SDLK_RETURN:
                    if (event.key.keysym.mod & KMOD_ALT)
                    {
                        display.toggleFullscreen();
                    }
                    break;
                case SDLK_F11:
                case SDLK_f:
                    display.toggleFullscreen();
                    break;
                case SDLK_F2:
                    display.toggleCrtEffect();
                    break;
                case SDLK_p:
                case SDLK_PAUSE:
                    setPaused(!paused);
                    break;
                case SDLK_r:
                    engine.reset();
                    setPaused(false);
                    break;
                case SDLK_m:
                    if (audioOpen)
                    {
                        audio->toggleMute();
                    }
                    break;
                default:
                    break;
                }
            }
            else if (event.type == SDL_WINDOWEVENT)
            {
                // Pause while the window is in the background
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST && !paused)
                {
                    setPaused(true);
                    pausedByFocusLoss = true;
                }
                else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED && pausedByFocusLoss)
                {
                    setPaused(false);
                }
            }
        }

        if (!paused)
        {
            input.update(engine);
            engine.update();
            engine.render(frame.data());
            if (audioOpen)
            {
                audio->play(engine.getAudioSamples());
            }
        }

        display.present(frame.data(), paused);
        timer.wait();
    }
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc > 2 || (argc == 2 && (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0)))
    {
        std::cout << "Usage: " << argv[0] << " [ROM image]\n\n"
                  << "Settings are read from " << CONFIG_FILE_NAME << " in the working directory.\n";
        return argc > 2 ? 1 : 0;
    }

    Configuration config = Configuration::load(CONFIG_FILE_NAME);
    if (argc == 2)
    {
        config.romFile = argv[1];
    }

    int result = 0;
    try
    {
        const std::vector<uint8_t> chr = loadChr(config.romFile);

        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            throw SdlError("SDL_Init()");
        }
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0)
        {
            std::cerr << "Gamepads are unavailable: " << SDL_GetError() << "\n";
        }

        run(config, chr);
    }
    catch (const std::exception& error)
    {
        showError(error.what());
        result = 1;
    }

    SDL_Quit();
    return result;
}
