# External music checklist

Manual sign-off for the music files (spec:
`docs/superpowers/specs/2026-10-10-external-music-design.md`, local only),
Alone in the Dark 1. Build with `make build-fitd` and run with `make run`.
Mark each cell `pass`, or `fail:` with a one-line note.

- **The files** are `data/aitd1/music/00.wav` to `07.wav`. On macOS `make run`
  links that folder into the app, which reads its files from
  `Tatou.app/Contents/Resources`; `aitd_remaster.cfg` is there too.
- **Settings.** Unless a row says otherwise, `music.external` is `true`,
  `music.folder` is `music` and `music.volume` is `1.0`. A cfg saved before
  this option worked holds `music.external = false`: tick **External music**
  in F1 first.
- Rows 4, 6, 11 and 12 edit the cfg, and rows 5 and 10 the music folder.
  Restore both afterwards.

| # | Check | Result |
|---|---|---|
| 1 | Start the game and a new game: the intro's song and the attic's song play from `data/aitd1/music` | |
| 2 | Let a song end without changing room: silence follows and nothing restarts | |
| 3 | With `debug.logLifeScripts = true`, play until the log prints `LM_NEXT_MUSIC` while a song plays: the queued song starts when the current one ends | |
| 3b | Play until the log prints `LM_FADE_MUSIC` while a song plays: the song fades out over about 3 s and the next song starts after the wait | |
| 4 | `music.folder = "nope"`: the game runs with no error and is silent, as before this change | |
| 5 | Rename `01.wav` away: that song is silent, the others play | |
| 6 | `music.external = false`: silent, as before this change | |
| 7 | Save during a song, load the save: the song restarts from the beginning | |
| 8 | `music.volume = 0.3`: the next song is quieter than the sound effects | |
| 9 | Quit (window close and the menu's quit) while a song plays: clean exit, no crash | |
| 10 | Replace `03.wav` by an empty file: that song is silent, the game goes on, the next song plays | |
| 11 | `music.volume = 2.0` in the cfg: plays at full volume (the game clamps the value to 1.0 when it reads the cfg) | |
| 12 | `music.folder = "music/"` and then an absolute path to the same folder: the songs play | |
| 13 | Turn "External music" off in F1 mid-song: the song keeps playing until the script's next stop or fade, which still act on it | |
