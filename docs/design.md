# Design principles

CUI's look is deliberate. These are the rules every component follows, and they are a good guide for the screens you build with it.

## Color

- Most of the screen is neutral. The accent is spent with intent: the selected item, the one data series that matters, progress, focus.
- Status colors mean status. Green for success, amber for attention, red for failure, always with a label or an icon, never as decoration.
- Use semantic tokens, never raw colors, so every screen works in light and dark with any accent.

## Type

- Inter for interface text, Geist for large headings and numbers.
- Two weights only: regular and medium. Hierarchy comes from size, color and spacing.
- Sentence case for every label. Numbers that change or align use tabular figures.

## Surfaces

- Group by proximity first, a hairline border second, a card only for a real boundary.
- Cards rest on a border with no shadow. Only floating layers cast shadows: menus, popovers, dialogs, toasts.
- Corners are concentric: an inner radius is the outer radius minus the padding between them. Controls are soft rectangles (10 pt), panels 14 pt, surfaces 18 pt, so a menu item sits 6 pt inside its panel at 8 pt. Fully round shapes are kept for things that are round by nature: switches, slider thumbs, avatars, chips, progress bars.
- Two kinds of edge. A field, checkbox or radio is marked only by its edge, so that edge is 3:1 against the surface. A button, chip or button group is already marked by its label, so its edge is a quiet hairline that firms up under the pointer.
- Solid buttons are flat color with a faint lit upper rim. No gloss and no drop shadow: they sit in the page.
- In dark mode, depth comes from lightness: the page is the darkest level and every layer above it is a step lighter. Floating layers add a hairline edge and a close, tight shadow rather than a wide haze.

## Tokens

| Token | Value | Used for |
|---|---|---|
| `radius_control` | 10 | buttons, fields, triggers, segmented tracks |
| `radius_panel` | 14 | menus, popovers, nested panels (items 8) |
| `radius_surface` | 18 | cards, dialogs, sheets |
| `control_sm / md / lg` | 30 / 36 / 44 | control heights |
| button padding | 11 / 14 / 18 | horizontal, by size |
| field padding | 12 (10 beside an icon) | horizontal |
| `BORDER` / `BORDER_STRONG` | OKLab L 0.915 / 0.665 light, 0.29 / 0.515 dark | hairlines / 3:1 control edges |
| dark levels | 0.18 page, 0.21 surface, 0.24 raised | depth by lightness |
| floating shadow | 12/32 at 12%, 1/3 at 6% (light) | menus, popovers, dialogs, toasts |

## Spacing

- A 4 pt grid. 16 pt inside groups, 24 pt between regions, 32 to 48 pt between sections.
- Controls are 30, 36 or 44 pt tall.

## Motion

- Motion explains cause and effect: what opened, what changed, where it came from.
- Routine feedback never bounces or overshoots. Presses scale to 0.98; triggers that anchor a menu answer with color. Hover changes color and opacity, without lifting cards.
- Input and content changes take effect immediately. Only the visual feedback transitions; tab content never waits for an indicator. Keyboard focus, keyboard selection and the command palette appear immediately.
- Reuse the motion tokens in `src/cui_internal.h`:

| Job | Duration at normal motion speed |
|---|---|
| Row feedback | 100 ms |
| Hover background and edges | 120 ms |
| Focus | immediate for keyboard focus; 100 ms color feedback elsewhere |
| Button press and release | 100 ms |
| Checkbox, radio, switch and selection fills | 150 ms |
| Icon changes | 150 ms |
| Dropdowns, selects and context menus | 150 ms |
| Popovers and hover cards | 180 ms |
| Tooltips | 120 ms; warm neighbours appear immediately |
| Tab and navigation indicators | 150 ms; immediate from the keyboard |
| Modal opening / closing | 220 / 150 ms |
| Sidebars and sheets opening | 220 ms |
| Toast opening / closing | 200 / 120 ms |
| Expand and collapse | 200 ms |
| Large panels and gesture settling | 280 ms |

Timed transitions use these cubic Bezier curves, solving the time coordinate as
well as the value:

| Purpose | Curve |
|---|---|
| Enter, exit, hover and press feedback | `(0.2, 0.8, 0.2, 1)` |
| Movement and state | `(0.4, 0, 0.2, 1)` |

- Animation decisions use current input events, separately from the visible keyboard focus ring. Hover and mouse-triggered panels keep their transitions after keyboard use; keyboard-triggered changes remain immediate.
- Timed transitions finish at their duration and retarget from the current value without queuing. Springs keep velocity for gestures, reordering and interactive motion. Exponential tweens smooth continuously changing values and color feedback in existing components.
- Floating layers use opacity and a small transform. Popovers travel 6 pt from 98% scale; dialogs fade from 98% scale without a vertical slide. Sheets slide over content rather than resizing it on every frame.
- Dragged controls, split dividers and scroll positions track input directly. Navigation has no added page animation, so scrolling and component state stay under the application's control.
- Disabled is one opacity everywhere (45%).
- Reduced motion snaps transitions to the final state, preserving feedback. `theme.motion` scales durations, including slow motion for inspection.
- Animation steps resume with one 60 Hz step after a gap longer than 100 ms. Tooltip dwell delays, toast lifetimes and deliberate holds keep the full elapsed time.
- Specialty effects such as typewriter text, rolling numbers and gauge needles retain their own timing; the ordinary interaction budgets do not shorten a deliberate hold or a playback clock.

