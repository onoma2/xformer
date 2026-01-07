import matplotlib.pyplot as plt
import matplotlib.patches as patches

# Constants matching C++ enums/styles
COLOR_LOW = '#444444'          # Dim gray
COLOR_MEDIUM = '#888888'       # Medium gray
COLOR_MEDIUM_BRIGHT = '#BBBBBB' # Light gray
COLOR_BRIGHT = '#FFFFFF'       # White
COLOR_BG = '#000000'           # Black

# Layout Constants
WIDTH = 256
HEIGHT = 64
K_LINE_COUNT = 6
K_ROW_START_Y = 4
K_ROW_STEP_Y = 8
K_EDIT_LINE_Y = 54
K_LABEL_X = 4
K_TEXT_X = 16

# Grid Configuration
# [BUS] [GAP] [MAIN] [IN/PM]
GRID_BUS_X = 196
GRID_MAIN_X = 214
GRID_EXT_X = 246
GRID_Y = 15
COL_WIDTH = 8
ROW_HEIGHT_TR = 8
ROW_HEIGHT_CV = 16

# Mock Data State
ti_states = [0, 1, 2, 1]
to_states = [1, 2, 1, 1]
cv_values = [0.0, 0.7, 1.0, 0.5]
bus_values = [0.2, 0.8, 0.0, 0.5]
in_val = 0.9
param_val = 0.1

to_ownership = [True, True, False, True]
cv_ownership = [True, True, False, True]

# Mock Script Content
script_lines = [
    "TR.X 1",             # Short line
    "CV 1 V 5",           # Short line
    "IF TR.X 1: TR.X 0",  # Medium line
    "RP 4: TR.X T",       # Medium line
    "// This is a comment", # Comment
    "L 1 0; L 2 12"       # Long line (potential collision?)
]

def draw_bar(ax, x, y, val, color_active):
    # Container (6x14 total, Inner 4x12)
    rect = patches.Rectangle((x + 1, y + 1), 6, 14, linewidth=1, edgecolor=COLOR_LOW, facecolor='none')
    ax.add_patch(rect)
    
    # Inner logic (12px height)
    center_y = y + 8
    h = int(abs(val - 0.5) * 2 * 5)
    h = max(0, min(5, h))
    
    # Center Line
    ax.add_patch(patches.Rectangle((x + 2, center_y), 4, 1, linewidth=0, facecolor=color_active))

    if h > 0:
        if val >= 0.5:
            # Up
            rect = patches.Rectangle((x + 2, center_y - h), 4, h, linewidth=0, facecolor=color_active)
        else:
            # Down
            rect = patches.Rectangle((x + 2, center_y + 1), 4, h, linewidth=0, facecolor=color_active)
        ax.add_patch(rect)

