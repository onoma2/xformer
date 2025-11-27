#include <SDL2/SDL.h>
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

struct Control {
    std::string name;
    float* value;
    float min;
    float max;
    float step;
    SDL_Rect rect;
    bool dragging;
    SDL_Rect textRect;
};

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
        if (SDL_Init(SDL_INIT_VIDEO) < 0) { std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl; return; }
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

        m_window = SDL_CreateWindow("Curve Analysis Tool", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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
        m_running = true;
        SDL_StartTextInput();
    }

    ~CurveAnalysisApp() {
        SDL_StopTextInput();
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

        // Main controls - positions will be updated in updateControlsLayout()
        m_controls.push_back({"Global Phase", &m_params.globalPhase, -1.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Wavefolder Fold", &m_params.wavefolderFold, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Wavefolder Gain", &m_params.wavefolderGain, 0.0f, 2.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Wavefolder Symmetry", &m_params.wavefolderSymmetry, -1.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"DJ Filter", &m_params.djFilter, -1.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Filter F (Res)", &m_params.filterF, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Fold F (Feedback)", &m_params.foldF, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Crossfade", &m_params.xFade, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Min", &m_min, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Max", &m_max, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;

        // Advanced controls
        y += 20;
        m_controls.push_back({"Fold Amount", &m_params.advanced.foldAmount, 1.0f, 32.0f, 0.1f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"HPF Curve", &m_params.advanced.hpfCurve, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Resonance Gain", &m_params.advanced.resonanceGain, 0.5f, 4.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Resonance Tame", &m_params.advanced.resonanceTame, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Feedback Curve", &m_params.advanced.feedbackCurve, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Fold Comp", &m_params.advanced.foldComp, 0.0f, 2.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"LPF Comp", &m_params.advanced.lpfComp, 0.0f, 2.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"HPF Comp", &m_params.advanced.hpfComp, 0.0f, 2.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Res Comp", &m_params.advanced.resComp, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Max Comp", &m_params.advanced.maxComp, 1.0f, 5.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"LFO Limiter Amt", &m_params.advanced.lfoLimiterAmount, 0.0f, 5.0f, 0.1f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"LFO Limiter Min", &m_params.advanced.lfoLimiterMin, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"Feedback Limit", &m_params.advanced.feedbackLimit, 0.0f, 8.0f, 0.1f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;

        // Hardware limitation controls
        y += 20;
        m_controls.push_back({"DAC Resolution (bits)", (float*)&m_params.dacResolutionBits, 8.0f, 24.0f, 1.0f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;
        m_controls.push_back({"DAC Update Rate", &m_params.dacUpdateRate, 0.001f, 10.0f, 0.001f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;  // Update rate in ms (0.001ms to 10ms)
        m_controls.push_back({"Timing Jitter (ms)", &m_params.timingJitter, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false, {0,0,0,0}}); y += controlHeight + spacing;  // Reduced jitter range
    }

    void resetControls() {
        m_params = CurveProcessor::Parameters();
        m_min = m_params.min;
        m_max = m_params.max;
        m_selectedShape = m_params.shape;
        m_shapeVariation = m_params.shapeVariation;
        m_invert = m_params.invert;
        m_selectedSpectrumSource = m_params.spectrumSource;
        m_processor.resetStates();
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
            for (auto& c : m_controls) {
                if (SDL_PointInRect(&p, &c.rect)) c.dragging = (e.type == SDL_MOUSEBUTTONDOWN);
                if (SDL_PointInRect(&p, &c.textRect) && e.type == SDL_MOUSEBUTTONDOWN) {
                    m_selectedControl = &c;
                    m_textInputBuffer.clear();
                }
            }
        }
    }

    void handleMouseMotion(const SDL_Event& e) {
        SDL_Point p = {e.motion.x, e.motion.y};
        for (auto& c : m_controls) {
            if (c.dragging) {
                if (SDL_PointInRect(&p, &c.rect)) {
                    float pos = (float)(e.motion.x - c.rect.x) / c.rect.w;
                    *c.value = c.min + std::clamp(pos, 0.f, 1.f) * (c.max - c.min);
                }
            }
        }
    }

    void updateControlsLayout() {
        int winWidth, winHeight;
        SDL_GetWindowSize(m_window, &winWidth, &winHeight);

        const int controlPanelWidth = std::max(300, static_cast<int>(winWidth / 4));
        const int margin = static_cast<int>(20 * std::min(float(winWidth)/1600.0f, float(winHeight)/1200.0f));
        const int controlX = margin;  // Left side of the window for controls
        const int controlWidth = static_cast<int>(controlPanelWidth * 0.8f);
        const int controlHeight = std::max(15, static_cast<int>(20 * std::min(float(winWidth)/1600.0f, float(winHeight)/1200.0f)));

        int y = 30;
        int spacing = 5;

        // Update positions for main controls (first 10)
        for (int i = 0; i < 10; ++i) {  // First 10 are main controls
            m_controls[i].rect = {controlX, y, controlWidth, controlHeight};
            y += controlHeight + spacing;
        }

        // Update positions for advanced controls (next group)
        y += 20;
        for (int i = 10; i < 23; ++i) { // Advanced controls from index 10 to 22
            m_controls[i].rect = {controlX, y, controlWidth, controlHeight};
            y += controlHeight + spacing;
        }

        // Update positions for hardware limitation controls (last 3)
        y += 20;
        for (int i = 23; i < static_cast<int>(m_controls.size()); ++i) {
            m_controls[i].rect = {controlX, y, controlWidth, controlHeight};
            y += controlHeight + spacing;
        }
    }

    void update() {
        updateControlsLayout();  // Update control positions based on window size

        m_params.min = m_min;
        m_params.max = m_max;
        m_params.shape = m_selectedShape;
        m_params.shapeVariation = m_shapeVariation;
        m_params.invert = m_invert;
        m_params.spectrumSource = m_selectedSpectrumSource;
        m_signalData = m_processor.process(m_params, m_sampleRate);
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
        const int topMargin = margin;

        // Calculate available space for graphs, reserving space at the bottom for spectrum
        const int spectrumHeight = 250; // Fixed height for spectrum
        const int availableWidth = winWidth - controlPanelWidth - 3 * margin;
        const int availableHeight = winHeight - spectrumHeight - 100; // Reserve space for spectrum and other UI elements

        const int graphWidth = availableWidth / 3;
        const int graphHeight = availableHeight / 2;

        std::vector<std::pair<std::string, const std::vector<float>*>> graphs = {
            {"Input Signal", &m_signalData.originalSignal},
            {"Post Wavefolder", &m_signalData.postWavefolder},
            {"Post Filter", &m_signalData.postFilter},
            {"Post Compensation", &m_signalData.postCompensation},
            {"Final Output", &m_signalData.finalOutput},
            {"Hardware Limited Output", &m_signalData.hardwareLimitedOutput}  // With all hardware constraints applied
        };

        for (size_t i = 0; i < graphs.size(); ++i) {
            int row = i / 3, col = i % 3;
            int x = controlPanelWidth + margin + col * (graphWidth + margin);
            int y = topMargin + margin + row * (graphHeight + margin + 30);
            drawGraph(graphs[i].first, *graphs[i].second, i, x, y, graphWidth, graphHeight);
        }
    }

    void drawGraph(const std::string& title, const std::vector<float>& data, int index, int x, int y, int w, int h) {
        SDL_Rect bg = {x, y, w, h};
        SDL_SetRenderDrawColor(m_renderer, 30, 30, 30, 255);
        SDL_RenderFillRect(m_renderer, &bg);
        drawGridLines(x, y, w, h);
        drawCenterLine(x, y, w, h);

        bool isNormalized = (index <= 1);
        SDL_Color color = (index == 4) ? SDL_Color{255, 255, 0, 255} : ((index==0) ? SDL_Color{100,100,255,255} : SDL_Color{0,255,0,255});
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
        int y = winHeight - 300; // Position at bottom
        int w = winWidth - controlPanelWidth - 2 * margin;
        int h = 250; // Fixed height for spectrum

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
        for (auto& c : m_controls) {
            SDL_SetRenderDrawColor(m_renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(m_renderer, &c.rect);
            float pos = (*c.value - c.min) / (c.max - c.min);
            SDL_Rect fill = {c.rect.x, c.rect.y, (int)(c.rect.w * pos), c.rect.h};
            SDL_SetRenderDrawColor(m_renderer, 0, 100, 200, 255);
            SDL_RenderFillRect(m_renderer, &fill);

            if (c.min < 0) {
                int centerX = c.rect.x + c.rect.w / 2;
                SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
                SDL_RenderDrawLine(m_renderer, centerX, c.rect.y, centerX, c.rect.y + c.rect.h);
            }

            SDL_SetRenderDrawColor(m_renderer, 100, 100, 100, 255);
            SDL_RenderDrawRect(m_renderer, &c.rect);
#ifdef HAS_SDL2_TTF
            if (m_font) {
                renderText(c.name, 20, c.rect.y + 5, {255, 255, 255, 255});
                std::stringstream ss;
                if (&c == m_selectedControl) {
                    ss << m_textInputBuffer;
                    if (int(SDL_GetTicks() / 500) % 2 == 0) {
                        ss << "|";
                    }
                } else {
                    ss << std::fixed << std::setprecision(2) << *c.value;
                }
                SDL_Surface* valueSurface = TTF_RenderText_Solid(m_font, ss.str().c_str(), {255,255,255,255});
                c.textRect = {c.rect.x + c.rect.w + 10, c.rect.y + 5, valueSurface->w, valueSurface->h};
                renderText(ss.str(), c.textRect.x, c.textRect.y, (&c == m_selectedControl) ? SDL_Color{255,255,0,255} : SDL_Color{255,255,255,255});
                SDL_FreeSurface(valueSurface);
            }
#endif
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
            int x = controlPanelWidth + margin;  // Position to the right of the control panel
            int y = 30;  // Start at the same vertical level as the controls

            renderText("Shape: " + std::string(Curve::name(m_selectedShape)), x, y, {255,255,255,255}); y+=20;
            renderText("Voltage Range: " + std::string(voltageRangeName(m_params.range)) + " (L to change)", x, y, {255,255,255,255}); y+=20;
            renderText("Controls: R-reset, S-var, I-inv, 1-8-shapes", x, y, {255,255,255,255}); y+=20;
            renderText("Sample Rate: " + std::to_string(m_sampleRate) + "Hz (Z-M)", x, y, {255,255,255,255}); y+=20;

            // Draw performance information
            const auto& perf = m_processor.getPerformance();
            y += 10;
            renderText("Performance:", x, y, {255,255,255,255}); y+=20;
            char perfText[128];
            snprintf(perfText, sizeof(perfText), "Process: %.3f ms", perf.processTimeMs);
            renderText(perfText, x, y, {255,255,255,255}); y+=20;
            snprintf(perfText, sizeof(perfText), "Time Budget: %.3f ms", perf.timeBudgetMs);
            renderText(perfText, x, y, {255,255,255,255}); y+=20;
            snprintf(perfText, sizeof(perfText), "CPU Usage: %.1f%%", perf.cpuUsagePercent);
            SDL_Color perfColor = {0, 255, 0, 255};  // Green by default
            if (perf.cpuUsagePercent > 80.0f) perfColor = {255, 255, 0, 255};  // Yellow for 80%+
            if (perf.cpuUsagePercent > 95.0f) perfColor = {255, 0, 0, 255};    // Red for 95%+
            renderText(perfText, x, y, perfColor); y+=20;

            y+=20;
            renderText("Spectrum Source (A,D,F,G,H):", x, y, {255,255,255,255}); y+=20;
            const char* sourceNames[] = {"Input", "Post Wavefolder", "Post Filter", "Post Compensation", "Final Output"};
            for(int i = 0; i < int(SpectrumSource::Last); ++i) {
                SDL_Color color = (i == int(m_selectedSpectrumSource)) ? SDL_Color{255, 255, 0, 255} : SDL_Color{255, 255, 255, 255};
                renderText(sourceNames[i], x + i * 150, y, color);
            }

            y+=40;
            renderText("Advanced Controls (Q/W, E/T, ...):", x, y, {255,255,255,255});
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
    std::string m_textInputBuffer;
    float m_dpi;
    void updateSampleRate(int newRate) { if (newRate >= 500 && newRate <= 192000) m_sampleRate = newRate; }
};

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    CurveAnalysisApp app;
    app.run();
    return 0;
}
