# Feature: Global Output Rotation (CV & Gate)

## Concept
Implement two system-wide "Rotation" parameters (one for CV, one for Gate) that shift the assignment of Tracks to Physical Outputs dynamically.

**Crucial Constraint:** The rotation must respect the "active set" or configuration defined in the Layout/Routing page. It should not naively rotate 1-8, but intelligently cycle through the relevant assignments.

## Implementation Logic: "The Active Pool"

Instead of rotating the *Physical Index* directly `(i + rot) % 8`, we act on the **Values** stored in the mapping array.

1.  **Identify Pool:** Collect all `Track Indices` currently assigned to physical outputs.
    *   *Example:* Output 1->Trk1, Output 2->Trk2, Output 3->Trk1 (Mult).
2.  **Rotate Values:** Apply the offset to the *Track Index* itself? No, that breaks specific assignments.

**Correct "Subset" Logic:**
The user likely wants to rotate the **Outputs**.
*   *Setup:* Out 1 (VCO A), Out 2 (VCO B), Out 3 (VCO C).
*   *Rotation 0:* 1->A, 2->B, 3->C.
*   *Rotation 1:* 1->C, 2->A, 3->B.

To achieve this without complex configuration, we can use a **Mask** or simply rotate the **Mapping Array** in memory (temporarily).

### Refined Plan

1.  **Parameters:**
    *   `Routable<int8_t> _cvOutputRotate`
    *   `Routable<int8_t> _gateOutputRotate`

2.  **Algorithm:**
    The rotation applies to the **Output Slots**.
    
    `TargetTrack[i] = BaseMapping[(i + Rotation) % 8]`
    
    *Constraint Handling:* If the user wants to restrict rotation to only Outputs 1-4, they simply leave Outputs 5-8 unassigned (or we implement a "Rotation Group Size" parameter, defaulting to 8).
    
    *Auto-Masking Idea:* We could scan the `_cvOutputTracks` array. If Outputs 5-8 are set to "None" or a specific disabled state, the modulo operator wraps at 4 instead of 8.
    *   *Logic:* Find the highest connected output `max_active_output`.
    *   `wrapped_index = (i + rotation) % (max_active_output + 1)`

## Revised Implementation Plan

### 1. Model (`Project.h`, `Routing.h`)
*   Add `_cvOutputRotate` and `_gateOutputRotate` (Routable).
*   Add `Routing::Target::CvOutputRotate` and `Routing::Target::GateOutputRotate`.

### 2. Engine (`Engine.cpp`)
*   **Pre-Calculation:** At the start of `update`, calculate the effective rotation offsets (Base + Modulation).
*   **Dispatch Loop:**
    ```cpp
    // Determine Rotation Modulo (Group Size)
    // Option A: Fixed 8 (Simple)
    // Option B: Smart (Count non-disabled outputs)
    
    int rotCV = _project.cvOutputRotate();
    int rotGate = _project.gateOutputRotate();
    
    for (int i = 0; i < 8; ++i) {
        // Apply rotation to the LOOKUP index
        int sourceIndexCV = (i + rotCV) % 8; 
        int sourceIndexGate = (i + rotGate) % 8;
        
        int trackID_CV = _project.cvOutputTrack(sourceIndexCV);
        int trackID_Gate = _project.gateOutputTrack(sourceIndexGate);
        
        // Send data
        driver.setCV(i, trackEngines[trackID_CV].cv());
        driver.setGate(i, trackEngines[trackID_Gate].gate());
    }
    ```

## Resource Cost
*   **CPU:** Still negligible (~32 integer ops).
*   **RAM:** 4 bytes.