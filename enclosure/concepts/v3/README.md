# Enclosure v3 concept (looks only)

`flashbee_case_v3_concept.scad` models the outside only: no internals, not printable. It shows how to bring back the look of gokux's original case. The working case is still `enclosure/flashbee_case.scad` (v2).

Renders: `v3_front.png` and `v3_side.png` (the concept), `v2_front.png` and `v2_side.png` (the current case). Render with
`openscad --imgsize=900,900 --colorscheme=Tomorrow --autocenter --viewall --camera=0,0,0,25,0,15,0 -o out.png flashbee_case_v3_concept.scad`
(side view: `--camera=0,0,0,70,0,55,0`).

## Why v2 looks plain next to the original

- Outline: a rounded rectangle. The original has angled corners, a waist below the display and a narrower chin.
- Size: 58 x 84.5 x 30 mm. The original is 51.5 x 86 x about 26 mm.
- The face is flat, with 0.6 mm engraving. The original has 3 to 4 raised and recessed levels, which give it shadows.
- One colour. The original has a yellow front cover, a black body and a black keyhole frame that links the display to the chin panel.
- The three side grooves draw attention to the 30 mm thickness.

## Changes in the concept

1. No side grooves and no engraved art.
2. The back edges are chamfered 4 mm, so the case looks thinner.
3. The outline has angled corners, a waist and a narrower chin.
4. Raised parts in place of engraving: a black bezel 1.4 mm proud with a bevelled inner edge, a black keyhole frame, a raised yellow chin panel and raised grip bars. Print the face in two colours on the XL (5 toolheads). On the Core One+, make the frame a separate part that is pressed in.
5. The AS3935 is in an antenna stub on the top edge, behind and below the USB-C opening. This moves the coil away from the C3 and the LCD, which may also reduce disturbers. The 20 cm Grove cable reaches.
6. The case splits like the original: a shallow yellow front cover (about 9 mm) and a deep black body that holds the sensor and the battery.

## Problems to solve before building it

- The angled shoulders collide with the sensor in its v2 position. At the top corners the inner wall is at about x 22.6, but the board reaches x 26.25. Changes 3 and 5 must be done together.
- The new split line means the screw bosses and the lever post must move.
- The stub adds about 15 mm to the height.
- Known rough spot: where the frame neck meets the bezel ring, slivers of yellow show. The grip bars could also be bigger.
- The v2 hardware assumptions are still not checked (lever fork, sniffer outline, sensor board size). Print a fit test first.

## Two ways forward

- A. Restyle v2 and keep its internals: changes 1, 2 and 4, plus a softer outline that still clears the sensor.
- B. Build v3 from this concept, with all six changes.
