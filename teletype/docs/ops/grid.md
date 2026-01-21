## Grid
Grid operators allow creating scenes that can interact with grid connected to
teletype (important: grid must be powered externally, do not connect it directly
to teletype!). You can light up individual LEDs, draw shapes and create controls
(such as buttons and faders) that can be used to trigger and control scripts.
You can take advantage of grid operators even without an actual grid by using
the built in Grid Visualizer.

For more information on grid integration see Advanced section and 
[Grid Studies](https://github.com/scanner-darkly/teletype/wiki/GRID-INTEGRATION).

As there are many operators let's review some naming conventions that apply to
the majority of them. All grid ops start with `G.`. For control related ops this
is followed by 3 letters specifying the control: `G.BTN` for buttons, `G.FDR`
for faders. To define a control you use the main ops `G.BTN` and `G.FDR`. To
define multiple controls replace the last letter with `X`: `G.BTX`, `G.FDX`. 

All ops that initialize controls use the same list of parameters: id,
coordinates, width, height, type, level, script. When creating multiple controls
there are two extra parameters: the number of columns and the number of rows.
Controls are created in the current group (set with `G.GRP`). To specify a
different group use the group versions of the 4 above ops - `G.GBT`, `G.GFD`,
`G.GBX`, `G.GFX` and specify the desired group as the first parameter.

All controls share some common properties, referenced by adding a `.` and:

* `EN`: `G.BTN.EN`, `G.FDR.EN` - enables or disables a control
* `V`: `G.BTN.V`, `G.FDR.V` - value, 1/0 for buttons, range value for faders
* `L`: `G.BTN.L`, `G.FDR.L` - level (brightness level for buttons and coarse faders, max value level for fine faders)
* `X`: `G.BTN.X`, `G.FDR.X` - the X coordinate
* `Y`: `G.BTN.Y`, `G.FDR.Y` - the Y coordinate

To get/set properties for individual controls you normally specify the control
id as the first parameter: `G.FDR.V 5` will return the value of fader 5. Quite
often the actual id is not important, you just want to work with the latest
control pressed. As these are likely the ops to be used most often they are
offered as shortcuts without a `.`: `G.BTNV` returns the value of the last
button pressed, `G.FDRL 4` will set the level of the last fader pressed etc etc.
