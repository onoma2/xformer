#include <SDL2/SDL.h>
#ifdef HAS_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif
#include <iostream>

#include "AlgoVisualization.h"
#include "AlgorithmProcessor.h"
#include "AudioEngine.h"

// Audio Callback Wrapper
void audioCallback(void* userdata, Uint8* stream, int len) {
    AudioEngine* engine = static_cast<AudioEngine*>(userdata);
    engine->process(reinterpret_cast<float*>(stream), len / sizeof(float), 48000); // Assuming 48k for now
}

class AlgoTesterApp {
public:
    AlgoTesterApp() :
        m_window(nullptr),
        m_renderer(nullptr),
#ifdef HAS_SDL2_TTF
        m_font(nullptr),
#endif
        m_running(false),
        m_selectedAlgorithm(AlgorithmType::Test),
        m_selectedVisualization(VisualizationType::NoteSequence)
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
            return;
        }

#ifdef HAS_SDL2_TTF
        if (TTF_Init() == -1) {
            std::cerr << "TTF could not initialize! TTF_Error: " << TTF_GetError() << std::endl;
            // We can continue without TTF
        } else {
            // Try to load a font
            m_font = TTF_OpenFont("/System/Library/Fonts/SFNS.ttf", 14);
            if (!m_font) m_font = TTF_OpenFont("/System/Library/Fonts/Arial.ttf", 14);
            if (!m_font) m_font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 14);
            if (!m_font) std::cerr << "Font could not be loaded!" << std::endl;
        }
#endif

        // Detect DPI to adjust initial window size appropriately
        float dpi = 96.0f; // standard DPI
        if (SDL_GetDisplayDPI(0, nullptr, &dpi, nullptr) == 0) {
            m_dpi = dpi;
        } else {
            // If DPI detection fails, default to standard DPI
            m_dpi = 96.0f;
        }

        // Calculate an appropriate window size based on DPI
        float dpiScale = dpi / 96.0f;
        int windowWidth = static_cast<int>(1400 / std::max(1.0f, dpiScale * 0.5f));  // Don't make window too small
        int windowHeight = static_cast<int>(900 / std::max(1.0f, dpiScale * 0.5f));

        m_window = SDL_CreateWindow("PEW|FORMER Algorithm Tester v1.0",
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   windowWidth, windowHeight,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!m_window) {
            std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return;
        }
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
        if (!m_renderer) {
            std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return;
        }

        // Initialize visualization
        m_visualization = std::make_unique<AlgoVisualization>();

        // Initialize algorithm processor
        m_processor = std::make_unique<AlgorithmProcessor>(64); // 64-step sequences

        // Initialize Audio
        m_audioEngine.init();

        m_running = true;
        SDL_StartTextInput();
    }

    ~AlgoTesterApp() {
        SDL_StopTextInput();
        m_audioEngine.cleanup();

#ifdef HAS_SDL2_TTF
        if (m_font) TTF_CloseFont(m_font);
        TTF_Quit();
#endif
        if (m_renderer) SDL_DestroyRenderer(m_renderer);
        if (m_window) SDL_DestroyWindow(m_window);
        SDL_Quit();
    }

    void run() {
        SDL_Event e;
        uint32_t lastUpdate = SDL_GetTicks();
        
        while (m_running) {
            // Calculate delta time
            uint32_t currentTicks = SDL_GetTicks();
            uint32_t deltaTime = currentTicks - lastUpdate;
            
            // Process events
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) m_running = false;
                else {
                    // Handle events in visualization
                    m_visualization->handleEvents(e);
                    
                    // Handle key events
                    if (e.type == SDL_KEYDOWN) {
                        handleKeyDown(e.key.keysym.sym, (SDL_Keymod)e.key.keysym.mod);
                    }
                }
            }
            
            // Update at approximately 60 FPS
            if (deltaTime >= 16) {  // ~60 FPS
                update();
                lastUpdate = currentTicks;
            }
            
            // Render
            render();
            
            // Cap frame rate slightly
            SDL_Delay(1);
        }
    }

private:
    void handleKeyDown(SDL_Keycode key, SDL_Keymod mod) {
        switch (key) {
            case SDLK_ESCAPE: 
                m_running = false; 
                break;
            case SDLK_r: 
                // Reset parameters
                m_visualization->resetControls();
                break;
            case SDLK_p: 
                // Toggle play
                m_visualization->togglePlay();
                break;
            case SDLK_1: m_visualization->setVisualizationType(VisualizationType::NoteSequence); break;
            case SDLK_2: m_visualization->setVisualizationType(VisualizationType::GateSequence); break;
            case SDLK_3: m_visualization->setVisualizationType(VisualizationType::VelocitySequence); break;
            case SDLK_4: m_visualization->setVisualizationType(VisualizationType::Spectrum); break;
            case SDLK_5: m_visualization->setVisualizationType(VisualizationType::StepProbability); break;
            case SDLK_6: m_visualization->setVisualizationType(VisualizationType::GateOffset); break;
            case SDLK_7: m_visualization->setVisualizationType(VisualizationType::IsTrill); break;
            
            // Algorithm selection shortcuts
            case SDLK_F1: m_visualization->getParameters().type = AlgorithmType::Test; break;
            case SDLK_F2: m_visualization->getParameters().type = AlgorithmType::Tritrance; break;
            case SDLK_F3: m_visualization->getParameters().type = AlgorithmType::Stomper; break;
            case SDLK_F4: m_visualization->getParameters().type = AlgorithmType::Markov; break;
            case SDLK_F5: m_visualization->getParameters().type = AlgorithmType::Chiparp; break;
            case SDLK_F6: m_visualization->getParameters().type = AlgorithmType::Goaacid; break;
            case SDLK_F7: m_visualization->getParameters().type = AlgorithmType::Snh; break;
            case SDLK_F8: m_visualization->getParameters().type = AlgorithmType::Wobble; break;
            case SDLK_F9: m_visualization->getParameters().type = AlgorithmType::Techno; break;
            case SDLK_F10: m_visualization->getParameters().type = AlgorithmType::Funk; break;
            case SDLK_F11: m_visualization->getParameters().type = AlgorithmType::Drone; break;
            case SDLK_F12: m_visualization->getParameters().type = AlgorithmType::Phase; break;
            
            default: break;
        }
    }

    void update() {
        // Update visualization
        m_visualization->update();

        // Process algorithm with current parameters if playing or always for continuous updates
        auto params = m_visualization->getParameters();
        auto data = m_processor->process(params);

        // Update audio engine with current output
        if (!data.noteSequence.empty()) {
            m_audioEngine.setCurrentOutput(data.noteSequence[0]); // Use first step for now
        }
    }

    void render() {
        // Process algorithm with current parameters to get latest data
        auto params = m_visualization->getParameters();
        auto data = m_processor->process(params);

        // Render visualization
        m_visualization->render(m_renderer, data);

        // Present the renderer
        SDL_RenderPresent(m_renderer);
    }

    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
#ifdef HAS_SDL2_TTF
    TTF_Font* m_font;
#endif
    bool m_running;
    float m_dpi;
    
    // Application components
    std::unique_ptr<AlgoVisualization> m_visualization;
    std::unique_ptr<AlgorithmProcessor> m_processor;
    AudioEngine m_audioEngine;
    
    AlgorithmType m_selectedAlgorithm;
    VisualizationType m_selectedVisualization;
};

int main(int argc, char* argv[]) {
    AlgoTesterApp app;
    app.run();
    return 0;
}