def draw_io_grid(ax):
    # --- BUS BLOCK (x=196) ---
    # Top Row (y=15): Bus 1, Bus 2
    draw_bar(ax, GRID_BUS_X, GRID_Y, bus_values[0], COLOR_MEDIUM_BRIGHT)
    draw_bar(ax, GRID_BUS_X + 8, GRID_Y, bus_values[1], COLOR_MEDIUM_BRIGHT)
    
    # Bot Row (y=31): Bus 3, Bus 4
    draw_bar(ax, GRID_BUS_X, GRID_Y + 16, bus_values[2], COLOR_MEDIUM_BRIGHT)
    draw_bar(ax, GRID_BUS_X + 8, GRID_Y + 16, bus_values[3], COLOR_MEDIUM_BRIGHT)

    # --- MAIN GRID (x=214) ---
    # Row 1: TI
    for col in range(4):
        x = GRID_MAIN_X + col * COL_WIDTH
        y = GRID_Y
        state = ti_states[col]
        color = COLOR_LOW
        if state == 1: color = COLOR_MEDIUM
        elif state == 2: color = COLOR_MEDIUM_BRIGHT
        ax.add_patch(patches.Rectangle((x + 1, y + 1), 6, 6, linewidth=0, facecolor=color))

    # Row 2: TO
    for col in range(4):
        x = GRID_MAIN_X + col * COL_WIDTH
        y = GRID_Y + ROW_HEIGHT_TR
        state = to_states[col]
        owned = to_ownership[col]
        color = COLOR_LOW
        if owned:
            if state == 1: color = COLOR_MEDIUM
            elif state == 2: color = COLOR_MEDIUM_BRIGHT
        ax.add_patch(patches.Rectangle((x + 1, y + 1), 6, 6, linewidth=0, facecolor=color))

    # Row 3: CV
    for col in range(4):
        x = GRID_MAIN_X + col * COL_WIDTH
        y = GRID_Y + ROW_HEIGHT_TR * 2
        val = cv_values[col]
        owned = cv_ownership[col]
        
        # Container
        rect = patches.Rectangle((x + 1, y + 1), 6, 14, linewidth=1, edgecolor=COLOR_LOW, facecolor='none')
        ax.add_patch(rect)
        
        if owned:
            draw_bar(ax, x, y, val, COLOR_MEDIUM_BRIGHT)

    # --- IN/PARAM BLOCK (x=246) ---
    # Top (y=15): IN (Aligned with Row 1+2)
    # Actually y=15 is Row 1. Height 16px covers Row 1+2.
    draw_bar(ax, GRID_EXT_X, GRID_Y, in_val, COLOR_MEDIUM_BRIGHT)
    
    # Bot (y=31): PARAM (Aligned with Row 3)
    draw_bar(ax, GRID_EXT_X, GRID_Y + 16, param_val, COLOR_MEDIUM_BRIGHT)

def create_mockup():
    fig, ax = plt.figure(figsize=(12, 3), dpi=150), plt.gca()
    ax.set_facecolor(COLOR_BG)
    ax.set_xlim(0, WIDTH)
    ax.set_ylim(HEIGHT, 0) # Inverted Y
    ax.set_aspect('equal')
    
    # Screen Border
    rect = patches.Rectangle((0, 0), WIDTH, HEIGHT, linewidth=1, edgecolor='#333333', facecolor='none')
    ax.add_patch(rect)
    
    # 1. Header (Icons & Pattern/Script Labels)
    ax.text(K_LABEL_X, 6+4, "M S D St", color=COLOR_MEDIUM, fontsize=8, family='monospace', verticalalignment='center')
    
    script_label = "S1"
    slot_label = "P1"
    script_x = WIDTH - 2 - (len(script_label) * 6)
    slot_x = script_x - (len(slot_label) * 6) - 4
    
    ax.text(slot_x, 6+4, slot_label, color=COLOR_MEDIUM, fontsize=8, family='monospace', verticalalignment='center')
    ax.text(script_x, 6+4, script_label, color=COLOR_MEDIUM, fontsize=8, family='monospace', verticalalignment='center')

    # 2. Script Lines
    for i in range(K_LINE_COUNT):
        y = K_ROW_START_Y + i * K_ROW_STEP_Y
        ax.text(K_LABEL_X, y + 4, str(i + 1), color=COLOR_MEDIUM, fontsize=8, family='monospace', verticalalignment='top')
        text = script_lines[i]
        color = COLOR_BRIGHT if i == 2 else COLOR_MEDIUM
        if text.startswith("//"): color = COLOR_LOW
        ax.text(K_TEXT_X, y + 4, text, color=color, fontsize=8, family='monospace', verticalalignment='top')

    # 3. Edit Line
    ax.text(K_LABEL_X, K_EDIT_LINE_Y + 4, "> TR.X 4", color=COLOR_BRIGHT, fontsize=8, family='monospace', verticalalignment='top')
    rect = patches.Rectangle((K_LABEL_X + 50, K_EDIT_LINE_Y), 6, 7, linewidth=0, facecolor=COLOR_MEDIUM)
    ax.add_patch(rect)

    # 4. Draw I/O Grid
    draw_io_grid(ax)
    
    plt.title("Teletype Script View - [BUS] [GRID] [IN/PM]")
    plt.tight_layout()
    plt.savefig("mockup_teletype_final.png")
    print("Mockup saved to mockup_teletype_final.png")

if __name__ == "__main__":
    create_mockup()
