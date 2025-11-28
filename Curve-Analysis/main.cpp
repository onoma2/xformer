#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#ifdef HAS_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

#include "CurveProcessor.h"
#include "Curve.h"
#include "dj_fft.h"
#include "AudioEngine.h"

struct Control {
    std::string name;
    union { // Use a union to store either float* or bool*
        float* value;
        bool* booleanValue;
    };
    float min;
    float max;
    float step;
    float defaultValue;      // Default value for float controls
    bool defaultBooleanValue; // Default value for boolean controls
    SDL_Rect rect;
    bool dragging;
    bool hovered;
    SDL_Rect textRect;
    bool isBoolean; // True if this control is a boolean toggle
};

struct ControlSection {
    std::string name;
    int startIndex;
    int count;
    bool collapsed;
    SDL_Rect headerRect; // Clickable area for header
};

// Audio Callback Wrapper
void audioCallback(void* userdata, Uint8* stream, int len) {
    AudioEngine* engine = static_cast<AudioEngine*>(userdata);
    engine->process(reinterpret_cast<float*>(stream), len / sizeof(float), 48000); // Assuming 48k for now
}

// Helper for Control initialization
Control makeFloatControl(const std::string& name, float* valuePtr, float minVal, float maxVal, float stepVal, int x, int y, int w, int h) {
    Control c;
    c.name = name;
    c.value = valuePtr;
    c.min = minVal;
    c.max = maxVal;
    c.step = stepVal;
    c.defaultValue = *valuePtr; // Store current value as default
    c.defaultBooleanValue = false;
    c.rect = {x, y, w, h};
    c.dragging = false;
    c.hovered = false;
    c.textRect = {0,0,0,0};
    c.isBoolean = false;
    return c;
}

Control makeBoolControl(const std::string& name, bool* boolPtr, int x, int y, int w, int h) {
    Control c;
    c.name = name;
    c.booleanValue = boolPtr;
    c.min = 0.0f; // Not used for bools, but needs init
    c.max = 1.0f; // Not used for bools
    c.step = 1.0f; // Not used for bools
    c.defaultValue = 0.0f;
    c.defaultBooleanValue = *boolPtr; // Store current value as default
    c.rect = {x, y, w, h};
    c.dragging = false;
    c.hovered = false;
    c.textRect = {0,0,0,0};
    c.isBoolean = true;
    return c;
}

class CurveAnalysisApp {
public:
    CurveAnalysisApp() :
        m_window(nullptr),
        m_renderer(nullptr),
#ifdef HAS_SDL2_TTF
        m_font(nullptr),
#endif
        m_running(false),
        m_selectedShape(Curve::Type::Sine),
        m_shapeVariation(false),
        m_invert(false),
        m_min(0.0f),
        m_max(1.0f),
        m_selectedSpectrumSource(SpectrumSource::FinalOutput),
        m_selectedControl(nullptr)
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) { std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl; return; }
#ifdef HAS_SDL2_TTF
        if (TTF_Init() == -1) { std::cerr << "TTF could not initialize! TTF_Error: " << TTF_GetError() << std::endl; return; }
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
        // For higher DPI displays, use a smaller physical window size
        float dpiScale = dpi / 96.0f;
        int windowWidth = static_cast<int>(1600 / std::max(1.0f, dpiScale * 0.5f));  // Don't make window too small
        int windowHeight = static_cast<int>(1200 / std::max(1.0f, dpiScale * 0.5f));

        m_window = SDL_CreateWindow("Curve Analysis Tool v2.0", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!m_window) { std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl; return; }
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
        if (!m_renderer) { std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl; return; }

#ifdef HAS_SDL2_TTF
        // Use standard font size
        m_font = TTF_OpenFont("/System/Library/Fonts/SFNS.ttf", 14);
        if (!m_font) m_font = TTF_OpenFont("/System/Library/Fonts/Arial.ttf", 14);
        if (!m_font) m_font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 14);
        if (!m_font) std::cerr << "Font could not be loaded! TTF_Error: " << TTF_GetError() << std::endl;
#endif
        initControls();
        m_processor.resetStates();
        
        // Initialize Audio
        SDL_AudioSpec want, have;
        SDL_zero(want);
        want.freq = 48000;
        want.format = AUDIO_F32;
        want.channels = 1;
        want.samples = 1024;
        want.callback = audioCallback;
        want.userdata = &m_audioEngine;

        m_audioDeviceId = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (m_audioDeviceId == 0) {
            std::cerr << "Failed to open audio: " << SDL_GetError() << std::endl;
        } else {
            SDL_PauseAudioDevice(m_audioDeviceId, 0); // Start playing
        }

        m_running = true;
        SDL_StartTextInput();
    }

    ~CurveAnalysisApp() {
        SDL_StopTextInput();
        if (m_audioDeviceId != 0) {
            SDL_CloseAudioDevice(m_audioDeviceId);
        }
#ifdef HAS_SDL2_TTF
        if (m_font) TTF_CloseFont(m_font);
#endif
        if (m_renderer) SDL_DestroyRenderer(m_renderer);
        if (m_window) SDL_DestroyWindow(m_window);
#ifdef HAS_SDL2_TTF
        TTF_Quit();
#endif
        SDL_Quit();
    }

    void run() {
        SDL_Event e;
        while (m_running) {
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) m_running = false;
                else if (e.type == SDL_KEYDOWN) handleKeyDown(e.key.keysym.sym, (SDL_Keymod)e.key.keysym.mod);
                else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) handleMouse(e);
                else if (e.type == SDL_MOUSEMOTION) handleMouseMotion(e);
                else if (e.type == SDL_TEXTINPUT) handleTextInput(e.text.text);
                else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    // Window resized, no special handling needed
                }
            }
            update();
            render();
            SDL_Delay(16);
        }
    }

