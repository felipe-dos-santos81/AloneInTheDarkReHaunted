# Music generation guide

What to generate for the external soundtrack of Alone in the Dark 1, piece
by piece: how many, how long, what each one scores, and which in-game text
to feed the generator. The generation itself is another project; this is
its brief. The player side is `docs/superpowers/specs/2026-10-10-external-music-design.md`
and the `music.*` keys in `docs/configuration.md`.

## What the engine plays

- One file per song number in `music.folder` (default `music`, next to the
  game data): `NN.ogg`, `.flac`, `.mp3` or `.wav`, `NN` two digits.
- `NN` is the number the game's own scripts pass to `MUSIC`. Our data is the
  CD release, whose scripts call songs 2 to 17; there, song `n` was CD audio
  track `n`. Songs 0, 1 and 6 are never called: **do not produce them.**
  A floppy release has its own scripts and only the eight chip tunes;
  with that data, redo the scan below before trusting this table.
- A song plays once and never loops. A script's `NEXT_MUSIC` queues what
  plays when the current song ends; a scripted fade is 3 s. So a piece
  should run about as long as the original; when a script cuts it short
  (a fight ends, a cinematic ends) the cut is the engine's and needs no
  tail work.
- Mono or stereo, any sample rate SoLoud reads. Loudness: sound effects are
  not normalised, so master the set to one level and let `music.volume`
  do the rest.

## The 15 pieces the game calls

