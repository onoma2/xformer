#pragma once

#include <SDL2/SDL.h>
#ifdef HAS_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif
#include <vector>
#include <string>

#include "AlgorithmProcessor.h"

// Visualization types for different algorithm representations
enum class VisualizationType {
    NoteSequence,
    GateSequence,
    VelocitySequence,
    Spectrum,
    StepProbability,
    GateOffset,
    IsTrill,
    Last
};

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

class AlgoVisualization {
public:
    AlgoVisualization();
    ~AlgoVisualization();

    void render(SDL_Renderer* renderer, const AlgorithmProcessor::SignalData& data);
    void update();
    void handleEvents(const SDL_Event& event);
    
    // Control management
    void addControl(const std::string& name, float* value, float min, float max, int x, int y, int w, int h);
    void addBooleanControl(const std::string& name, bool* value, int x, int y, int w, int h);
    void updateControlsLayout();
    void resetControls();
    
    // Algorithm parameters
    AlgorithmParameters& getParameters() { return m_params; }
    const AlgorithmParameters& getParameters() const { return m_params; }
    
    // Visualization type
    VisualizationType getVisualizationType() const { return m_visType; }
    void setVisualizationType(VisualizationType type) { m_visType = type; }
    
    // Getters for UI state
    bool isPlaying() const { return m_playing; }
    void togglePlay() { m_playing = !m_playing; }

private:
    // Control system
    std::vector<Control> m_controls;
    std::vector<ControlSection> m_sections;
    Control* m_selectedControl;
    std::string m_textInputBuffer;
    
    // Algorithm parameters
    AlgorithmParameters m_params;
    
    // Visualization settings
    VisualizationType m_visType;
    bool m_playing;
    SDL_Texture* m_plotTexture;
    int m_plotWidth, m_plotHeight;
    
    // UI elements
    SDL_Rect m_plotRect;
    SDL_Rect m_controlsRect;
    
    // Font (if available)
#ifdef HAS_SDL2_TTF
    TTF_Font* m_font;
#endif
    
    // DPI for scaling
    float m_dpi;
    float m_scale;
    
    // Private methods
    void initControls();
    void renderPlot(SDL_Renderer* renderer, const AlgorithmProcessor::SignalData& data);
    void renderControls(SDL_Renderer* renderer);
    void renderVisualization(SDL_Renderer* renderer, const std::vector<float>& data, SDL_Color color);
    void handleMouse(const SDL_Event& e);
    void handleMouseMotion(const SDL_Event& e);
    void handleKeyDown(const SDL_Keycode key, const SDL_Keymod mod);
    void handleTextInput(const char* text);
    void drawText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color);
#ifdef HAS_SDL2_TTF
    void renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color);
#endif
    std::string formatFloat(float value, int decimals = 2);
};