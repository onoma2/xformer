#include "AlgoVisualization.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <iostream>

// Helper functions for Control initialization
static Control makeFloatControl(const std::string& name, float* valuePtr, float minVal, float maxVal, float stepVal, int x, int y, int w, int h) {
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

static Control makeIntControl(const std::string& name, int* intPtr, int minVal, int maxVal, int stepVal, int x, int y, int w, int h) {
    Control c;
    c.name = name;
    // Cast int pointer to float pointer (for union access) - this is safe for our use case
    // We'll handle the int/float conversion in the drag functions
    c.value = reinterpret_cast<float*>(intPtr);
    c.min = static_cast<float>(minVal);
    c.max = static_cast<float>(maxVal);
    c.step = static_cast<float>(stepVal);
    c.defaultValue = static_cast<float>(*intPtr); // Store current value as default
    c.defaultBooleanValue = false;
    c.rect = {x, y, w, h};
    c.dragging = false;
    c.hovered = false;
    c.textRect = {0,0,0,0};
    c.isBoolean = false;
    return c;
}

static Control makeBoolControl(const std::string& name, bool* boolPtr, int x, int y, int w, int h) {
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

AlgoVisualization::AlgoVisualization() :
    m_selectedControl(nullptr),
    m_visType(VisualizationType::NoteSequence),
    m_playing(false),
    m_plotTexture(nullptr),
    m_plotWidth(0),
    m_plotHeight(0),
#ifdef HAS_SDL2_TTF
    m_font(nullptr),
#endif
    m_dpi(96.0f),
    m_scale(1.0f)
{
    // Initialize algorithm parameters with defaults
    m_params = AlgorithmParameters();

    // Initialize plot texture dimensions
    m_plotWidth = 800;
    m_plotHeight = 300;  // Increase height for better visibility

#ifdef HAS_SDL2_TTF
    // Initialize font if available
    m_font = TTF_OpenFont("/System/Library/Fonts/SFNS.ttf", 14);
    if (!m_font) m_font = TTF_OpenFont("/System/Library/Fonts/Arial.ttf", 14);
    if (!m_font) m_font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 14);
    if (!m_font) std::cerr << "Font could not be loaded!" << std::endl;
#else
    // Ensure m_font is not accessed when HAS_SDL2_TTF is not defined
#endif

    // Initialize controls
    initControls();

    // Set the plot rectangle - needs to be done after controls are initialized
    // since updateControlsLayout will be called which sets the plot rectangle position
    m_plotRect = {250, 50, m_plotWidth, m_plotHeight};
}

AlgoVisualization::~AlgoVisualization() {
    if (m_plotTexture) {
        SDL_DestroyTexture(m_plotTexture);
    }
#ifdef HAS_SDL2_TTF
    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }
#endif
}

