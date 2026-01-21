# Advanced

## Teletype terminology

Here is a picture to help understand the naming of the various parts of a Teletype command:

![Teletype command terminology](img/terminology.jpg)

`COMMAND`

: The entire command, e.g. `IF X: Y 1; Z 2;`.

`PRE`

: The (optional)  part before the _`PRE SEP`_, e.g. `IF X`.

`POST`

: The part after the _PRE SEP_, e.g. `Y 1; Z 2`.

`SUB`

: A sub command (only allowed in the _`POST`_), e.g. `Y 1`, or `Z 2`.

`PRE SEP`

: A _colon_, only one is allowed.

`SUB SEP`

: A _semi-colon_, that separates sub commands (if used), only allowed in the _`POST`_.

`NUM`

: A number between `−32768` and `32767`.

`OP`

: An _operator_, e.g. `X`, `TR.PULSE`

`MOD`

: A _modifier_, e.g. `IF`, or `L`.

## Sub commands

Sub commands allow you to run multiple commands on a single line by utilising a semi-colon to separate each command, for example the following script:

```
X 0
Y 1
Z 2
```

Can be rewritten using sub commands as:

```
X 0; Y 1; Z 2
```

On their own sub commands allow for an increased command density on the Teletype. However when combined with `PRE` statements, certain operations become a lot easier.

Firstly, sub commands cannot be used before a `MOD` or in the `PRE` itself. For example, the following is **not allowed**:

```
X 1; IF X: TR.PULSE 1
```

We can use them in the `POST` though, particularly with an `IF`, for example:

```
IF X: CV 1 N 60; TR.P 1
IF Y: TR.P 1; TR.P 2; TR.P 3
```

Sub commands can also be used with `L`.

## Aliases

In general, aliases are a simple concept to understand. Certain `OP`s have been given shorted names to save space and the amount of typing, for example:

```
TR.PULSE 1
```

Can be replaced with:

```
TR.P 1
```

Where confusion may arise is with the symbolic aliases that have been given to some of the maths `OP`s. For instance `+` is given as an alias for `ADD` and it _must_ be used as a direct replacement:

```
X ADD 1 1
X + 1 1
```

The key to understanding this is that the Teletype uses _prefix notation_[^polish] always, even when using mathematical symbols.

[^polish]: Also know as _Polish notation_.

The following example (using _infix notation_) **will not work**:

```
X 1 + 1
```

Aliases are entirely optional, most `OP`s do not have aliases. Consult the `OP` tables and documentation to find them.

## Avoiding non-determinism

Although happy accidents in the modular world are one of it's many joys, when writing computer programs they can be incredibly frustrating. Here are some small tips to help keep things predictable (when you want them to be):

  1. **Don't use variables unless you need to.**
  
     This is not to say that variables are not useful, rather it's the opposite and they are extremely powerful. But it can be hard to keep a track of what each variable is used for and on which script it is used. Rather, try to save using variables for when you do want non-deterministic (i.e. _variable_) behaviour.
     
  2. **Consider using `I` as a temporary variable.**
  
     If you do find yourself needing a variable, particularly one that is used to continue a calculation on another line, consider using the variable `I`. Unlike the other variables, `I` is overwritten whenever `L` is used, and as such, is implicitly transient in nature. One should never need to worry about modifying the value of `I` and causing another script to malfunction, as no script should ever assume the value of `I`.
     
  3. **Use `PN` versions of `OP`s.**
  
     Most `P` `OP`s are now available as `PN` versions that ignore the value of `P.I`. (e.g. `PN.START` for `P.START`). Unless you explicitly require the non-determinism of `P` versions, stick to the `PN` versions (space allowing).
  
  4. **Avoid using `A`, `B`, `C` and `D` to refer to the trigger outputs, instead use the numerical values directly.**
   
     As `A-D` are variables, they may no longer contain the values `1-4`, and while this was the recommend way to name triggers, it is no longer consider ideal. Newer versions of the Teletype hardware have replaced the labels on the trigger outputs, with the numbers `1` to `4`.

## Grid integration

Grid integration can be described very simply: it allows you to use grid with
teletype. However, there is more to it than just that. You can create custom
grid interfaces that can be tailored individually for each scene. Since it's
done with scripts you can dynamically change these interfaces at any point -
you could even create a dynamic interface that reacts to the scene itself or
incoming triggers or control voltages.

You can simply use grid as an LED display to visualize your scene. Or make it
into an earthsea style keyboard. You can create sequencers, or control surfaces
to control other sequencers. The grid operators simplify building very complex
interfaces, while something simple like a bank of faders can be done with just
two lines of scripts.

Grid integration consists of 3 main features: grid operators, Grid Visualizer,
and Grid Control mode. Grid operators allow you to draw on grid or create grid
controls, such as buttons and faders, that can trigger scripts when pressed. 
As with any other operators you can execute them in Live screen or use them in
any of your scripts.

Grid Visualizer provides a virtual grid within the Teletype itself:

![Grid Visualizer](img/gridvisualizer.jpg)

It can be very useful while developing a script as you don't have to switch
between the grid and the keyboard as often. To turn it on navigate to Live
screen and press `Alt-G` (press again to switch to Full View / turn it off).
You can also emulate button presses, which means it can even be used as an 
alternative to grid if you don't have one, especially in full mode - try it
with one of the many [grid scenes](https://github.com/scanner-darkly/teletype/wiki/CODE-EXCHANGE)
already developed. For more information on how to use it please refer to 
[the Grid Visualizer documentation](https://github.com/scanner-darkly/teletype/wiki/GRID-VISUALIZER).

Grid Control Mode is a built in grid interface that allows you to use grid to
trigger and mute scripts, edit variables and tracker values, save and load
scenes, and more. It's available in addition to whatever grid interface you
develop - simply press the front panel button while the grid is attached. It can
serve as a simple way to use grid to control any scene even without using grid
ops, but it can also be very helpful when used together with a scripted grid
interface. For more information and diagrams please refer to 
[the Grid Control documentation](https://github.com/scanner-darkly/teletype/wiki/GRID-CONTROL-MODE),

If you do want to try and build your own grid interfaces 
[the Grid Studies](https://github.com/scanner-darkly/teletype/wiki/GRID-INTEGRATION)
is the best place to start.