## Responsive

- Every screen works from a 360 point phone to a wide monitor. Layout adapts before anything gets smaller: fill up to a comfortable width, wrap side by side parts into a stack, then change structure (a drawer for the sidebar, one pane at a time).
- Never truncate what someone has to read to act: tabs and wide tables scroll sideways instead, and a banner's message wraps while its action keeps its label.
- Margins tighten on small screens (16 pt instead of 24 to 40); type and control sizes do not.

## Interaction

- Every action works with the keyboard. Focus rings appear for keyboard focus, never for clicks. A clicked control keeps the focus, so the keyboard carries on from there: the first key shows the ring and acts.
- Give icon only buttons a tooltip: it is their name.
- Do not move layout under the user. Loading states keep their size; numbers keep their width.
- Prefer changing a control in place ("Saved") to a toast for the result of a button press. Toasts are for background events.

## What generic looks like

A lot of interface work now comes out of the same few templates, and people have learned to spot it. Almost none of the tells below is wrong in isolation. They read as generic because they are defaults nobody chose. CUI's components and examples stay clear of them, and so should screens built with it.

**Color and light**

- No purple to blue or purple to pink gradients, no cyan on near black, no neon. The palette comes from the accent you pick, and the accent is used sparingly.
- No glow halos, colored blur blobs or spotlights behind content to make a screen feel atmospheric. Depth comes from surface levels and contrast. A glow is allowed only when it reports something: the voice orb's level, a pointer the user is moving.
- Headings and numbers are one solid color. Emphasis comes from size and weight, not gradient text. Gradient text exists as a component for the rare brand moment; it is never the default.
- Text on a colored surface is tinted toward that surface, never neutral gray on color.

**Surfaces and layout**

- No decorative grid or dot backgrounds. Grids are for canvases, maps and measuring.
- No cards inside cards. Inside a card, group with spacing, type and hairlines.
- An edge or a shadow, not both, for anything resting on the page. Floating layers (menus, popovers, dialogs) are the exception, because they sit over other content.
- No rows of identical icon tile, heading and blurb cards. If content is a list, it looks like a list.
- Left aligned by default. Center only what is truly alone: an empty state, a dialog's single message.
- Spacing has rhythm: tight inside a group, clearly looser between groups.

**Type and icons**

- No pill badge or eyebrow label above a heading to restate it. If a badge carries information, it sits next to the thing it describes.
- No icon in a rounded tile above every heading. An icon earns its place by helping someone find or recognize something.
- Sparkles mean nothing. CUI does not use them as a default icon for anything, and the AI components do not need them to say "AI".
- Monospace is for code and aligned technical data, not for looking technical.
- No emoji in place of icons.

**Status and motion**

- A dot means a state: online, unread, failed. No dots as decoration on tags or labels.
- Motion reports cause and effect. No pulsing on things that are not changing, no blinking caret on text nobody is typing, no bounce on routine dialogs.
- Marquees pause under the pointer and stop for reduced motion.

**Words and content**

- Say what something does. No "supercharge", "seamless", "world class", no slogans built from short fragments ("Fast. Small. Native."), no "Not X. Y." contrasts, no em dashes doing the work of a full stop.
- Button labels are a verb and an object: "Create project", not "Submit".
- Example content is specific and plausible. Real-looking names that are not famous people, prices and numbers that are not round templates, products with an actual job. Placeholder pioneers (Ada Lovelace, Grace Hopper) and template dashboards ("Revenue $15,231 +20.1%") are as generic as "John Doe".

**Charts and numbers**

- No gradient fading out under a line. A sparkline is a line and the point where it ends; an area chart is a flat tint, because the area is a quantity.
- A change always names its baseline, in the delta itself ("+2 this week") or in the title it sits under ("Median build, 30 days"). A bare "+12.5%" says nothing.
- Its color says good or bad, not up or down: fewer flaky tests is green.

**Coherence**

- One value per axis, used everywhere: one radius scale, one light direction, one accent, one icon set, one spacing scale, one set of motion curves. A mix of personalities on one screen reads as assembled rather than designed.
- Every rule above is a decision, not only a ban. Avoiding the tells and putting nothing in their place produces its own generic look; CUI's positive choices are the token tables in this document.
- Icons come in even sizes that match the text beside them: 12 with small text, 14 in small controls, 16 in rows and default controls, 18 in large ones. Odd sizes sit off the pixel grid and their strokes blur.