Lengths are the CD track lengths from the disc's cue sheet (`GAME.INS`);
the chip tune length is given where a chip version exists (`00.wav` to
`07.wav` in `data/aitd1/music`, made by m-aitd's `make convert-music`).
Target length is the CD length within about 10 percent. Total: about 24
minutes of music.

| # | Plays when | CD length | Chip | Voice | Text to prompt with |
|---|---|---|---|---|---|
| 02 | The epilogue's last scene and credits: the house behind, the car, the driver's reveal. Follows 09. | 1:20 | 1:03 | **Lyrics** | Jeremy's note, `doc01.txt`: "They will find my body but will not have my soul." Closing on "Some may understand what I have done. May God forgive me. Farewell." Sung as a farewell, not a dirge. |
| 03 | The intro: the car on the road to Derceto, the walk up, the climb to the attic. Cut by 04 when play starts. | 1:17 | 2:42 | **Lyrics** | Edward's case notes, `doc20.txt`: "it's the kind of place ghosts run away from in terror. Grisly murders, curses, lunacy ..." Emily's letter, `doc21.txt`: "Derceto is waiting for me. I pray that my fear is nothing more than the fruit of my imagination." One file plays for both heroes, so the lyric is in one voice: Emily's, or a narrator's that fits either. |
| 04 | The attic theme: starts the game, and is queued again (`NEXT_MUSIC`) after every enemy dies, so it is the "calm restored" theme of the whole house. Played most. | 2:08 | 3:28 | none | Emily, `doc21.txt`: "That creaking old mansion, with its unusual tales, its secret library door, the ancient upstairs clock, all those occult books". Uneasy calm. |
| 05 | Danger: queued by every enemy when it engages (after the sting 12, 13 or 14) and runs while the fight lasts. | 1:44 | 1:46 | none | `doc09.txt`, "The Creatures of Night": "night engenders monsters and that night creatures exist." Driving, no resolution. |
| 07 | House haunting cue: an invisible object on each of the four house floors rolls dice every frame and now and then plays a creak, a groan, or this song. Rare, unannounced. | 2:03 | 2:27 | none | Jeremy's diary, `doc07.txt`, February 7: "His breath was ice and his burning eyes froze me; I could not move!" Sparse, long silences welcome. |
| 08 | Death: the hero's body carried to the tree in the cavern, "The End" (`@40`). Game over follows. | 0:34 | none | none | Pregzt, `doc18.txt`: "My servants will lay you upon the sacrificial stone. My roar will rend the night." One phrase, then nothing. |
| 09 | Epilogue, first part: the hero walks out of Derceto to the waiting car. Then 02. | 1:29 | none | none | `doc21.txt`: "Nothing will ever persuade me that my uncle was insane." Dawn after a night; relief with a shadow. |
| 10 | The ending cinematic: the oil lamp thrown, the tree and Pregzt burning (`ENDSEQ.PAK`), then "Get out of here!" (`@112`) as the cavern shakes. | 1:23 | none | none | `doc03.txt`, Book of Abdul: "Let the shadow of Cthulhu darken the sky." Climax, fire, collapse. |
| 11 | Caves haunting cue: the same dice as 07, on the three underground floors (cellar passages, caves, the pirates' cavern). | 3:20 | none | none | `doc17.txt`, the Astarte's song: "A skull! Go to port / Saber! To starboard! / Pass over that will / And with death you'll deal." Drums far off ("the insidious rhythm of far-off drums"). An optional sung version may use these eight lines. |
| 12 | Fight sting, type A: some enemies open with it (the attic window zombie among them), then 05. | 0:25 | none | none | `doc09.txt`: "He who prowls among books will perish by the blade." A hit, a rise, hand over to 05. |
| 13 | Fight sting, type B (the attic's other zombie among them), then 05. | 0:11 | none | none | `doc09.txt`: "Unhappy he who frees the prowler." |
| 14 | Fight sting, type C (the dance hall ghosts, the cave creatures among others), then 05. | 0:09 | none | none | `doc09.txt`: "He who flies in the dark caverns will scream in fear." |
| 15 | The first gramophone record, "You're listening to the Blue Danube" (`@942`). Diegetic: it comes out of the horn. | 2:51 | none | none | Strauss, The Blue Danube, 1866, public domain. A 1920s shellac recording: horn, hiss, wow. |
| 16 | The second record, "the Posthumous Opus 69 Nr 1" (`@1041`). | 1:39 | none | none | Chopin, Waltz Op. 69 No. 1, 1835, public domain. Same gramophone sound. |
| 17 | The third record, "the Dance of Death" (`@1031`): the one the dance hall's ghosts answer to; any record may draw "They seem to dislike this music" (`@108`). | 3:33 | none | none | Saint-Saëns, Danse macabre, 1874, public domain. Same gramophone sound. |

Notes for the generator:

- 04 and 05 alternate for most of the game; they should share a key or a
  motif so the hand-over does not jar.
- 12, 13 and 14 end where 05 begins; write them as upbeats to 05.
- 15 to 17 are the only pieces that are not score: the player hears them
  as a record in the room. Period sound matters more than fidelity.
- 02 and 03 are the vocal pieces agreed for this brief; everything else
  stays instrumental like the original.

## Extras the engine does not call yet

Numbers 18 and up are free. Files can be produced now; each needs a cue in
the engine before it plays, listed here as pending work.

| # | Piece | Length | Engine work needed |
|---|---|---|---|
| 18 | Title screen theme (the startup menu is silent today). | 1:30, loop-friendly | `playMusic(18)` when the startup menu opens, stop on New game or Load. |
| 19 | Ambient loop, the house (attic to ground floor). | 3:00, seamless loop | Looping does not exist: a loop flag on `osystem_playMusicFile`, and a cue on floor change when nothing else plays. |
| 20 | Ambient loop, cellar and caves. | 3:00, seamless loop | Same as 19. |
| 21 | Ambient loop, the dance hall. | 2:00, seamless loop | Same as 19, keyed to the room. |

Text for these: 19 Jeremy's diary (`doc07.txt`), 20 the pirate trial
(`doc17.txt`), 21 the dance of death (`@1031`, `@108`).

## Workflow for the generation project

1. `make lang-extract` here; copy `data/lang/en/` over (it is git-ignored).
2. Strip the `#` codes; keep `messages.txt` for the `@` lines and the
   documents for the fragments.
3. One prompt per row of the table above: the "Plays when" cell as the
   scene, the text cell as the quoted material, the length as the target;
   lyrics only for 02 and 03.
4. Name each result `NN.ogg` (or flac, mp3, wav), `NN` the row number, and
   drop the set into `data/aitd1/music/`. `make run` picks it up; on macOS
   `make run` links the folder into the app.
5. Check in-game as described below, then tick
   `docs/external-music-checklist.md` with the new files.

## Where the text is, and how this table was made

- `make lang-extract` writes the English and French game text as UTF-8 to
  `data/lang/<lang>/`: `messages.txt` (`@number:text`, the `@` numbers
  quoted above) and `doc01.txt` to `doc21.txt` (the readable documents;
  `#P`, `#T`, `#C`, `#G<n>` are page, tab, centre and image codes to strip).
  Fragments above are quoted from those files; the other project reads
  them from there.
- The cue column comes from a linear disassembly of every script in
  `LISTLIFE.PAK` (563 scripts; opcodes `MUSIC` 44, `NEXT_MUSIC` 80,
  `FADE_MUSIC` 81, with the AITD1 argument sizes), joined to the objects of
  `OBJETS.ITD` (26 words each) that run each script, and read against the
  room list in `data/aitd1/AITD1_walkthrough_llm.md`. `FADE_MUSIC` appears
  in no script. To redo it, `tools/aitd_data/pak.py` reads the PAK and
  `TatouSource/FitdLib/AITD1.cpp` lists the opcodes in order.
- In-game spot check: with `debug.logLifeScripts = true` the log prints
  `LM_MUSIC n` and `LM_NEXT_MUSIC n` as they fire. Rows 04, 12 and 05 are
  the first minute of a new game; row 07 needs patience. Not yet checked
  in-game at the time of writing.
