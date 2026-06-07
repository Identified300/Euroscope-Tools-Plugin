# EuroScope Tools

A small suite of EuroScope controller tools sharing one on-screen panel. Build
produces **`EuroScopeTools.dll`** (load it in EuroScope under OTHER SET →
Plug-ins → Load).

## Tools

### CRR — Continuous Range Readout
A live distance number (rounded nm) next to selected aircraft, measured to a
fix. Colour is a property of the **fix**: assign aircraft to a fix and they show
their range in that fix's colour. Fixes are drawn on the scope as `* NAME`
(click a label to abbreviate to the first letter). On-scope text size is
adjustable.

```
.crr fix <NAME> [colour]    define/colour a fix (drawn as * NAME)
.crr add <CS> [FIX]         show CS's range to FIX (last fix if omitted)
.crr remove <CS>            stop showing an aircraft
.crr color <FIX> <colour>   recolour a fix
.crr abbrev <FIX>           full name / first letter on the scope
.crr delfix <NAME>          delete a fix and its aircraft
.crr font <size>            on-scope text size (8-28)
.crr list | clear | panel | collapse | help
```
Colours: white red orange yellow green cyan blue magenta grey, or `r,g,b`.

### PTL — Predicted Track Line
A bright green line from each aircraft showing where it will be in N minutes at
its current ground speed and track. Direction is taken from the aircraft's
position history (so it follows the real ground track regardless of map rotation
or magnetic variation); length is `groundspeed × minutes`.

```
.ptl on | off | toggle      show/hide the green vector line
.ptl mine                   only aircraft you are tracking
.ptl all                    all aircraft
.ptl len <0-5>              length in minutes, 0.5 steps
.ptl help
```

## Panel

Drag the header to move; the `-`/`+` button collapses it.

- Swatches — set the current colour (used for fixes).
- `Font` — on-scope text size: left-click +2, right-click −2.
- CRR fixes / aircraft — click a fix name to recolour, `x` to delete.
- PTL — `ON/OFF` toggle, `My trks / All a/c` scope toggle, and `Length`
  (left-click +0.5, right-click −0.5, clamped 0–5 min).

