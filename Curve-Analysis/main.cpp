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

struct Control {
    std::string name;
    float* value;
    float min;
    float max;
    float step;
    SDL_Rect rect;
    bool dragging;
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
        m_max(1.0f)
    {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) { std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl; return; }
#ifdef HAS_SDL2_TTF
        if (TTF_Init() == -1) { std::cerr << "TTF could not initialize! TTF_Error: " << TTF_GetError() << std::endl; return; }
#endif
        m_window = SDL_CreateWindow("Curve Analysis Tool", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
        if (!m_window) { std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl; return; }
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
        if (!m_renderer) { std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl; return; }
#ifdef HAS_SDL2_TTF
        m_font = TTF_OpenFont("/System/Library/Fonts/SFNS.ttf", 14);
        if (!m_font) m_font = TTF_OpenFont("/System/Library/Fonts/Arial.ttf", 14);
        if (!m_font) m_font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 14);
        if (!m_font) std::cerr << "Font could not be loaded! TTF_Error: " << TTF_GetError() << std::endl;
#endif
        initControls();
        m_processor.resetStates();
        m_running = true;
    }

    ~CurveAnalysisApp() {
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
                else if (e.type == SDL_KEYDOWN) handleKeyDown(e.key.keysym.sym);
                else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) handleMouse(e);
                else if (e.type == SDL_MOUSEMOTION) handleMouseMotion(e);
            }
            update();
            render();
            SDL_Delay(16);
        }
    }

private:
    void initControls() {
        int y = 30;
        int controlHeight = 30;
        int controlWidth = 200;
        int spacing = 10;
        m_controls.push_back({"Global Phase", &m_params.globalPhase, -1.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false}); y += controlHeight + spacing;
        m_controls.push_back({"Wavefolder Fold", &m_params.wavefolderFold, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false}); y += controlHeight + spacing;
        m_controls.push_back({"Wavefolder Gain", &m_params.wavefolderGain, 0.0f, 2.0f, 0.01f, {250, y, controlWidth, controlHeight}, false}); y += controlHeight + spacing;
        m_controls.push_back({"Wavefolder Symmetry", &m_params.wavefolderSymmetry, -1.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false}); y += controlHeight + spacing;
        m_controls.push_back({"DJ Filter", &m_params.djFilter, -1.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false}); y += controlHeight + spacing;
        m_controls.push_back({"Filter F (Res)", &m_params.filterF, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false}); y += controlHeight + spacing;
        m_controls.push_back({"Fold F (Feedback)", &m_params.foldF, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false}); y += controlHeight + spacing;
        m_controls.push_back({"Crossfade", &m_params.xFade, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false}); y += controlHeight + spacing;
        m_controls.push_back({"Min", &m_min, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false}); y += controlHeight + spacing;
        m_controls.push_back({"Max", &m_max, 0.0f, 1.0f, 0.01f, {250, y, controlWidth, controlHeight}, false});
    }

    void resetControls() {
        m_params = CurveProcessor::Parameters();
        m_min = m_params.min;
        m_max = m_params.max;
        m_selectedShape = m_params.shape;
        m_shapeVariation = m_params.shapeVariation;
        m_invert = m_params.invert;
        m_processor.resetStates();
    }

    void handleKeyDown(SDL_Keycode key) {
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
            default: break;
        }
    }

    void handleMouse(const SDL_Event& e) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            SDL_Point p = {e.button.x, e.button.y};
            for (auto& c : m_controls) {
                if (SDL_PointInRect(&p, &c.rect)) c.dragging = (e.type == SDL_MOUSEBUTTONDOWN);
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

    void update() {
        m_params.min = m_min;
        m_params.max = m_max;
        m_params.shape = m_selectedShape;
        m_params.shapeVariation = m_shapeVariation;
        m_params.invert = m_invert;
        m_signalData = m_processor.process(m_params, m_sampleRate);
    }

    void render() {
        SDL_SetRenderDrawColor(m_renderer, 20, 20, 20, 255);
        SDL_RenderClear(m_renderer);
        drawWaveforms();
        drawControls();
        drawInfo();
        SDL_RenderPresent(m_renderer);
    }

    void drawWaveforms() {
        const int margin = 40, controlPanelWidth = 450, topMargin = 50;
        const int graphWidth = (1600 - controlPanelWidth - 4 * margin) / 3;
        const int graphHeight = (900 - topMargin - 3 * margin) / 2;

        std::vector<std::pair<std::string, const std::vector<float>*>> graphs = {
            {"Input Signal", &m_signalData.originalSignal},
            {"Post Wavefolder", &m_signalData.postWavefolder},
            {"Post Filter", &m_signalData.postFilter},
            {"Post Compensation", &m_signalData.postCompensation},
            {"Final Output", &m_signalData.finalOutput}
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
            SDL_RenderDrawLine(m_renderer, x + (i-1)*w/(data.size()-1), y + n1*h, x + i*w/(data.size()-1), y + n2*h);
        }
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
        for (const auto& c : m_controls) {
            SDL_SetRenderDrawColor(m_renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(m_renderer, &c.rect);
            float pos = (*c.value - c.min) / (c.max - c.min);
            SDL_Rect fill = {c.rect.x, c.rect.y, (int)(c.rect.w * pos), c.rect.h};
            SDL_SetRenderDrawColor(m_renderer, 0, 100, 200, 255);
            SDL_RenderFillRect(m_renderer, &fill);

            // Draw center line for bipolar controls
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
                std::stringstream ss; ss << std::fixed << std::setprecision(2) << *c.value;
                renderText(ss.str(), c.rect.x + c.rect.w + 10, c.rect.y + 5, {255, 255, 255, 255});
            }
#endif
        }
    }

    void drawInfo() {
#ifdef HAS_SDL2_TTF
        if (m_font) {
            int y = 400;
            renderText("Shape: " + std::string(Curve::name(m_selectedShape)), 20, y, {255,255,255,255}); y+=20;
            renderText("Voltage Range: " + std::string(voltageRangeName(m_params.range)) + " (L to change)", 20, y, {255,255,255,255}); y+=20;
            renderText("Controls: R-reset, S-var, I-inv, 1-8-shapes", 20, y, {255,255,255,255}); y+=20;
            renderText("Sample Rate: " + std::to_string(m_sampleRate) + "Hz (Z-M)", 20, y, {255,255,255,255});
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
    CurveProcessor m_processor;
    CurveProcessor::Parameters m_params;
    CurveProcessor::SignalData m_signalData;
    std::vector<Control> m_controls;
    Curve::Type m_selectedShape;
    bool m_shapeVariation, m_invert;
    float m_min, m_max;
    int m_sampleRate = 48000;
    void updateSampleRate(int newRate) { if (newRate >= 500 && newRate <= 192000) m_sampleRate = newRate; }
};

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    CurveAnalysisApp app;
    app.run();
    return 0;
}