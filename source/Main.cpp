#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <vector>

#include <SDL2/SDL.h>

#include "Emulation/Controller.hpp"
#include "SMB/SMBEngine.hpp"
#include "Util/Video.hpp"

#include "Configuration.hpp"
#include "Constants.hpp"

static SDL_Window* window;
static SDL_Renderer* renderer;
static SDL_Texture* texture;
static SDL_Texture* scanlineTexture;
static SDL_AudioDeviceID audioDevice;

/**
 * Load the Super Mario Bros. ROM image.
 */
static std::optional<std::vector<uint8_t>> loadRomImage(const std::string& fileName)
{
    std::ifstream file(fileName, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to open the file \"" << fileName << "\". Exiting.\n";
        return std::nullopt;
    }

    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // iNES header (16 bytes) + 2 PRG banks (16 KiB each) + CHR (8 KiB)
    if (rom.size() < 16 + 2 * 0x4000 + SMBEngine::CHR_SIZE || rom[0] != 'N' || rom[1] != 'E' || rom[2] != 'S' || rom[3] != 0x1a)
    {
        std::cerr << "\"" << fileName << "\" is not a Super Mario Bros. ROM image. Exiting.\n";
        return std::nullopt;
    }
    return rom;
}

/**
 * Initialize libraries for use.
 */
static bool initialize(const Configuration& config)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
        std::cerr << "SDL_Init() failed during initialize(): " << SDL_GetError() << std::endl;
        return false;
    }

    window = SDL_CreateWindow(APP_TITLE,
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              RENDER_WIDTH * config.scale,
                              RENDER_HEIGHT * config.scale,
                              0);
    if (window == nullptr)
    {
        std::cerr << "SDL_CreateWindow() failed during initialize(): " << SDL_GetError() << std::endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, (config.vsync ? SDL_RENDERER_PRESENTVSYNC : 0) | SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr)
    {
        std::cerr << "SDL_CreateRenderer() failed during initialize(): " << SDL_GetError() << std::endl;
        return false;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RENDER_WIDTH, RENDER_HEIGHT);
    if (texture == nullptr)
    {
        std::cerr << "SDL_CreateTexture() failed during initialize(): " << SDL_GetError() << std::endl;
        return false;
    }

    if (config.scanlines)
    {
        scanlineTexture = generateScanlineTexture(renderer);
    }

    if (config.audioEnabled)
    {
        SDL_AudioSpec desiredSpec{};
        desiredSpec.freq = config.audioFrequency;
        desiredSpec.format = AUDIO_S8;
        desiredSpec.channels = 1;
        desiredSpec.samples = 2048;

        audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, nullptr, 0);
        if (audioDevice == 0)
        {
            std::cerr << "Unable to open audio device: " << SDL_GetError() << std::endl;
        }
        else
        {
            SDL_PauseAudioDevice(audioDevice, 0);
        }
    }

    return true;
}

/**
 * Shutdown libraries for exit.
 */
static void shutdown()
{
    if (audioDevice != 0)
    {
        SDL_CloseAudioDevice(audioDevice);
    }

    SDL_DestroyTexture(scanlineTexture);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}

static void mainLoop(const Configuration& config, const std::vector<uint8_t>& rom)
{
    SMBEngine engine(rom.data() + 16 + 2 * 0x4000, audioDevice != 0 ? config.audioFrequency : 0, config.frameRate);
    if (!config.paletteFile.empty())
    {
        if (const auto palette = loadPalette(config.paletteFile))
        {
            engine.setPalette(*palette);
        }
    }
    engine.reset();

    std::vector<uint32_t> renderBuffer(RENDER_WIDTH * RENDER_HEIGHT);
    bool running = true;
    uint32_t progStartTime = SDL_GetTicks();
    int frame = 0;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        Controller& controller1 = engine.getController(0);
        controller1.setButtonState(Button::A, keys[SDL_SCANCODE_X]);
        controller1.setButtonState(Button::B, keys[SDL_SCANCODE_Z]);
        controller1.setButtonState(Button::Select, keys[SDL_SCANCODE_BACKSPACE]);
        controller1.setButtonState(Button::Start, keys[SDL_SCANCODE_RETURN]);
        controller1.setButtonState(Button::Up, keys[SDL_SCANCODE_UP]);
        controller1.setButtonState(Button::Down, keys[SDL_SCANCODE_DOWN]);
        controller1.setButtonState(Button::Left, keys[SDL_SCANCODE_LEFT]);
        controller1.setButtonState(Button::Right, keys[SDL_SCANCODE_RIGHT]);

        if (keys[SDL_SCANCODE_R])
        {
            engine.reset();
        }
        if (keys[SDL_SCANCODE_ESCAPE])
        {
            running = false;
            break;
        }
        if (keys[SDL_SCANCODE_F])
        {
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        }

        engine.update();
        engine.render(renderBuffer.data());

        if (audioDevice != 0)
        {
            const std::vector<uint8_t>& samples = engine.getAudioSamples();
            SDL_QueueAudio(audioDevice, samples.data(), static_cast<Uint32>(samples.size()));
        }

        SDL_UpdateTexture(texture, nullptr, renderBuffer.data(), sizeof(uint32_t) * RENDER_WIDTH);

        SDL_RenderClear(renderer);

        // Render the screen
        SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH, RENDER_HEIGHT);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);

        // Render scanlines
        if (scanlineTexture != nullptr)
        {
            SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH * 3, RENDER_HEIGHT * 3);
            SDL_RenderCopy(renderer, scanlineTexture, nullptr, nullptr);
        }

        SDL_RenderPresent(renderer);

        /**
         * Ensure that the framerate stays as close to the desired FPS as possible. If the frame was rendered faster, then delay.
         * If the frame was slower, reset time so that the game doesn't try to "catch up", going super-speed.
         */
        const uint32_t now = SDL_GetTicks();
        const int delay = static_cast<int>(progStartTime + frame * 1000.0 / config.frameRate) - static_cast<int>(now);
        if (delay > 0)
        {
            SDL_Delay(delay);
        }
        else
        {
            frame = 0;
            progStartTime = now;
        }
        frame++;
    }
}

int main(int argc, char** argv)
{
    const Configuration config = Configuration::load(CONFIG_FILE_NAME);

    const auto rom = loadRomImage(config.romFile);
    if (!rom || !initialize(config))
    {
        std::cerr << "Failed to initialize. Please check previous error messages for more information. The program will now exit.\n";
        return -1;
    }

    mainLoop(config, *rom);

    shutdown();

    return 0;
}
