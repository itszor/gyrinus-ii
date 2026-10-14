Gyrinus II - Son of Gyrinus (Tom Cooper, 1991; public domain) - static recompilation
=====================================================================================

Disc images: `Gyrinus II - Son of Gyrinus (19xx)(Tom Cooper)(Disk 1 of 2).adf`
(!Gyrinus2, !MrMemory, DoubleDisk) and disk 2 (!Gyrn2Mapz: level maps,
backgrounds, level music).

A recipe: this repository holds no game data. It rebuilds the game from
your own copy, recognised by content (recipe.json):

    ./build.sh "Gyrinus II - Son of Gyrinus (19xx)(Tom Cooper).zip"    (or the two .adf files)
    cd recomp && ./gyrinus2

build.sh runs recipe.json's steps with SuperSpider's tools/weave.py
(~/stuff/superspider, or $SUPERSPIDER; $SS below): it extracts the discs
to extracted/disc1 and disc2 and their applications to game/ (libroger's
appdir), detokenises the BASIC, then analyses, translates and links.
Needs python3, OCaml/dune, gcc and SDL3.

    $SS/tools/detok.py  BBC BASIC V detokeniser (also lists inline assembler)
    basic/              the BASIC programs, detokenised (make basic in recomp/)
    tools/images.py     the translated images; `analyse` runs SuperSpider
                        (~/stuff/superspider) on each, `gen` runs $SS/tools/gen_c.py
    analysis/           SuperSpider output (ss/), adapter output (*.json, out/),
    recomp/             make regen (analysis + gen_c.py), make, ./gyrinus2

Only the 2MB+ version is recompiled (!Run, Code.RunMe, Soundfx.Loadfx2Meg);
the 1MB variants (!Run1Meg, RunMe1Meg, Loadfx1Meg, the NoSound voices,
!MrMemory's switcher) are not used.

Running
-------

    cd recomp && make && ./gyrinus2 [--no-sound] [--scale=N] [--fullscreen]
    ./gyrinus2 --trace-gaps     interpret untranslated code instead of stopping (none found so far)
    ./gyrinus2 --save=DIR       where the zone state the game saves goes (default savedata/)
    ./gyrinus2 --limit=N        headless, deterministic: Space, Space, then walking right

Keys (from the disc's !Instruct): title page - Space start, Escape quit (to
the credits; Space there ends), S speaker on/off. In the game - Z left,
X right, Shift jump, Return fire, keypad Enter enter a door, P pause,
Escape back to the title. Going back to the title (and into the game)
shows "insert disk" prompts, which Space answers; the original does that
too even with both discs present (DoubleDisk/ReadMe). `--script` key
numbers are negated INKEY numbers: Space 99, Z 98, X 67, Shift 1,
Return 74, keypad Enter 61, Escape 113.

What runs
---------

!Run, in order, and what the recompilation does with each step:

- Code.Warnung, Code.Loading (BASIC): a warning picture for 5s (or a
  key or click) and the loading picture. Translated by hand into C in
  recomp/game.c (interactive runs only).
- MiscModuls.MemAlloc and its *ScreenSize, *RMAFree etc.: only resize
  memory areas; not run. The machine is set up as they leave it
  (screen memory 160K).
- Soundfx.Loadfx2Meg: *RMLoads 16 sample voice modules. Each is translated
  for a fixed RMA address (tools/images.py) and loaded at start-up. Each
  converts its sample to VIDC log format at initialisation (with
  Sound_SoundLog) and plays the whole sample on GateOn, whatever the note's
  duration. Loadfx2Meg then runs `Tuning 0`, `Tuning -&1000`,
  `Tuning -&2000`: *Tuning is relative, so the tuning ends three octaves
  below the default, which brings the effects (played at BBC pitch 50 or
  so) down to about 8 kHz. game.c runs the three commands; they don't
  affect the music, whose player sets its pitches itself.
  The game attaches voices by number, 1 to 16: Loadfx2Meg's first
  commands (`RmFaster SoundChannels` & co) replace the sound modules with
  fresh copies, emptying the voice table, so the 16 modules get slots 1-16
  (!Run ends with `RmReinit WaveSynth` etc. to bring the OS's own voices
  back). libroger has no built-in voices, so its slots are the same.
- Code.RunMe (BASIC): the title picture, `Gyr$TuneOn` / `Gyr$TuneCont`
  (title tune on, carried on into the game), VDU 21, *Run Code.MCode. The
  first part is C in game.c.
- Code.MCode: the game, translated (gen/mcode). An absolute file loaded at
  &7F800 and entered at &8BD08, its execution address (the extracted
  name's `,0007f800-0008bd08`). It
  runs * commands for its files and music: *Load/*Save at &9600,
  `If "<Gyr$TuneOn>" = "TuneOn" Then RmRun ...Musix.Level#`, *RMKill
  CoconizerSong.
- The music: CoconizerSong modules (Musik.TitleTrak, Musix.Level1-5), a
  Coconizer player plus a song each. The game RMRuns and RMKills them
  itself. Their code is identical, so one translation (gen/coco) is
  shared, all loaded at &1840000 (one at a time).
- Code.EndGame (BASIC): the credits until Space. C in game.c, run when
  MCode returns.

The BASIC programs share an RLE decoder each assembles with BASIC's inline
assembler (PROCassembl); game.c's decode() is that loop in C. Brandy
could list the files, but not run the assembler, and tools/detok.py
does the listing.

Known differences and open questions
------------------------------------

- Music speed: the Coconizer player's EventV handler doesn't look at the
  event number, so in the original any other enabled event (keypresses)
  advanced the tune too, and the music sped up while keys were pressed.
  game.c lets only the vsync event (4) reach it; libroger doesn't raise
  key events anyway.
- The hub zone and the first EASY zone draw on a plain blue background;
  not checked against the original.
- In headless runs libroger answers VDU variable 149 (DisplayBank's
  address) with the write bank, so both of MCode's screen pointers are the
  same there; interactive runs are right. Headless runs are for comparing
  builds, not for looking at.

libroger additions this game needed (all general): an entry point other
than the load address, system variables and GSTrans, an OS_CLI interpreter
(Set, If, Load, Save, RMLoad/RMRun/RMKill, FX), modules loaded and killed
on demand, OS_File save, reads preferring files the program saved, a
claim return address on the stack for vector claimants (the player's
EventV handler intercepts with `ldmia sp!, {..., pc}`), Sound_SoundLog
(without it every effect sample was converted to silence), Sound_Tuning
and *Tuning, note durations left to the voices as in RISC OS (they were
cutting the effects off after 5cs), VDU 21/6,
OS_SpriteOp 8 (MCode builds its sprite table from the sprite count), and
an `exited` hook for EndGame.