void AlgoVisualization::initControls() {
    int y = 30;
    int controlHeight = 20;
    int controlWidth = 150;
    int spacing = 5;

    // Algorithm selection
    // Create a "control" for algorithm type (will be handled specially in UI)
    m_controls.push_back(makeFloatControl("Algorithm", reinterpret_cast<float*>(&m_params.type), 0.0f, static_cast<float>(AlgorithmType::Last)-0.1f, 1.0f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;

    // Main parameters
    m_controls.push_back(makeIntControl("Flow", &m_params.flow, 1, 16, 1, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
    m_controls.push_back(makeIntControl("Ornament", &m_params.ornament, 1, 16, 1, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
    m_controls.push_back(makeIntControl("Power", &m_params.power, 0, 16, 1, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
    m_controls.push_back(makeIntControl("Glide", &m_params.glide, 0, 16, 1, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
    m_controls.push_back(makeIntControl("Trill", &m_params.trill, 0, 8, 1, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;

    // Pattern and sequence parameters
    y += 10; // Extra spacing
    m_controls.push_back(makeIntControl("Steps", &m_params.steps, 1, 64, 1, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
    m_controls.push_back(makeIntControl("Loop Length", &m_params.loopLength, 0, 64, 1, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;

    // Custom algorithm parameters
    y += 10; // Extra spacing
    m_controls.push_back(makeFloatControl("Custom Param 1", &m_params.customParam1, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
    m_controls.push_back(makeFloatControl("Custom Param 2", &m_params.customParam2, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
    m_controls.push_back(makeFloatControl("Custom Param 3", &m_params.customParam3, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;
    m_controls.push_back(makeFloatControl("Custom Param 4", &m_params.customParam4, 0.0f, 1.0f, 0.01f, 20, y, controlWidth, controlHeight)); y += controlHeight + spacing;

    // Initialize Sections
    m_sections.push_back({"Algorithm Parameters", 0, 6, false, {0,0,0,0}}); // Algorithm + 5 main params, Open by default
    m_sections.push_back({"Pattern Parameters", 6, 2, true, {0,0,0,0}}); // 2 pattern params, Collapsed by default
    m_sections.push_back({"Custom Algorithm", 8, 4, true, {0,0,0,0}}); // 4 custom params, Collapsed by default
}

void AlgoVisualization::addControl(const std::string& name, float* value, float min, float max, int x, int y, int w, int h) {
    m_controls.push_back(makeFloatControl(name, value, min, max, (max-min)/100.0f, x, y, w, h));
}

void AlgoVisualization::addBooleanControl(const std::string& name, bool* value, int x, int y, int w, int h) {
    m_controls.push_back(makeBoolControl(name, value, x, y, w, h));
}

void AlgoVisualization::updateControlsLayout() {
    // Update section header rectangles - starting position
    int currentY = 10;

    for (size_t s = 0; s < m_sections.size(); s++) {
        auto& section = m_sections[s];

        // Position the section header
        section.headerRect = {10, currentY, 200, 25};
        currentY += 30; // Section header height

        // Position controls in this section if not collapsed
        if (!section.collapsed) {
            for (int i = section.startIndex; i < section.startIndex + section.count; i++) {
                if (i >= 0 && i < static_cast<int>(m_controls.size())) {
                    // Position this control
                    m_controls[i].rect.x = 20;  // Fixed x position for controls
                    m_controls[i].rect.y = currentY;
                    currentY += m_controls[i].rect.h + 5;  // Move to next position
                }
            }
        }
    }

    // Update plot rectangle position - only set if not already initialized
    if (m_plotRect.w == 0 || (m_plotRect.x == 250 && m_plotRect.y == 10 && m_plotRect.w == 800 && m_plotRect.h == 300)) {
        // Only use default if it hasn't been set properly
        m_plotRect = {250, 50, m_plotWidth, m_plotHeight};
    }
}

void AlgoVisualization::render(SDL_Renderer* renderer, const AlgorithmProcessor::SignalData& data) {
    // Clear the renderer
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    // Render the visualization plot
    renderPlot(renderer, data);
    
    // Render the controls
    renderControls(renderer);
}

void AlgoVisualization::renderPlot(SDL_Renderer* renderer, const AlgorithmProcessor::SignalData& data) {
    // Set the plot area background
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_RenderFillRect(renderer, &m_plotRect);
    
    // Render the selected visualization type
    SDL_Color color = {0, 255, 0, 255};  // Green by default
    std::vector<float> visData;
    
    switch (m_visType) {
        case VisualizationType::NoteSequence:
            visData = data.noteSequence;
            color = {0, 255, 0, 255};  // Green for notes
            break;
        case VisualizationType::GateSequence:
            visData = data.gateSequence;
            color = {255, 0, 0, 255};  // Red for gates
            break;
        case VisualizationType::VelocitySequence:
            visData = data.velocitySequence;
            color = {0, 0, 255, 255};  // Blue for velocity
            break;
        case VisualizationType::Spectrum:
            visData = data.spectrum;
            color = {255, 255, 0, 255};  // Yellow for spectrum
            break;
        case VisualizationType::StepProbability:
            visData = data.probabilitySequence;
            color = {255, 0, 255, 255};  // Magenta for probability
            break;
        case VisualizationType::GateOffset:
            visData = data.gateOffsetSequence;
            color = {0, 255, 255, 255};  // Cyan for gate offset
            break;
        case VisualizationType::IsTrill:
            visData = data.isTrillSequence;
            color = {128, 0, 128, 255};  // Purple for trill
            break;
        default:
            visData = data.noteSequence;
            break;
    }
    
    if (!visData.empty()) {
        renderVisualization(renderer, visData, color);
    }
    
    // Draw grid lines
    SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255);
    // Vertical grid lines (for steps)
    for (int i = 0; i <= 16; i++) {
        int x = m_plotRect.x + (i * m_plotRect.w) / 16;
        SDL_RenderDrawLine(renderer, x, m_plotRect.y, x, m_plotRect.y + m_plotRect.h);
    }
    // Horizontal grid lines
    for (int i = 0; i <= 10; i++) {
        int y = m_plotRect.y + (i * m_plotRect.h) / 10;
        SDL_RenderDrawLine(renderer, m_plotRect.x, y, m_plotRect.x + m_plotRect.w, y);
    }
    
    // Draw axes
    SDL_SetRenderDrawColor(renderer, 100, 100, 150, 255);
    SDL_RenderDrawLine(renderer, m_plotRect.x, m_plotRect.y, m_plotRect.x, m_plotRect.y + m_plotRect.h);  // Y-axis
    SDL_RenderDrawLine(renderer, m_plotRect.x, m_plotRect.y + m_plotRect.h, m_plotRect.x + m_plotRect.w, m_plotRect.y + m_plotRect.h);  // X-axis
    
    // Draw title
    std::string title = "Algorithm: " + AlgorithmProcessor::getAlgorithmName(m_params.type);
    drawText(renderer, title, m_plotRect.x + 10, m_plotRect.y + 5, {200, 200, 255, 255});
    
    std::string visTitle;
    switch (m_visType) {
        case VisualizationType::NoteSequence: visTitle = "Note Sequence"; break;
        case VisualizationType::GateSequence: visTitle = "Gate Sequence"; break;
        case VisualizationType::VelocitySequence: visTitle = "Velocity Sequence"; break;
        case VisualizationType::Spectrum: visTitle = "Spectrum Analysis"; break;
        case VisualizationType::StepProbability: visTitle = "Step Probability"; break;
        case VisualizationType::GateOffset: visTitle = "Gate Offset"; break;
        case VisualizationType::IsTrill: visTitle = "Trill Indicators"; break;
        default: visTitle = "Visualization";
    }
    drawText(renderer, visTitle, m_plotRect.x + 10, m_plotRect.y + 20, {200, 200, 200, 255});
}

void AlgoVisualization::renderVisualization(SDL_Renderer* renderer, const std::vector<float>& data, SDL_Color color) {
    if (data.empty()) {
        // Draw a message when no data
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_Rect msgRect = {m_plotRect.x + 10, m_plotRect.y + m_plotRect.h/2 - 10, m_plotRect.w - 20, 20};
        SDL_RenderFillRect(renderer, &msgRect);
        return;
    }

    // Draw the visualization based on current type
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    int n = data.size();
    if (n <= 1) {
        // Draw a single point if only one value
        if (n == 1) {
            int x = m_plotRect.x + m_plotRect.w / 2;
            int y = m_plotRect.y + m_plotRect.h - (data[0] * m_plotRect.h);
            for (int dy = -3; dy <= 3; dy++) {
                for (int dx = -3; dx <= 3; dx++) {
                    if (dx*dx + dy*dy <= 9) {  // Larger circle
                        int px = x + dx;
                        int py = y + dy;
                        if (px >= m_plotRect.x && px < m_plotRect.x + m_plotRect.w &&
                            py >= m_plotRect.y && py < m_plotRect.y + m_plotRect.h) {
                            SDL_RenderDrawPoint(renderer, px, py);
                        }
                    }
                }
            }
        }
        return;
    }

    // Draw lines connecting the data points - make them more visible
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int i = 0; i < n - 1; i++) {
        int x1 = m_plotRect.x + (i * m_plotRect.w) / (n-1);  // Better distribution
        int y1 = m_plotRect.y + m_plotRect.h - (data[i] * m_plotRect.h);
        int x2 = m_plotRect.x + ((i + 1) * m_plotRect.w) / (n-1);  // Better distribution
        int y2 = m_plotRect.y + m_plotRect.h - (data[i + 1] * m_plotRect.h);

        // Draw a thicker line by drawing multiple lines
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        if (y1 > 1) SDL_RenderDrawLine(renderer, x1, y1-1, x2, y2-1);
        if (y2 < m_plotRect.y + m_plotRect.h - 1) SDL_RenderDrawLine(renderer, x1, y1+1, x2, y2+1);
    }

    // Draw points for each step - make them more visible
    for (int i = 0; i < n; i++) {
        int x = m_plotRect.x + (i * m_plotRect.w) / (n > 1 ? n-1 : 1);  // Better distribution
        int y = m_plotRect.y + m_plotRect.h - (data[i] * m_plotRect.h);

        // Draw a more visible circle at each point
        for (int dy = -3; dy <= 3; dy++) {
            for (int dx = -3; dx <= 3; dx++) {
                if (dx*dx + dy*dy <= 9) {  // Larger circle
                    int px = x + dx;
                    int py = y + dy;
                    if (px >= m_plotRect.x && px < m_plotRect.x + m_plotRect.w &&
                        py >= m_plotRect.y && py < m_plotRect.y + m_plotRect.h) {
                        SDL_RenderDrawPoint(renderer, px, py);
                    }
                }
            }
        }
    }
}

void AlgoVisualization::renderControls(SDL_Renderer* renderer) {
    // Draw section headers
    for (auto& section : m_sections) {
        SDL_Color bgColor = section.collapsed ? SDL_Color{40, 40, 50, 255} : SDL_Color{60, 60, 80, 255};
        SDL_Color textColor = section.collapsed ? SDL_Color{150, 150, 170, 255} : SDL_Color{220, 220, 255, 255};
        
        SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
        SDL_RenderFillRect(renderer, &section.headerRect);
        
        SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
        SDL_RenderDrawRect(renderer, &section.headerRect);
        
        std::string headerText = (section.collapsed ? "[+] " : "[-] ") + section.name;
        drawText(renderer, headerText, section.headerRect.x + 5, section.headerRect.y + 5, textColor);
    }
    
    // Draw controls
    for (auto& control : m_controls) {
        if (control.isBoolean) {
            // Boolean toggle control
            SDL_Color bgColor = *control.booleanValue ? SDL_Color{0, 100, 0, 255} : SDL_Color{100, 0, 0, 255};
            SDL_Color borderColor = control.hovered ? SDL_Color{200, 200, 200, 255} : SDL_Color{100, 100, 100, 255};
            
            SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            SDL_RenderFillRect(renderer, &control.rect);
            
            SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            SDL_RenderDrawRect(renderer, &control.rect);
            
            drawText(renderer, control.name, control.rect.x + control.rect.w + 5, control.rect.y, 
                     *control.booleanValue ? SDL_Color{0, 255, 0, 255} : SDL_Color{255, 0, 0, 255});
        } else {
            // Slider control
            SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
            SDL_RenderFillRect(renderer, &control.rect);
            
            SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
            SDL_RenderDrawRect(renderer, &control.rect);
            
            // Draw slider value
            float range = control.max - control.min;
            float pos = range > 0 ? ((*control.value - control.min) / range) : 0.0f;
            pos = std::max(0.0f, std::min(1.0f, pos));
            
            SDL_Rect sliderFill = control.rect;
            sliderFill.w = static_cast<int>(pos * control.rect.w);
            SDL_SetRenderDrawColor(renderer, 70, 100, 150, 255);
            SDL_RenderFillRect(renderer, &sliderFill);
            
            drawText(renderer, control.name + ": " + formatFloat(*control.value), 
                     control.rect.x, control.rect.y - 15, SDL_Color{200, 200, 220, 255});
        }
    }
}

void AlgoVisualization::handleEvents(const SDL_Event& event) {
    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        handleMouse(event);
    } else if (event.type == SDL_MOUSEMOTION) {
        handleMouseMotion(event);
    } else if (event.type == SDL_KEYDOWN) {
        handleKeyDown(event.key.keysym.sym, static_cast<SDL_Keymod>(event.key.keysym.mod));
    } else if (event.type == SDL_TEXTINPUT) {
        handleTextInput(event.text.text);
    }
}

void AlgoVisualization::handleMouse(const SDL_Event& e) {
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

void AlgoVisualization::handleMouseMotion(const SDL_Event& e) {
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
                float pos = static_cast<float>(validX - c.rect.x) / c.rect.w;

                if (c.name == "Algorithm") {
                    // Special case for algorithm selection
                    int algoCount = static_cast<int>(AlgorithmType::Last) - 1;
                    *c.value = static_cast<float>(static_cast<int>(pos * algoCount));
                } else {
                    *c.value = c.min + pos * (c.max - c.min);
                }

                // Apply step increment if needed
                if (c.step != 0.0f) {
                    *c.value = c.min + std::round((*c.value - c.min) / c.step) * c.step;
                }
                *c.value = std::max(c.min, std::min(c.max, *c.value));

                // For integer controls, we need to cast the float value back to int
                // This works because we're using the same memory location via union
            } else {
                // Mouse left the rectangle: Cancel dragging immediately
                c.dragging = false;
            }
        }
    }
}

void AlgoVisualization::handleKeyDown(SDL_Keycode key, SDL_Keymod mod) {
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
        case SDLK_ESCAPE: 
            // This would typically signal the app to quit
            break;
        case SDLK_r: 
            resetControls(); 
            break;
        case SDLK_p: 
            togglePlay(); 
            break;
        case SDLK_1: setVisualizationType(VisualizationType::NoteSequence); break;
        case SDLK_2: setVisualizationType(VisualizationType::GateSequence); break;
        case SDLK_3: setVisualizationType(VisualizationType::VelocitySequence); break;
        case SDLK_4: setVisualizationType(VisualizationType::Spectrum); break;
        case SDLK_5: setVisualizationType(VisualizationType::StepProbability); break;
        case SDLK_6: setVisualizationType(VisualizationType::GateOffset); break;
        case SDLK_7: setVisualizationType(VisualizationType::IsTrill); break;

        // Algorithm selection shortcuts
        case SDLK_F1: m_params.type = AlgorithmType::Test; break;
        case SDLK_F2: m_params.type = AlgorithmType::Tritrance; break;
        case SDLK_F3: m_params.type = AlgorithmType::Stomper; break;
        case SDLK_F4: m_params.type = AlgorithmType::Markov; break;
        case SDLK_F5: m_params.type = AlgorithmType::Chiparp; break;
        case SDLK_F6: m_params.type = AlgorithmType::Goaacid; break;
        case SDLK_F7: m_params.type = AlgorithmType::Snh; break;
        case SDLK_F8: m_params.type = AlgorithmType::Wobble; break;
        case SDLK_F9: m_params.type = AlgorithmType::Techno; break;
        case SDLK_F10: m_params.type = AlgorithmType::Funk; break;
        case SDLK_F11: m_params.type = AlgorithmType::Drone; break;
        case SDLK_F12: m_params.type = AlgorithmType::Phase; break;
        
        default: break;
    }
}

void AlgoVisualization::handleTextInput(const char* text) {
    if (m_selectedControl) {
        m_textInputBuffer += text;
    }
}

void AlgoVisualization::resetControls() {
    m_params = AlgorithmParameters(); // Reset to default constructor values
    updateControlsLayout();
}

void AlgoVisualization::update() {
    // Update any animations or continuous processes here
    // For now, just keep track of playing state
}

std::string AlgoVisualization::formatFloat(float value, int decimals) {
    std::ostringstream oss;
    oss.precision(decimals);
    oss << std::fixed << value;
    return oss.str();
}

#ifdef HAS_SDL2_TTF
void AlgoVisualization::renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color) {
    if (m_font) {
        SDL_Surface* s = TTF_RenderText_Solid(m_font, text.c_str(), color);
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
        SDL_Rect r = {x, y, s->w, s->h};
        SDL_RenderCopy(renderer, t, nullptr, &r);
        SDL_FreeSurface(s);
        SDL_DestroyTexture(t);
    }
}
#endif

void AlgoVisualization::drawText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color) {
#ifdef HAS_SDL2_TTF
    if (m_font) {
        renderText(renderer, text, x, y, color);
    }
#else
    // Fallback: Draw a simple representation of text without SDL2_ttf
    // Draw each character as a simple block pattern
    if (!text.empty()) {
        int charWidth = 8;
        int charHeight = 12;
        int charSpacing = 2;

        for (size_t i = 0; i < text.length(); i++) {
            char c = text[i];
            int charX = x + i * (charWidth + charSpacing);

            // Draw a simple representation of each character
            // This is a very basic approach - draw a filled rectangle with a pattern based on the character
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_Rect charRect = {charX, y, charWidth, charHeight};
            SDL_RenderFillRect(renderer, &charRect);

            // Draw inner pattern based on character to make them distinguishable
            // This is a very simple approach to show different characters
            SDL_SetRenderDrawColor(renderer, color.r/2, color.g/2, color.b/2, color.a);
            if (c != ' ') {  // Don't draw pattern for space
                // Draw a simple internal pattern based on the character
                int patternX = charX + 2;
                int patternY = y + 2;

                // Different characters get slightly different patterns
                int patternOffset = c % 4;
                for (int py = 0; py < 4; py++) {
                    for (int px = 0; px < 4; px++) {
                        if ((px + py + patternOffset) % 3 == 0) {
                            SDL_RenderDrawPoint(renderer, patternX + px, patternY + py);
                        }
                    }
                }
            }
        }
    }
#endif
}