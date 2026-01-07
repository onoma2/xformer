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
GRID_X = 222  # Right-aligned: 256 - 2 (margin) - 32 (width) = 222
GRID_Y = 15   # Below header: 6 (y) + 8 (height) + 1 (spacing) = 15
COL_WIDTH = 8
ROW_HEIGHT_TR = 8
ROW_HEIGHT_CV = 16
COLS = 4

# Mock Data State
ti_states = [0, 1, 2, 1]  # 0:None, 1:Inactive, 2:Active
to_states = [1, 2, 1, 1]
cv_values = [0.0, 0.7, 1.0, 0.5] # Min, Above Center, Max, Center

# Mock Layout Ownership (True=Owned by this track, False=Owned by others)
to_ownership = [True, True, False, True] # TO 2 is mismatched
cv_ownership = [True, True, False, True] # CV 2 is mismatched

# Mock Script Content
script_lines = [
    "TR.X 1",             # Short line
    "CV 1 V 5",           # Short line
    "IF TR.X 1: TR.X 0",  # Medium line
    "RP 4: TR.X T",       # Medium line
    "// This is a comment", # Comment
    "L 1 0; L 2 12"       # Long line (potential collision?)
]

def draw_io_grid(ax):
    # Row 1: TI (Trigger Inputs)
    for col in range(COLS):
        x = GRID_X + col * COL_WIDTH
        y = GRID_Y
        state = ti_states[col]
        
        color = COLOR_LOW
        if state == 1: color = COLOR_MEDIUM
        elif state == 2: color = COLOR_MEDIUM_BRIGHT
            
        # x+1, y+1, 6x6
        rect = patches.Rectangle((x + 1, y + 1), 6, 6, linewidth=0, facecolor=color)
        ax.add_patch(rect)

    # Row 2: TO (Trigger Outputs)
    for col in range(COLS):
        x = GRID_X + col * COL_WIDTH
        y = GRID_Y + ROW_HEIGHT_TR
        state = to_states[col]
        owned = to_ownership[col]
        
        color = COLOR_LOW
        if owned:
            if state == 1: color = COLOR_MEDIUM
            elif state == 2: color = COLOR_MEDIUM_BRIGHT # Signal active
        else:
            color = COLOR_LOW # Mismatch
            
        rect = patches.Rectangle((x + 1, y + 1), 6, 6, linewidth=0, facecolor=color)
        ax.add_patch(rect)

    # Row 3+4: CV (CV Outputs) - BIPOLAR LOGIC
    for col in range(COLS):
        x = GRID_X + col * COL_WIDTH
        y = GRID_Y + ROW_HEIGHT_TR * 2
        val = cv_values[col] # 0.0 to 1.0
        owned = cv_ownership[col]
        
        # Container (6x14 total, Inner 4x12)
        # Top border y+1, Bottom border y+14. Inner y+2 to y+13.
        rect = patches.Rectangle((x + 1, y + 1), 6, 14, linewidth=1, edgecolor=COLOR_LOW, facecolor='none')
        ax.add_patch(rect)
        
        if owned:
            # Inner Top: y+2
            # Inner Center: y+2 + 6 = y+8
            center_y = y + 8
            
            # Max swing 5 pixels to stay safely inside 12px height
            h = int(abs(val - 0.5) * 2 * 5) # Map 0-0.5 to 0-5
            h = max(0, min(5, h))
            
            # Always draw center line (1px) at center_y
            ax.add_patch(patches.Rectangle((x + 2, center_y), 4, 1, linewidth=0, facecolor=COLOR_MEDIUM_BRIGHT))

            if h > 0:
                if val >= 0.5:
                    # Positive: Grow UP from center_y (exclusive)
                    # Rect starts at center_y - h
                    rect = patches.Rectangle((x + 2, center_y - h), 4, h, linewidth=0, facecolor=COLOR_MEDIUM_BRIGHT)
                else:
                    # Negative: Grow DOWN from center_y + 1
                    rect = patches.Rectangle((x + 2, center_y + 1), 4, h, linewidth=0, facecolor=COLOR_MEDIUM_BRIGHT)
                ax.add_patch(rect)

def create_mockup():
    fig, ax = plt.figure(figsize=(12, 3), dpi=150), plt.gca()
    ax.set_facecolor(COLOR_BG)
    ax.set_xlim(0, WIDTH)
    ax.set_ylim(HEIGHT, 0) # Inverted Y
    ax.set_aspect('equal')
    
    # Screen Border
    rect = patches.Rectangle((0, 0), WIDTH, HEIGHT, linewidth=1, edgecolor='#333333', facecolor='none')
    ax.add_patch(rect)
    
    # -------------------
    # DRAW UI
    # -------------------
    
    # 1. Header (Icons & Pattern/Script Labels)
    # Icons Left
    ax.text(K_LABEL_X, 6+4, "M S D St", color=COLOR_MEDIUM, fontsize=8, family='monospace', verticalalignment='center')
    
    # Right Label: Simulate C++ calculation
    script_label = "S1"
    slot_label = "P1"
    
    script_width = len(script_label) * 6
    script_x = WIDTH - 2 - script_width
    
    slot_width = len(slot_label) * 6
    slot_x = script_x - slot_width - 4
    
    ax.text(slot_x, 6+4, slot_label, color=COLOR_MEDIUM, fontsize=8, family='monospace', verticalalignment='center')
    ax.text(script_x, 6+4, script_label, color=COLOR_MEDIUM, fontsize=8, family='monospace', verticalalignment='center')

    # 2. Script Lines
    for i in range(K_LINE_COUNT):
        y = K_ROW_START_Y + i * K_ROW_STEP_Y
        
        # Line Number
        ax.text(K_LABEL_X, y + 4, str(i + 1), color=COLOR_MEDIUM, fontsize=8, family='monospace', verticalalignment='top')
        
        # Script Text
        text = script_lines[i]
        color = COLOR_BRIGHT if i == 2 else COLOR_MEDIUM # Highlight line 3
        if text.startswith("//"): color = COLOR_LOW
        
        ax.text(K_TEXT_X, y + 4, text, color=color, fontsize=8, family='monospace', verticalalignment='top')

    # 3. Edit Line
    ax.text(K_LABEL_X, K_EDIT_LINE_Y + 4, "> TR.X 4", color=COLOR_BRIGHT, fontsize=8, family='monospace', verticalalignment='top')
    # Cursor
    cursor_x = K_LABEL_X + 50 # Approx
    rect = patches.Rectangle((cursor_x, K_EDIT_LINE_Y), 6, 7, linewidth=0, facecolor=COLOR_MEDIUM)
    ax.add_patch(rect)

    # 4. Draw I/O Grid
    draw_io_grid(ax)
    
    plt.title("Teletype Script View - Full Mockup (Bipolar CV)")
    plt.tight_layout()
    plt.savefig("mockup_teletype_full.png")
    print("Mockup saved to mockup_teletype_full.png")

if __name__ == "__main__":
    create_mockup()