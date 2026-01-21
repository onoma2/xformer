## Patterns
Patterns facilitate musical data manipulation– lists of numbers that can be used as sequences, chord sets, rhythms, or whatever you choose. Pattern memory consists four banks of 64 steps. Functions are provided for a variety of pattern creation, transformation, and playback.

New in teletype 2.0, a second version of all Pattern ops have been added. The original `P` ops (`P`, `P.L`, `P.NEXT`, etc.) act upon the ‘working pattern’ as defined by `P.N`. By default the working pattern is assigned to pattern 0 (`P.N 0`), in order to execute a command on pattern 1 using `P` ops you would need to first reassign the working pattern to pattern 1 (`P.N 1`). 

The new set of ops, `PN` (`PN`, `PN.L`, `PN.NEXT`, etc.), include a variable to designate the pattern number they act upon, and don’t effect the pattern assignment of the ‘working pattern’ (ex: `PN.NEXT 2` would increment pattern 2 one index and return the value at the new index). For simplicity throughout this introduction we will only refer to the `P` ops, but keep in mind that they now each have a `PN` counterpart (all of which are detailed below)

Both patterns and their arrays of numbers are indexed from 0. This makes the first pattern number 0, and the first value of a pattern is index 0. The pattern index (`P.I`) functions like a playhead which can be moved throughout the pattern and/or read using ops: `P`, `P.I`, `P.HERE`, `P.NEXT`, and `P.PREV`. You can contain pattern movements to ranges of a pattern and define wrapping behavior using ops: `P.START`, `P.END`, `P.L`, and `P.WRAP`.

Values can be edited, added, and retrieved from the command line using ops: `P`, `P.INS`, `P.RM`, `P.PUSH`, `P.HERE`, `P.NEXT`, and `P.PREV`. Some of these ops will additionally impact the pattern length upon their execution: `P.INS`, `P.RM`, `P.PUSH`, and `P.POP`.

To see your current pattern data use the `<tab>` key to cycle through live mode, edit mode, and pattern mode. In pattern mode each of the 4 patterns is represented as a column. You can use the arrow keys to navigate throughout the 4 patterns and their 64 values. For reference a key of numbers runs the down the lefthand side of the screen in pattern mode displaying 0-63.
 
From a blank set of patterns you can enter data by typing into the first cell in a column. Once you hit `<enter>` you will move to the cell below and the pattern length will become one step long. You can continue this process to write out a pattern of desired length. The step you are editing is always the brightest. As you add steps to a pattern by editing the value and hitting `<enter>` they become brighter than the unused cells. This provides a visual indication of the pattern length.

The start and end points of a pattern are represented by the dotted line next to the column, and the highlighted dot in this line indicates the current pattern index for each of the patterns. See the key bindings for an extensive list of editing shortcuts available within pattern mode.