private:
    void initControls() {
        int y = 30;
        int controlHeight = 20;
        int controlWidth = 150;
        int spacing = 5;
        
        // --- Signal Chain Controls (including Enable Toggles and Feedback Toggles) ---
        // Core Function Toggles
        m_controls.push_back(makeBoolControl("Enable Phase Skew", &m_params.enablePhaseSkew, 20, y, 20, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeBoolControl("Enable Wavefolder", &m_params.enableWavefolder, 20, y, 20, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeBoolControl("Enable DJ Filter", &m_params.enableDjFilter, 20, y, 20, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeBoolControl("Enable Post-Filt Comp", &m_params.enablePostFilterCompensation, 20, y, 20, controlHeight)); y += controlHeight + spacing;
        y += 10; // Extra spacing

        // Main Signal Path Controls
        m_controls.push_back(makeFloatControl("Global Phase", &m_params.globalPhase, -1.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Phase Skew", &m_params.phaseSkew, -1.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Wavefolder Fold", &m_params.wavefolderFold, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Wavefolder Gain", &m_params.wavefolderGain, 0.0f, 2.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Wavefolder Symmetry", &m_params.wavefolderSymmetry, -1.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("DJ Filter Amt", &m_params.djFilter, -1.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Filter F (Res)", &m_params.filterF, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Filter Slope (6/12/24)", &m_params.filterSlopeFloatProxy, 0.0f, 2.0f, 1.0f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Fold F (Feedback)", &m_params.foldF, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Crossfade", &m_params.xFade, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        
        // Feedback Routing Controls with Toggles
        y += 10; // Extra spacing
        m_controls.push_back(makeBoolControl("Enable Shape -> Fold", &m_params.enableShapeToWavefolderFold, 20, y, 20, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Shape -> Wavefolder Fold", &m_params.shapeToWavefolderFold, -1.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        
        y += 5; // Small spacing
        m_controls.push_back(makeBoolControl("Enable Fold -> Filter", &m_params.enableFoldToFilterFreq, 20, y, 20, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Fold -> Filter Freq", &m_params.foldToFilterFreq, -1.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        
        y += 5; // Small spacing
        m_controls.push_back(makeBoolControl("Enable Filter -> Fold", &m_params.enableFilterToWavefolderFold, 20, y, 20, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Filter -> Wavefolder Fold", &m_params.filterToWavefolderFold, -1.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        
        y += 5; // Small spacing
        m_controls.push_back(makeBoolControl("Enable Shape -> Skew", &m_params.enableShapeToPhaseSkew, 20, y, 20, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Shape -> Phase Skew", &m_params.shapeToPhaseSkew, -1.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;

        y += 5; // Small spacing
        m_controls.push_back(makeBoolControl("Enable Filter -> Skew", &m_params.enableFilterToPhaseSkew, 20, y, 20, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Filter -> Phase Skew", &m_params.filterToPhaseSkew, -1.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;

        m_controls.push_back(makeFloatControl("Min", &m_min, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Max", &m_max, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;

        // --- Advanced Shaping Controls ---
        y += 20;
        m_controls.push_back(makeFloatControl("Fold Amount", &m_params.advanced.foldAmount, 1.0f, 32.0f, 0.1f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("HPF Curve", &m_params.advanced.hpfCurve, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Resonance Gain", &m_params.advanced.resonanceGain, 0.5f, 4.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Resonance Tame", &m_params.advanced.resonanceTame, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Feedback Curve", &m_params.advanced.feedbackCurve, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Fold Comp", &m_params.advanced.foldComp, 0.0f, 2.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("LPF Comp", &m_params.advanced.lpfComp, 0.0f, 2.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("HPF Comp", &m_params.advanced.hpfComp, 0.0f, 2.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Res Comp", &m_params.advanced.resComp, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Max Comp", &m_params.advanced.maxComp, 1.0f, 5.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("LFO Limiter Amt", &m_params.advanced.lfoLimiterAmount, 0.0f, 5.0f, 0.1f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("LFO Limiter Min", &m_params.advanced.lfoLimiterMin, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Feedback Limit", &m_params.advanced.feedbackLimit, 0.0f, 8.0f, 0.1f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;

        // --- Hardware Simulation Controls ---
        y += 20;
        m_controls.push_back(makeFloatControl("LFO Freq (Hz)", &m_params.frequency, 0.01f, 10.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("DAC Resolution (bits)", &m_params.dacResolutionFloatProxy, 12.0f, 16.0f, 1.0f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("DAC Update Rate", &m_params.dacUpdateRate, 0.1f, 5.0f, 0.001f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Timing Jitter (ms)", &m_params.timingJitter, 0.0f, 0.5f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;

        // --- Fine Tuning Controls ---
        y += 20;
        m_controls.push_back(makeFloatControl("Fine Fold-F (0.0-0.1)", &m_params.foldF, 0.0f, 0.1f, 0.0001f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
        m_controls.push_back(makeFloatControl("Super Fine Fold-F (0-0.01)", &m_params.foldF, 0.0f, 0.01f, 0.00001f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;

        // --- Audio Engine Controls ---
        y += 20;
        m_controls.push_back(makeFloatControl("Audio Vol (0-1)", &m_audioEngine.m_volume, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); 
        m_controls.back().defaultValue = 0.0f; // Ensure default is silence
        y += controlHeight + spacing;
        
        m_controls.push_back(makeFloatControl("Audio Mod Amt", &m_audioEngine.m_modAmount, 0.0f, 2.0f, 0.01f, 20, y, controlWidth, controlHeight)); 
        m_controls.back().defaultValue = 0.0f; // Ensure default is no mod
        y += controlHeight + spacing;
        
        // Initialize Sections (Corrected counts and start indices)
        // Recount:
        // Signal Chain: 4 toggles + 10 floats + 5*(1 bool + 1 float) + 2 (min/max) = 4 + 10 + 10 + 2 = 26 controls
        m_sections.push_back({"Signal Chain", 0, 26, false, {0,0,0,0}}); // Open by default
        
        // Advanced Shaping: 13 controls
        m_sections.push_back({"Advanced Shaping", 26, 13, true, {0,0,0,0}}); // Collapsed by default
        
        // Hardware Simulation: 4 controls
        m_sections.push_back({"Hardware Simulation", 39, 4, true, {0,0,0,0}}); // Collapsed by default
        
        // Fine Tuning: 2 controls
        m_sections.push_back({"Fine Tuning", 43, 2, true, {0,0,0,0}}); // Collapsed by default
        
        // Audio Engine: 2 controls
        m_sections.push_back({"Audio Engine", 45, 2, true, {0,0,0,0}}); // Collapsed by default
    }

    void resetControls() {
        m_params = CurveProcessor::Parameters(); // Reset to default constructor values
        // This will reset enables to their defaults (Core enabled, others disabled)
        
        m_min = m_params.min;
        m_max = m_params.max;
        m_selectedShape = m_params.shape;
        m_shapeVariation = m_params.shapeVariation;
        m_invert = m_params.invert;
        m_selectedSpectrumSource = m_params.spectrumSource;
        m_processor.resetStates();

        // Also reset audio engine parameters
        m_audioEngine.m_volume = 0.0f;
        m_audioEngine.m_modAmount = 0.0f;
        m_audioEngine.m_active = false;
        
        updateControlsLayout(); // Re-layout
    }

    void handleKeyDown(SDL_Keycode key, SDL_Keymod mod) {
        if (m_selectedControl) {
            if (key == SDLK_RETURN) {
                try {
                    *m_selectedControl->value = std::stof(m_textInputBuffer);
                } catch (const std::exception&) {
                    // Ignore invalid input
                }
                m_selectedControl = nullptr;
                m_textInputBuffer.clear();
            } else if (key == SDLK_BACKSPACE && !m_textInputBuffer.empty()) {
                m_textInputBuffer.pop_back();
            }
            return;
        }

        float step = (mod & KMOD_SHIFT) ? 0.1f : 0.01f;
        switch (key) {
            case SDLK_ESCAPE: m_running = false; break;
            case SDLK_r: resetControls(); break;
            case SDLK_s: m_shapeVariation = !m_shapeVariation; break;
            case SDLK_i: m_invert = !m_invert; break;
            case SDLK_1: m_selectedShape = Curve::Type::Sine; break;
            case SDLK_2: m_selectedShape = Curve::Type::Triangle; break;
            case SDLK_3: m_selectedShape = Curve::Type::RampUp; break;
            case SDLK_4: m_selectedShape = Curve::Type::RampDown; break;
            case SDLK_5: m_selectedShape = Curve::Type::Square; break;
            case SDLK_6: m_selectedShape = Curve::Type::Linear; break;
            case SDLK_7: m_selectedShape = Curve::Type::Bell; break;
            case SDLK_8: m_selectedShape = Curve::Type::Sigmoid; break;
            case SDLK_z: updateSampleRate(500); break;
            case SDLK_x: updateSampleRate(8000); break;
            case SDLK_c: updateSampleRate(16000); break;
            case SDLK_v: updateSampleRate(32000); break;
            case SDLK_b: updateSampleRate(44100); break;
            case SDLK_n: updateSampleRate(48000); break;
            case SDLK_m: updateSampleRate(96000); break;
            case SDLK_l: m_params.range = (VoltageRange)(((int)m_params.range + 1) % (int)VoltageRange::Last); break;
            case SDLK_a: m_selectedSpectrumSource = SpectrumSource::Input; break;
            case SDLK_d: m_selectedSpectrumSource = SpectrumSource::PostWavefolder; break;
            case SDLK_f: m_selectedSpectrumSource = SpectrumSource::PostFilter; break;
            case SDLK_g: m_selectedSpectrumSource = SpectrumSource::PostCompensation; break;
            case SDLK_h: m_selectedSpectrumSource = SpectrumSource::FinalOutput; break;

            // Advanced controls
            case SDLK_q: m_params.advanced.foldAmount += step * ((mod & KMOD_SHIFT) ? 10.f : 1.f); break;
            case SDLK_w: m_params.advanced.foldAmount -= step * ((mod & KMOD_SHIFT) ? 10.f : 1.f); break;
            case SDLK_e: m_params.advanced.hpfCurve += step; break;
            case SDLK_t: m_params.advanced.hpfCurve -= step; break;
            
            // Audio
            case SDLK_p: m_audioEngine.m_active = !m_audioEngine.m_active; break;

            default: break;
        }
    }

    void handleTextInput(const char* text) {
        if (m_selectedControl) {
            m_textInputBuffer += text;
        }
    }

    void handleMouse(const SDL_Event& e) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            SDL_Point p = {e.button.x, e.button.y};
            
            // Global MouseUp: Clear all dragging states
            if (e.type == SDL_MOUSEBUTTONUP) {
                for (auto& c : m_controls) {
                    c.dragging = false;
                }
                return;
            }
            
            // Check section headers
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                for (auto& section : m_sections) {
                    if (SDL_PointInRect(&p, &section.headerRect)) {
                        section.collapsed = !section.collapsed;
                        updateControlsLayout(); // Re-layout immediately
                        return;
                    }
                }
            }

            for (auto& c : m_controls) {
                // If control is effectively invisible (height 0 or off screen), don't interact
                if (c.rect.h == 0) continue;
                
                bool shiftPressed = (SDL_GetModState() & KMOD_SHIFT);

                if (c.isBoolean) {
                    if (SDL_PointInRect(&p, &c.rect) && e.type == SDL_MOUSEBUTTONDOWN) {
                        if (shiftPressed) {
                            *c.booleanValue = c.defaultBooleanValue; // Reset to default
                        } else {
                            *c.booleanValue = !(*c.booleanValue); // Toggle
                        }
                        updateControlsLayout(); // Re-layout needed if collapsed sections toggle
                        return; // Handled click, prevent further processing
                    }
                } else {
                    if (SDL_PointInRect(&p, &c.rect)) {
                        if (e.type == SDL_MOUSEBUTTONDOWN) {
                            if (shiftPressed) {
                                *c.value = c.defaultValue; // Reset to default
                                // Don't start dragging if resetting
                            } else {
                                c.dragging = true;
                            }
                        }
                    }
                    if (SDL_PointInRect(&p, &c.textRect) && e.type == SDL_MOUSEBUTTONDOWN) {
                        m_selectedControl = &c;
                        m_textInputBuffer.clear();
                    }
                }
            }
        }
    }

    void handleMouseMotion(const SDL_Event& e) {
        SDL_Point p = {e.motion.x, e.motion.y};
        for (auto& c : m_controls) {
            c.hovered = SDL_PointInRect(&p, &c.rect);
            if (c.isBoolean) {
                // Boolean controls don't drag
                continue;
            }

            if (c.dragging) {
                if (SDL_PointInRect(&p, &c.rect)) { // Only allow updates if mouse is inside the rect
                    int mouseX = e.motion.x;
                    int validX = std::clamp(mouseX, c.rect.x, c.rect.x + c.rect.w);
                    float pos = (float)(validX - c.rect.x) / c.rect.w;
                    
                    if (c.name == "LFO Freq (Hz)") {
                        float mappedValue;
                        if (pos <= 0.8f) {
                            float ratio = pos / 0.8f;
                            mappedValue = 0.01f + ratio * (1.0f - 0.01f);
                        } else {
                            float ratio = (pos - 0.8f) / 0.2f;
                            mappedValue = 1.0f + ratio * (10.0f - 1.0f);
                        }
                        *c.value = mappedValue;
                    } else {
                        *c.value = c.min + pos * (c.max - c.min);
                    }
                } else {
                    // Mouse left the rectangle: Cancel dragging immediately
                    c.dragging = false;
                }
            }
        }
    }

    void updateControlsLayout() {
        int winWidth, winHeight;
        SDL_GetWindowSize(m_window, &winWidth, &winHeight);

        const int controlPanelWidth = std::max(300, static_cast<int>(winWidth / 4));
        const int margin = static_cast<int>(20 * std::min(float(winWidth)/1600.0f, float(winHeight)/1200.0f));
        const int controlX = margin;
        const int controlWidth = static_cast<int>(controlPanelWidth * 0.8f);
        const int controlHeight = std::max(15, static_cast<int>(20 * std::min(float(winWidth)/1600.0f, float(winHeight)/1200.0f)));

        int y = 30;
        int spacing = 5;
        int groupSpacing = 25; // Height of the header area

        for (auto& section : m_sections) {
            // Header Position
            section.headerRect = {controlX, y, controlWidth, 20};
            y += groupSpacing;

            // Controls
            for (int i = 0; i < section.count; ++i) {
                int idx = section.startIndex + i;
                if (idx >= (int)m_controls.size()) break;

                if (section.collapsed) {
                    // Hide control
                    m_controls[idx].rect = {0, 0, 0, 0}; 
                } else {
                    // Show control
                    m_controls[idx].rect = {controlX, y, controlWidth, controlHeight};
                    y += controlHeight + spacing;
                }
            }
            y += 5; // Small gap between sections
        }
    }

    void update() {
        updateControlsLayout();  // Update control positions based on window size

        // Map Slope Proxy to Enum
        m_params.filterSlope = static_cast<FilterSlope>(std::round(m_params.filterSlopeFloatProxy));
        // Snap proxy to valid integer steps (0, 1, 2)
        m_params.filterSlopeFloatProxy = static_cast<float>(static_cast<int>(m_params.filterSlope));

        // Sync the integer dacResolutionBits with its float proxy for UI control
        m_params.dacResolutionBits = static_cast<int>(m_params.dacResolutionFloatProxy);
        // Ensure the float proxy matches the integer value (in case it was truncated)
        m_params.dacResolutionFloatProxy = static_cast<float>(m_params.dacResolutionBits);

        m_params.min = m_min;
        m_params.max = m_max;
        m_params.shape = m_selectedShape;
        m_params.shapeVariation = m_shapeVariation;
        m_params.invert = m_invert;
        m_params.spectrumSource = m_selectedSpectrumSource;
        m_signalData = m_processor.process(m_params, m_sampleRate);
        
        // Update Audio Engine with latest LFO buffer and frequency
        m_audioEngine.setLfoBuffer(m_signalData.hardwareLimitedOutput, m_params.frequency);
    }

    void render() {
        SDL_SetRenderDrawColor(m_renderer, 20, 20, 20, 255);
        SDL_RenderClear(m_renderer);
        drawWaveforms();
        drawSpectrum();
        drawControls();
        drawInfo();
        SDL_RenderPresent(m_renderer);
    }

    void drawWaveforms() {
        // Get current window dimensions to adapt layout
        int winWidth, winHeight;
        SDL_GetWindowSize(m_window, &winWidth, &winHeight);

        // Calculate layout based on available space
        const int margin = static_cast<int>(20 * std::min(float(winWidth)/1600.0f, float(winHeight)/1200.0f));
        const int controlPanelWidth = std::max(300, static_cast<int>(winWidth / 4));
        const int topMargin = margin + 70; // Reduced margin to move graphs up

        // Calculate available space for graphs, reserving space at the bottom for spectrum
        const int spectrumHeight = 200; // Reduced height for spectrum to prevent overlap
        // Reserve space for spectrum, top UI, and a bottom footer
        // Adding extra padding to separate graphs from spectrum
        const int availableWidth = winWidth - controlPanelWidth - 3 * margin;
        const int availableHeight = winHeight - spectrumHeight - topMargin - margin - 40; 

        // Reduce graph size to 95% of available area as requested
        const int totalGraphWidth = static_cast<int>(availableWidth * 0.95f);
        const int totalGraphHeight = static_cast<int>(availableHeight * 0.95f);
        
        const int graphWidth = totalGraphWidth / 4; // 4 columns
        const int graphHeight = totalGraphHeight / 2; // 2 rows
        
        // Center the grid in the available area
        const int offsetX = (availableWidth - totalGraphWidth) / 2;
        const int offsetY = (availableHeight - totalGraphHeight) / 2;

        std::vector<std::pair<std::string, const std::vector<float>*>> graphs = {
            // Row 1
            {"Input Signal", &m_signalData.originalSignal},
            {"Skewed Phase", &m_signalData.skewedPhase}, 
            {"Mirrored Phase", &m_signalData.mirroredPhase}, // NEW
            {"Post Wavefolder", &m_signalData.postWavefolder},
            // Row 2
            {"Post Filter", &m_signalData.postFilter},
            {"Final Output", &m_signalData.finalOutput},
            {"Hardware Limited Output", &m_signalData.hardwareLimitedOutput},  // With all hardware constraints applied
            // Empty slot or another debug view? Let's put Post Compensation here for completeness if needed, or leave blank
            {"Post Compensation", &m_signalData.postCompensation}
        };

        for (size_t i = 0; i < graphs.size(); ++i) {
            int row = i / 4; // 4 columns
            int col = i % 4;
            int x = controlPanelWidth + margin + offsetX + col * (graphWidth + 10); // small spacing
            int y = topMargin + margin + offsetY + row * (graphHeight + 40); // extra vertical spacing for labels
            drawGraph(graphs[i].first, *graphs[i].second, i, x, y, graphWidth, graphHeight);

            // For the hardware limited output plot (index 6 now), detect and visualize stair-stepping artifacts
            if (i == 6) {  // Hardware Limited Output plot index
                // Calculate differences between Final Output and Hardware Limited Output
                const auto& finalOutput = m_signalData.finalOutput;
                const auto& hardwareLimited = m_signalData.hardwareLimitedOutput;

                if (finalOutput.size() == hardwareLimited.size() && !finalOutput.empty()) {
                    float maxDiff = 0.0f;
                    for (size_t j = 0; j < finalOutput.size(); ++j) {
                        float diff = std::abs(finalOutput[j] - hardwareLimited[j]);
                        maxDiff = std::max(maxDiff, diff);
                    }

                    // If difference is significant, indicate stair-stepping artifacts
                    if (maxDiff > 0.05f) {  // Threshold for significant difference (50mV)
                        // Draw a red border around the graph to indicate stair-stepping
                        SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);  // Red
                        SDL_Rect border = {x, y, graphWidth, graphHeight};
                        SDL_RenderDrawRect(m_renderer, &border);

#ifdef HAS_SDL2_TTF
                        if (m_font) {
                            renderText("STAIR-STEPPING!", x + 10, y + 10, {255, 50, 50, 255});
                        }
#endif
                    } else if (maxDiff > 0.02f) {  // Moderate difference (20mV)
                        // Draw an orange border
                        SDL_SetRenderDrawColor(m_renderer, 255, 165, 0, 255);  // Orange
                        SDL_Rect border = {x, y, graphWidth, graphHeight};
                        SDL_RenderDrawRect(m_renderer, &border);
                    }
                }
            }
        }
    }

    void drawGraph(const std::string& title, const std::vector<float>& data, int index, int x, int y, int w, int h) {
        SDL_Rect bg = {x, y, w, h};
        SDL_SetRenderDrawColor(m_renderer, 30, 30, 30, 255);
        SDL_RenderFillRect(m_renderer, &bg);
        drawGridLines(x, y, w, h);
        drawCenterLine(x, y, w, h);

        bool isNormalized = (index <= 3); // Row 1 is normalized (0-1), Row 2 is Bipolar
        SDL_Color color;
        if (index == 6) color = SDL_Color{255, 255, 0, 255}; // Hardware Limit (Yellow)
        else if (index == 0) color = SDL_Color{100, 100, 255, 255}; // Input (Blue)
        else color = SDL_Color{0, 255, 0, 255}; // Others (Green)
        
        drawWave(data, x, y, w, h, isNormalized, color);
        
        SDL_SetRenderDrawColor(m_renderer, 100, 100, 100, 255);
        SDL_RenderDrawRect(m_renderer, &bg);
#ifdef HAS_SDL2_TTF
        if (m_font) {
            renderText(title, x, y - 25, {255, 255, 255, 255});
            const auto& range = voltageRangeInfo(m_params.range);
            std::string yLabel = isNormalized ? "0-1" : (std::to_string((int)range.lo) + "V to " + std::to_string((int)range.hi) + "V");
            renderText(yLabel, x - 55, y + h/2 - 8, {255, 255, 255, 255});
        }
#endif
    }

    void drawWave(const std::vector<float>& data, int x, int y, int w, int h, bool isNormalized, SDL_Color c) {
        if (data.empty()) return;
        SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, c.a);
        const auto& range = voltageRangeInfo(m_params.range);

        for (size_t i = 1; i < data.size(); ++i) {
            float v1 = data[i-1], v2 = data[i];
            float n1 = isNormalized ? 1.f - v1 : 1.f - (v1 - range.lo) / (range.hi - range.lo);
            float n2 = isNormalized ? 1.f - v2 : 1.f - (v2 - range.lo) / (range.hi - range.lo);

            // Clamp normalized values to prevent drawing outside the graph area
            n1 = std::clamp(n1, 0.0f, 1.0f);
            n2 = std::clamp(n2, 0.0f, 1.0f);

            SDL_RenderDrawLine(m_renderer, x + (i-1)*w/(data.size()-1), y + n1*h, x + i*w/(data.size()-1), y + n2*h);
        }
    }

    void drawSpectrum() {
        int winWidth, winHeight;
        SDL_GetWindowSize(m_window, &winWidth, &winHeight);

        const int margin = static_cast<int>(20 * std::min(float(winWidth)/1600.0f, float(winHeight)/1200.0f));
        const int controlPanelWidth = std::max(300, static_cast<int>(winWidth / 4));
        int x = controlPanelWidth + margin;
        int h = 200; // Fixed height for spectrum
        int y = winHeight - h - 50; // Position at bottom with padding
        int w = winWidth - controlPanelWidth - 2 * margin;

        SDL_Rect bg = {x, y, w, h};
        SDL_SetRenderDrawColor(m_renderer, 30, 30, 30, 255);
        SDL_RenderFillRect(m_renderer, &bg);

        const auto& spectrum = m_signalData.spectrum;
        const auto& spectrum_oversampled = m_signalData.spectrum_oversampled;
        if (spectrum.empty() || spectrum_oversampled.empty()) return;

        // Draw oversampled spectrum (aliasing potential) in red
        SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
        float max_mag = -100.f;
        for(float mag : spectrum_oversampled) max_mag = std::max(max_mag, mag);
        float min_freq = 20.f;
        float max_freq_oversampled = m_sampleRate; // 2 * (m_sampleRate / 2)
        for(size_t i = 0; i < spectrum_oversampled.size(); ++i) {
            float freq = float(i) * max_freq_oversampled / spectrum_oversampled.size();
            if (freq < min_freq) continue;
            if (freq > m_sampleRate / 2.f) {
                float aliased_freq = m_sampleRate - freq;
                if (aliased_freq < min_freq) continue;
                float mag = spectrum_oversampled[i];
                float normalized_mag = (mag + 60.f) / 60.f;
                int bar_height = std::clamp(int(normalized_mag * h), 0, h);
                int bar_x = x + w * log10(aliased_freq / min_freq) / log10((m_sampleRate/2.f) / min_freq);
                SDL_Rect bar = {bar_x, y + h - bar_height, 2, bar_height};
                SDL_RenderFillRect(m_renderer, &bar);
            }
        }

        // Draw normal spectrum in orange
        SDL_SetRenderDrawColor(m_renderer, 255, 128, 0, 255);
        max_mag = -100.f;
        for(float mag : spectrum) max_mag = std::max(max_mag, mag);
        for(size_t i = 0; i < spectrum.size(); ++i) {
            float freq = float(i) * m_sampleRate / (2 * spectrum.size());
            if (freq < min_freq) continue;
            float mag = spectrum[i];
            float normalized_mag = (mag + 60.f) / 60.f;
            int bar_height = std::clamp(int(normalized_mag * h), 0, h);
            int bar_x = x + w * log10(freq / min_freq) / log10((m_sampleRate/2.f) / min_freq);
            SDL_Rect bar = {bar_x, y + h - bar_height, 2, bar_height};
            SDL_RenderFillRect(m_renderer, &bar);
        }

        drawGridLines(x, y, w, h);

        // Draw Nyquist line
        int nyquist_x = x + w;
        SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
        SDL_RenderDrawLine(m_renderer, nyquist_x, y, nyquist_x, y + h);

        SDL_SetRenderDrawColor(m_renderer, 100, 100, 100, 255);
        SDL_RenderDrawRect(m_renderer, &bg);

#ifdef HAS_SDL2_TTF
        if (m_font) {
            renderText("Spectrum Analysis", x, y - 25, {255, 255, 255, 255});
            renderText("Nyquist", nyquist_x - 50, y + h + 5, {255, 0, 0, 255});
            for (int i = 0; i <= 6; ++i) {
                 renderText(std::to_string(-i * 10) + "dB", x - 40, y + i * h / 6 - 8, {255, 255, 255, 255});
            }
            for (float freq = 100.f; freq < m_sampleRate/2.f; freq *= 2) {
                int line_x = x + w * log10(freq / min_freq) / log10((m_sampleRate/2.f) / min_freq);
                renderText(std::to_string(int(freq)), line_x - 15, y + h + 5, {255,255,255,255});
            }
        }
#endif
    }

    void drawGridLines(int x, int y, int w, int h) {
        SDL_SetRenderDrawColor(m_renderer, 50, 50, 50, 255);
        for (int i = 1; i < 4; ++i) SDL_RenderDrawLine(m_renderer, x, y + i*h/4, x + w, y + i*h/4);
        for (int i = 1; i < 4; ++i) SDL_RenderDrawLine(m_renderer, x + i*w/4, y, x + i*w/4, y + h);
    }

    void drawCenterLine(int x, int y, int w, int h) {
        SDL_SetRenderDrawColor(m_renderer, 60, 60, 60, 255);
        SDL_RenderDrawLine(m_renderer, x, y + h/2, x + w, y + h/2);
    }

    void drawControls() {
        // Draw Control Panel Background
        int winWidth, winHeight;
        SDL_GetWindowSize(m_window, &winWidth, &winHeight);
        const int controlPanelWidth = std::max(300, static_cast<int>(winWidth / 4));
        SDL_Rect panelRect = {0, 0, controlPanelWidth, winHeight};
        SDL_SetRenderDrawColor(m_renderer, 25, 25, 25, 255); // Slightly lighter than main bg
        SDL_RenderFillRect(m_renderer, &panelRect);
        SDL_SetRenderDrawColor(m_renderer, 40, 40, 40, 255);
        SDL_RenderDrawLine(m_renderer, controlPanelWidth, 0, controlPanelWidth, winHeight);

        // Draw Section Headers
        for (const auto& section : m_sections) {
            // Draw collapse icon
            std::string icon = section.collapsed ? "[+]" : "[-]";
            
            // Hover effect for header
            int mouseX, mouseY;
            SDL_GetMouseState(&mouseX, &mouseY);
            SDL_Point p = {mouseX, mouseY};
            
            SDL_Color headerColor = {200, 200, 200, 255};
            if (SDL_PointInRect(&p, &section.headerRect)) {
                headerColor = {255, 255, 255, 255}; // Brighter on hover
            }

            if (m_font) {
                renderText(icon + " " + section.name, section.headerRect.x, section.headerRect.y, headerColor);
            }
        }

        for (auto& c : m_controls) {
            // Skip invisible controls
            if (c.rect.h == 0) continue;

            // Determine if the control (or its parent function) is disabled
            bool functionEnabled = true;
            if (c.name == "Phase Skew" && !m_params.enablePhaseSkew) functionEnabled = false;
            else if ((c.name.find("Wavefolder") != std::string::npos || c.name.find("Fold F") != std::string::npos) && !m_params.enableWavefolder) functionEnabled = false;
            else if ((c.name.find("DJ Filter") != std::string::npos || c.name.find("Filter F") != std::string::npos || c.name.find("Filter Slope") != std::string::npos) && !m_params.enableDjFilter) functionEnabled = false;
            // Feedback specific toggles (if the toggle itself is off, it's disabled)
            else if (c.name == "Shape -> Wavefolder Fold" && !m_params.enableShapeToWavefolderFold) functionEnabled = false;
            else if (c.name == "Fold -> Filter Freq" && !m_params.enableFoldToFilterFreq) functionEnabled = false;
            else if (c.name == "Filter -> Wavefolder Fold" && !m_params.enableFilterToWavefolderFold) functionEnabled = false;
            else if (c.name == "Shape -> Phase Skew" && !m_params.enableShapeToPhaseSkew) functionEnabled = false;
            else if (c.name == "Filter -> Phase Skew" && !m_params.enableFilterToPhaseSkew) functionEnabled = false;
            // Min/Max are global, not tied to a specific function enable.

            if (c.isBoolean) {
                // Render as checkbox
                SDL_Rect checkboxRect = {c.rect.x, c.rect.y, c.rect.h, c.rect.h}; // Square checkbox
                SDL_SetRenderDrawColor(m_renderer, 50, 50, 50, 255); // Background
                SDL_RenderFillRect(m_renderer, &checkboxRect);
                
                // Checkmark
                if (*c.booleanValue) {
                    SDL_SetRenderDrawColor(m_renderer, 0, 200, 0, 255); // Green for ON
                    SDL_RenderFillRect(m_renderer, &checkboxRect);
                }
                
                // Border
                SDL_SetRenderDrawColor(m_renderer, 150, 150, 150, 255);
                SDL_RenderDrawRect(m_renderer, &checkboxRect);

#ifdef HAS_SDL2_TTF
                if (m_font) {
                    SDL_Color textColor = c.hovered ? SDL_Color{255,255,255,255} : SDL_Color{220,220,220,255};
                    renderText(c.name, c.rect.x + c.rect.h + 5, c.rect.y + 2, textColor);
                }
#endif
            } else {
                // Render as slider
                SDL_SetRenderDrawColor(m_renderer, 50, 50, 50, 255); // Slider track background
                SDL_RenderFillRect(m_renderer, &c.rect);
                
                float pos;
                if (c.name == "LFO Freq (Hz)") {
                    float val = *c.value;
                    if (val <= 1.0f) {
                        float ratio = (val - 0.01f) / (1.0f - 0.01f);
                        pos = ratio * 0.8f;
                    } else {
                        float ratio = (val - 1.0f) / (10.0f - 1.0f);
                        pos = 0.8f + ratio * 0.2f;
                    }
                } else {
                    pos = (*c.value - c.min) / (c.max - c.min);
                }
                pos = std::clamp(pos, 0.0f, 1.0f);
                SDL_Rect fill = {c.rect.x, c.rect.y, (int)(c.rect.w * pos), c.rect.h};
                
                // Determine color based on hovered/dragging state and if function is enabled
                SDL_Color activeColor = {0, 100, 200, 255}; // Standard blue
                SDL_Color inactiveColor = {50, 50, 50, 255}; // Grayed out

                if (!functionEnabled) {
                    SDL_SetRenderDrawColor(m_renderer, inactiveColor.r, inactiveColor.g, inactiveColor.b, inactiveColor.a);
                } else if (c.hovered || c.dragging) {
                    SDL_SetRenderDrawColor(m_renderer, 0, 140, 240, 255); // Brighter blue
                } else {
                    SDL_SetRenderDrawColor(m_renderer, activeColor.r, activeColor.g, activeColor.b, activeColor.a);
                }
                SDL_RenderFillRect(m_renderer, &fill);

                // Center line for bipolar controls
                if (c.min < 0) {
                    int centerX = c.rect.x + (int)(((0.0f - c.min) / (c.max - c.min)) * c.rect.w);
                    SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 100);
                    SDL_RenderDrawLine(m_renderer, centerX, c.rect.y, centerX, c.rect.y + c.rect.h);
                }

                // Border
                if (!functionEnabled) {
                    SDL_SetRenderDrawColor(m_renderer, 80, 80, 80, 255); // Gray border for inactive
                } else if (c.hovered || &c == m_selectedControl) {
                     SDL_SetRenderDrawColor(m_renderer, 150, 150, 150, 255);
                } else {
                     SDL_SetRenderDrawColor(m_renderer, 80, 80, 80, 255);
                }
                SDL_RenderDrawRect(m_renderer, &c.rect);

#ifdef HAS_SDL2_TTF
                if (m_font) {
                    SDL_Color textColor = functionEnabled ? (c.hovered ? SDL_Color{255,255,255,255} : SDL_Color{220,220,220,255}) : SDL_Color{100,100,100,255};
                    renderText(c.name, 20, c.rect.y + 2, textColor);
                    
                    std::stringstream ss;
                    if (&c == m_selectedControl) {
                        ss << m_textInputBuffer;
                        if (int(SDL_GetTicks() / 500) % 2 == 0) {
                            ss << "|";
                        }
                    } else if (c.name == "Filter Slope (6/12/24)") {
                        int val = static_cast<int>(*c.value);
                        if (val == 0) ss << "6dB/oct";
                        else if (val == 1) ss << "12dB/oct";
                        else if (val == 2) ss << "24dB/oct";
                        else ss << "Unknown";
                    } else if (c.name == "LFO Freq (Hz)") {
                        ss << std::fixed << std::setprecision(2) << *c.value;
                    } else {
                        ss << std::fixed << std::setprecision(2) << *c.value;
                    }
                    
                    // Calculate text position to be right of the slider
                    int textX = c.rect.x + c.rect.w + 10;
                    
                    // Pre-render to get size for textRect
                    SDL_Surface* valueSurface = TTF_RenderText_Solid(m_font, ss.str().c_str(), {255,255,255,255});
                    if (valueSurface) {
                        c.textRect = {textX, c.rect.y + 2, valueSurface->w, valueSurface->h};
                        SDL_Color valColor = functionEnabled ? ((&c == m_selectedControl) ? SDL_Color{255,255,0,255} : SDL_Color{200,200,200,255}) : SDL_Color{100,100,100,255};
                        if (c.hovered && functionEnabled) valColor.r = std::min(255, valColor.r + 55);
                        
                        renderText(ss.str(), c.textRect.x, c.textRect.y, valColor);
                        SDL_FreeSurface(valueSurface);
                    }
                }
#endif
            }
        }
    }

    void drawInfo() {
#ifdef HAS_SDL2_TTF
        if (m_font) {
            // Get window dimensions to calculate positioning
            int winWidth, winHeight;
            SDL_GetWindowSize(m_window, &winWidth, &winHeight);

            // Calculate position just below the control panel
            const int controlPanelWidth = std::max(300, static_cast<int>(winWidth / 4));
            const int margin = static_cast<int>(20 * std::min(float(winWidth)/1600.0f, float(winHeight)/1200.0f));
            const int startX = controlPanelWidth + margin;
            int x = startX;
            int y = 10; // Status bar Y position

            // Draw Status Bar items
            std::string text;
            
            // 1. Shape
            text = "Shape: " + std::string(Curve::name(m_selectedShape));
            if (m_shapeVariation) text += " (Var)";
            if (m_invert) text += " (Inv)";
            renderText(text, x, y, {255,255,255,255}); 
            
            // 2. Voltage Range
            x += 220;
            text = "Range: " + std::string(voltageRangeName(m_params.range));
            renderText(text, x, y, {255,255,255,255}); 
            
            // 3. Sample Rate
            x += 200;
            text = "Rate: " + std::to_string(m_sampleRate) + "Hz";
            renderText(text, x, y, {255,255,255,255}); 

            // 4. Performance (Moved to right side of top bar)
            x = winWidth - 250; 
            const auto& perf = m_processor.getPerformance();
            char perfText[128];
            snprintf(perfText, sizeof(perfText), "CPU: %.1f%% (%.2fms)", perf.cpuUsagePercent, perf.processTimeMs);
            SDL_Color perfColor = {0, 255, 0, 255};  // Green by default
            if (perf.cpuUsagePercent > 80.0f) perfColor = {255, 255, 0, 255};  // Yellow for 80%+
            if (perf.cpuUsagePercent > 95.0f) perfColor = {255, 0, 0, 255};    // Red for 95%+
            renderText(perfText, x, y, perfColor);

            // --- Hardware Safety Analysis Panel ---
            const auto& stats = m_processor.getHardwareStats();
            int safeX = startX; 
            int safeY = y + 30;
            
            renderText("Hardware Safety:", safeX, safeY, {200, 200, 200, 255});
            
            // Metric 1: Max Slew Rate
            safeX += 150;
            char slewText[64];
            snprintf(slewText, sizeof(slewText), "Max Step: %.2f V", stats.maxSlewRate);
            SDL_Color slewColor = {0, 255, 0, 255};
            if (stats.maxSlewRate > 0.5f) slewColor = {255, 165, 0, 255}; // Orange > 0.5V
            if (stats.maxSlewRate > 1.0f) slewColor = {255, 50, 50, 255}; // Red > 1.0V
            renderText(slewText, safeX, safeY, slewColor);

            // Metric 2: Algo Complexity
            safeX += 180;
            std::string algoText = "Algo Cost: " + std::to_string(stats.algoComplexityScore);
            SDL_Color algoColor = {0, 255, 0, 255};
            if (stats.algoComplexityScore > 10) algoColor = {255, 165, 0, 255}; // Medium
            if (stats.algoComplexityScore > 15) algoColor = {255, 50, 50, 255}; // High
            renderText(algoText, safeX, safeY, algoColor);

            // Metric 3: Clipping
            safeX += 150;
            char clipText[64];
            snprintf(clipText, sizeof(clipText), "Clipping: %.1f%%", stats.clippingPercent);
            SDL_Color clipColor = {0, 255, 0, 255};
            if (stats.clippingPercent > 1.0f) clipColor = {255, 165, 0, 255};
            if (stats.clippingPercent > 5.0f) clipColor = {255, 50, 50, 255};
            renderText(clipText, safeX, safeY, clipColor);
            
            
            // Spectrum Legend (positioned above spectrum)
            int spectrumHeight = 200;
            int spectrumY = winHeight - spectrumHeight - 50; 
            int legendY = spectrumY - 25;
            x = startX;
            
            renderText("Spectrum Source (Keys A-H):", x, legendY, {200,200,200,255}); 
            x += 220;
            
            const char* sourceNames[] = {"Input", "Skewed", "Post Wave", "Post Filt", "Post Comp", "Output"};
            for(int i = 0; i < int(SpectrumSource::Last); ++i) {
                SDL_Color color = (i == int(m_selectedSpectrumSource)) ? SDL_Color{255, 255, 0, 255} : SDL_Color{150, 150, 150, 255};
                renderText(sourceNames[i], x, legendY, color);
                x += 90; // Reduced spacing to fit
            }
            
            // Shortcuts Hint (Moved to very bottom)
            std::string shortcuts = "Keys: R-Reset, S-Var, I-Inv, 1-8-Shape, L-Range, P-Audio";
            renderText(shortcuts, controlPanelWidth + margin, winHeight - 20, {100,100,100,255});
            
            // Audio Status (Top Right, below CPU)
            if (m_audioEngine.m_active) {
                renderText("AUDIO ON", winWidth - 100, y + 30, {0, 255, 0, 255});
            }
        }
#endif
    }

    void renderText(const std::string& text, int x, int y, SDL_Color color) {
        SDL_Surface* s = TTF_RenderText_Solid(m_font, text.c_str(), color);
        SDL_Texture* t = SDL_CreateTextureFromSurface(m_renderer, s);
        SDL_Rect r = {x, y, s->w, s->h};
        SDL_RenderCopy(m_renderer, t, nullptr, &r);
        SDL_FreeSurface(s);
        SDL_DestroyTexture(t);
    }

    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
#ifdef HAS_SDL2_TTF
    TTF_Font* m_font;
#endif
    bool m_running;
    CurveProcessor m_processor{2048};
    CurveProcessor::Parameters m_params;
    CurveProcessor::SignalData m_signalData;
    std::vector<Control> m_controls;
    Curve::Type m_selectedShape;
    bool m_shapeVariation, m_invert;
    float m_min, m_max;
    int m_sampleRate = 48000;
    SpectrumSource m_selectedSpectrumSource;
    Control* m_selectedControl;
    std::vector<ControlSection> m_sections;
    std::string m_textInputBuffer;
    float m_dpi;
    SDL_AudioDeviceID m_audioDeviceId = 0;
    AudioEngine m_audioEngine;
    void updateSampleRate(int newRate) { if (newRate >= 500 && newRate <= 192000) m_sampleRate = newRate; }
};

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    CurveAnalysisApp app;
    app.run();
    return 0;
}
