///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// The remaster's own menu strings per language.
///////////////////////////////////////////////////////////////////////////////
#include "uiText.h"

#include <cstring>
#include <map>
#include <string>

namespace text {

// en, fr, it, es, de, pt. French, Italian, Spanish and German come word for
// word from the menus' former if-chains; strings they never had stay "".
// This file is UTF-8 (MSVC compiles it with /utf-8; see the CMake change above).
const UiRow kUiRows[] = {
    { "Please Wait...", "Veuillez Patienter...", "Attendere Prego...", "Por Favor Espere...", "Bitte Warten...", "Aguarde..." },
    { "Display: Fullscreen", "Affichage: Plein écran", "Schermo: Intero", "Pantalla: Completa", "Anzeige: Vollbild", "Tela: Cheia" },
    { "Display: Windowed", "Affichage: Fenêtré", "Schermo: Finestra", "Pantalla: Ventana", "Anzeige: Fenster", "Tela: Janela" },
    { "Controls", "Commandes", "Comandi", "Controles", "Steuerung", "Controles" },
    { "Hints: On", "Indices: Activé", "Suggerimenti: On", "Pistas: On", "Hinweise: An", "Dicas: Sim" },
    { "Hints: Off", "Indices: Désactivé", "Suggerimenti: Off", "Pistas: Off", "Hinweise: Aus", "Dicas: Não" },
    { "Map", "Carte", "Mappa", "Mapa", "Karte", "Mapa" },
    { "Controller Connected", "Manette Connectée", "Controller Connesso", "Mando Conectado", "Controller Verbunden", "Controle Conectado" },
    { "No Controller", "Pas de Manette", "Nessun Controller", "Sin Mando", "Kein Controller", "Sem Controle" },
    { "Action", "Action", "Azione", "Acción", "Aktion", "Ação" },
    { "Key", "Touche", "Tasto", "Tecla", "Taste", "Tecla" },
    { "Button", "Bouton", "Pulsante", "Botón", "Knopf", "Botão" },
    { "Back", "Retour", "Indietro", "Volver", "Zurück", "Voltar" },
    { "Defaults", "Par Défaut", "Predefiniti", "Predeterminado", "Standard", "Padrão" },
    // Task 7 adds the remasterOptions rows.
};
const size_t kUiRowCount = sizeof(kUiRows) / sizeof(kUiRows[0]);

namespace {

const char* column(const UiRow& r, const std::string& lang)
{
    if (lang == "FRANCAIS") return r.fr;
    if (lang == "ITALIANO") return r.it;
    if (lang == "ESPAGNOL") return r.es;
    if (lang == "DEUTSCH") return r.de;
    if (lang == "PORTUGUE") return r.pt;
    return nullptr;
}

// Unicode -> CP850. kCp850 matches Python's cp850 codec (checked byte for byte).
int dosCode(unsigned cp)
{
    if (cp < 0x80) return (int)cp;
    if (cp == 0xA9) return 0xA9;   // the game font draws 0xA9 as (c)
    static const unsigned short kCp850[128] = {
        0x00C7,0x00FC,0x00E9,0x00E2,0x00E4,0x00E0,0x00E5,0x00E7,0x00EA,0x00EB,0x00E8,0x00EF,0x00EE,0x00EC,0x00C4,0x00C5,
        0x00C9,0x00E6,0x00C6,0x00F4,0x00F6,0x00F2,0x00FB,0x00F9,0x00FF,0x00D6,0x00DC,0x00F8,0x00A3,0x00D8,0x00D7,0x0192,
        0x00E1,0x00ED,0x00F3,0x00FA,0x00F1,0x00D1,0x00AA,0x00BA,0x00BF,0x00AE,0x00AC,0x00BD,0x00BC,0x00A1,0x00AB,0x00BB,
        0x2591,0x2592,0x2593,0x2502,0x2524,0x00C1,0x00C2,0x00C0,0x00A9,0x2563,0x2551,0x2557,0x255D,0x00A2,0x00A5,0x2510,
        0x2514,0x2534,0x252C,0x251C,0x2500,0x253C,0x00E3,0x00C3,0x255A,0x2554,0x2569,0x2566,0x2560,0x2550,0x256C,0x00A4,
        0x00F0,0x00D0,0x00CA,0x00CB,0x00C8,0x0131,0x00CD,0x00CE,0x00CF,0x2518,0x250C,0x2588,0x2584,0x00A6,0x00CC,0x2580,
        0x00D3,0x00DF,0x00D4,0x00D2,0x00F5,0x00D5,0x00B5,0x00FE,0x00DE,0x00DA,0x00DB,0x00D9,0x00FD,0x00DD,0x00AF,0x00B4,
        0x00AD,0x00B1,0x2017,0x00BE,0x00B6,0x00A7,0x00F7,0x00B8,0x00B0,0x00A8,0x00B7,0x00B9,0x00B3,0x00B2,0x25A0,0x00A0,
    };
    for (int i = 0; i < 128; i++)
        if (kCp850[i] == cp && i + 0x80 != 0xA9)
            return i + 0x80;
    return -1;
}

} // namespace

bool toDos(const char* utf8, std::string* out)
{
    out->clear();
    for (const unsigned char* p = (const unsigned char*)utf8; *p;)
    {
        unsigned cp = *p++;
        int extra = cp >= 0xF0 ? 3 : cp >= 0xE0 ? 2 : cp >= 0xC0 ? 1 : 0;
        if (extra) cp &= 0x3F >> extra;
        for (; extra > 0 && (*p & 0xC0) == 0x80; extra--) cp = (cp << 6) | (*p++ & 0x3F);
        const int code = extra ? -1 : dosCode(cp);
        if (code < 0) return false;
        out->push_back((char)code);
    }
    return true;
}

const char* uiText(const std::string& lang, const char* english)
{
    for (size_t i = 0; i < kUiRowCount; i++)
        if (std::strcmp(kUiRows[i].en, english) == 0)
        {
            const char* t = column(kUiRows[i], lang);
            return t && *t ? t : english;
        }
    return english;
}

const char* uiTextDos(const std::string& lang, const char* english)
{
    static std::map<std::pair<std::string, std::string>, std::string> cache;
    auto key = std::make_pair(lang, std::string(english));
    auto it = cache.find(key);
    if (it == cache.end())
    {
        std::string dos;
        if (!toDos(uiText(lang, english), &dos))
            dos = english;   // the doctest keeps every cell encodable; English is ASCII
        it = cache.emplace(key, dos).first;
    }
    return it->second.c_str();
}

} // namespace text
