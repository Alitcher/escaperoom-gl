# Level Format (JSON)

Every escape room is **one JSON file** - a "creation instance." The engine loads
it into a `World` and plays it; the editor writes it back out. Puzzle logic is
**data, not code**: entities are linked by `id`.

Example: `levels/sample-room.json`.

---

## Top level
| Field | Type | Meaning |
|---|---|---|
| `name` | string | level title |
| `author` | string | who made it |
| `spawn` | `{ position:[x,y,z], yaw:number }` | where/which way the player starts |
| `lights` | `Light[]` | scene lights |
| `items` | `Item[]` (optional) | display name + icon for inventory items |
| `entities` | `Entity[]` | every object in the room |
| `win` | `{ exit: id, message?: string }` | using this exit (once unlocked) = you escaped; `message` is shown on the win screen |

## Light
| Field | Type | Notes |
|---|---|---|
| `type` | `"point"` \| `"directional"` | |
| `position` | `[x,y,z]` | for point lights |
| `direction` | `[x,y,z]` | for directional lights |
| `color` | `[r,g,b]` | 0-1 |
| `intensity` | number | |

## Item
Optional. Items work without an entry here; this only changes how they look.
| Field | Type | Notes |
|---|---|---|
| `id` | string | the item id used in `pickup.item`, `contains`, `unlockedBy`, `gives` |
| `name` | string | shown in the UI; defaults to `id` |
| `icon` | string | PNG/JPG path relative to the level file (e.g. `icons/key.png`); without it the item shows as a colored tile with its letters |

## Entity
| Field | Type | Notes |
|---|---|---|
| `id` | string | **unique**; other entities reference it |
| `model` | string | asset file (e.g. `drawer.glb`); omit for invisible logic-only entities |
| `parent` | id (optional) | makes this a child of another entity: its `transform` is then relative to the parent, and it moves with the parent. Things hidden inside a container are usually its children |
| `transform` | `{ position:[x,y,z], rotation?:[x,y,z], scale?:[x,y,z] }` | rotation = Euler degrees, matrix = Rx * Ry * Rz (same as the editor's Inspector); scale defaults to `[1,1,1]` |
| `interaction` | `Interaction` (optional) | what happens when the player clicks it |

## Interaction types
The runtime has one handler per `type`. Add a new puzzle = add a new handler; the
JSON stays the source of truth.

| `type` | Fields | Behaviour |
|---|---|---|
| `pickup` | `item: string` | click -> add `item` to inventory; entity hides |
| `container` | `locked: bool`, `unlockedBy?: item`, `contains?: item` | click -> if locked, the player must be **holding** `unlockedBy` (or a keypad must have opened it); then it opens and gives `contains` |
| `readable` | `text: string` | click -> show `text` (a clue/code) |
| `keypad` | `code: string` or `codeFrom: id`, `opens: id` | player enters digits; correct code -> unlock the `opens` entity (an exit or a container). With `codeFrom`, the code is the **total of the last roll** of that dice entity |
| `exit` | `unlockedBy?: item` | the door; starts locked; a keypad or holding `unlockedBy` unlocks it; using it unlocked -> `win` (if it is `win.exit`) |
| `npc` | `name?: string`, `line: string` or `lines: string[]`, `gives?: item` | click -> shows one line of speech; each click shows the next line, then it starts over. `gives` is handed over on the first talk |
| `dice` | `count?: number` (default 3), `sides?: number` (default 6) | click -> rolls all dice at once and shows the faces and total; can roll again. A keypad with `codeFrom` uses that total |

**Inventory** is a list of item ids (strings). `contains`, `pickup.item` and
`npc.gives` add to it. To use an item, the player **holds** it (keys 1-9 or
click it in the inventory) and then clicks the thing. `unlockedBy` checks the
held item, not just the inventory.

**What `contains` means:** if an entity with that id exists, it starts hidden
"inside" the container. Opening the container gives it to the player: a
`pickup` adds its `item`, a `readable` is shown at once and kept in the
inventory so it can be read again. Any other entity type is revealed in the
room. If no entity has that id, `contains` is a plain item id that goes
straight to the inventory.

---

## How a puzzle chains (the sample)
```
mika (npc) -> hints: "the drawer lock wants the total of the three dice"
dice (dice, 3 x d6) -> roll, total e.g. 11
drawer-lock (keypad, codeFrom "dice", opens "drawer")
drawer (container, locked, contains "key") -> key goes to inventory
hold key (1-9) -> cabinet (container, locked, unlockedBy "key", contains "note")
note (readable, "Stage door code: 4827")
keypad (code "4827", opens "door")
door (exit) -> win
```

## Versioning
Add a `"version": 1` field when the schema first ships, so old levels can be
migrated as the format grows. Keep changes backward-compatible where possible.
