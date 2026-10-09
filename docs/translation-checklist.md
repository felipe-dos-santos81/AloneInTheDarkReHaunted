# Brazilian Portuguese checklist

Manual sign-off for the Portuguese text (spec:
`docs/superpowers/specs/2026-10-09-pt-br-translation-design.md`, local only).
Alone in the Dark 1. Build: `make build-fitd`; run: `make run data=DIR`. Pick
**Português** in the language menu (the sixth row). Run rows 1-21 and 24-26 with
`font.enableTTF = false`, then rows 15-23 with `font.enableTTF = true`.
Mark each cell `pass`, or `fail:` with a one-line note.

The text is checked on the branch by `make test` and `make lang-pack` (message
numbers, image-code order, characters the bitmap font draws, widths). What
those cannot show is how it looks in the game, which is this list. The 118
messages in `Assets/lang/pt-BR/width-ok.txt` are wider than the English; row 9
checks they fit.

| # | What to check | Result | Notes |
|---|---|---|---|
| 1 | Language menu: six rows, "Português" last; pick it by keyboard, then again by mouse | | |
| 2 | Language menu: the five flags line up with English, Français, Italiano, Español and Deutsch (no flag for Português) | | |
| 3 | Language menu with `graphics.hdBackgrounds = false` (no HD backgrounds, so the classic frame): the frame encloses all six rows | | |
| 4 | Start menu (including "Voltar ao macOS/Linux" off Windows) and the loading screen ("Aguarde...") are in Portuguese, centred | | |
| 5 | Picking up an object: the found-object box is in Portuguese and the name fits the box | | |
| 6 | Inventory: object names and action names are in Portuguese | | |
| 7 | Using and throwing an object: the messages are in Portuguese | | |
| 8 | Save and load screens are in Portuguese | | |
| 9 | Width: the longest names in `width-ok.txt` fit their boxes (spot-check messages 850 and 730, then a few more) | | |
| 10 | System menu (Escape): "Tela: Janela/Cheia", "Controles", "Dicas: Sim/Não", "Mapa" | | |
| 11 | Controls menu: headers "Ação", "Tecla", "Botão"; "Voltar" and "Padrão" | | |
| 12 | Switch to Français: the controls menu shows "Par Défaut" with the é | | |
| 13 | F1 dialog: every page, every tooltip | | |
| 14 | F1 dialog pressed in game: the footer ("Salvar", "Fechar" in game) and the status messages after Save, Reload and Restore | | |
| 15 | Book (e.g. doc05): pages turn, text fits the page, image pages (#G) still show | | |
| 16 | Letter (e.g. doc07): text fits, no cut-off line | | |
| 17 | Notebook: text fits, no cut-off line | | |
| 18 | The intro letters at a new game (docs 20 and 21) | | |
| 19 | Credits (doc19) | | |
| 20 | ã and õ (e.g. "não", "ações", "mãe") draw as one letter with the tilde, not as two letters | | |
| 21 | Accented capitals (À Á Â Ã Ê Í Ó Ô Õ Ú) draw as the plain capital in the bitmap font | | |
| 22 | The same capitals draw with their accents in TrueType text | | |
| 23 | Books, letters and notebooks read in TrueType text (`font.enableTTF = true`) | | |
| 24 | Voice-over: books and letters play the English voice-over page by page | | |
| 25 | Switch to English: text, the original font and the menus are the original, with no leftover Portuguese | | |
| 26 | Quit and restart: the language menu appears again and Português can be picked again | | |

## Known issues

Not fixed on this branch:

- With `font.enableTTF`, some spaces between words disappear in documents (the
  renderer; seen in Portuguese documents, not yet compared with English).
- In Portuguese the controls menu's key and button name column (SDL's key
  names, e.g. "Up", "Left") stays English.
- On macOS and Linux, message 13 shows "Return to macOS/Linux" in English in
  the original languages (`main.cpp`); Portuguese shows "Voltar ao macOS/Linux".
- In TrueType mode the lantern-glow suppression matches the English "lamp has
  no oil" text, so it does not fire in French or Portuguese (`fontTTF.cpp`).
- Documents 01-19 were checked as simulated pages only; only 05, 07, 20 and 21
  have been seen in the game.
