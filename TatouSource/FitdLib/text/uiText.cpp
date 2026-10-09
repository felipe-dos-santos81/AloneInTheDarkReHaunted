///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// The remaster's own menu strings per language.
///////////////////////////////////////////////////////////////////////////////
#include "uiText.h"
#include "cp850.h"

#include <cstring>
#include <map>
#include <string>

namespace text {

// en, fr, it, es, de, pt. French, Italian, Spanish and German come word for
// word from the menus' former if-chains; strings they never had stay "".
// This file is UTF-8 (MSVC compiles it with /utf-8: FitdLib/CMakeLists.txt and
// tests/engine/CMakeLists.txt).
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
    // remasterOptions.cpp (the F1 dialog)
    { "Backgrounds and renderer", "", "", "", "", "Fundos e renderizador" },
    { "HD backgrounds", "", "", "", "", "Fundos em HD" },
    { "HD character models", "", "", "", "", "Modelos de personagem em HD" },
    { "Background scale", "", "", "", "", "Escala dos fundos" },
    { "Texture filtering", "", "", "", "", "Filtragem de texturas" },
    { "Wall depth for ambient occlusion", "", "", "", "", "Profundidade das paredes para oclusão ambiente" },
    { "Adds collision-wall depth to the scene so SSAO can darken architectural edges.", "", "", "", "", "Adiciona à cena a profundidade das paredes de colisão, para que o SSAO escureça as quinas da arquitetura." },
    { "Blurred menu background", "", "", "", "", "Fundo do menu desfocado" },
    { "Menu blur amount", "", "", "", "", "Desfoque do menu" },
    { "Gameplay hints", "", "", "", "", "Dicas durante o jogo" },
    { "Menu artwork", "", "", "", "", "Arte dos menus" },
    { "Renderer backend", "", "", "", "", "Renderizador" },
    { "Renderer changes take effect after restarting the game.", "", "", "", "", "As mudanças de renderizador têm efeito após reiniciar o jogo." },
    { "MSAA is applied when the renderer is recreated or the window changes size.", "", "", "", "", "O MSAA é aplicado quando o renderizador é recriado ou a janela muda de tamanho." },
    { "Fullscreen", "", "", "", "", "Tela cheia" },
    { "Lighting and post-processing", "", "", "", "", "Iluminação e pós-processamento" },
    { "Bloom threshold", "", "", "", "", "Limiar do bloom" },
    { "Bloom intensity", "", "", "", "", "Intensidade do bloom" },
    { "Bloom passes", "", "", "", "", "Passes do bloom" },
    { "Film grain", "", "", "", "", "Granulação de filme" },
    { "Grain intensity", "", "", "", "", "Intensidade da granulação" },
    { "SSAO radius", "", "", "", "", "Raio do SSAO" },
    { "SSAO intensity", "", "", "", "", "Intensidade do SSAO" },
    { "Vignette", "", "", "", "", "Vinheta" },
    { "Vignette intensity", "", "", "", "", "Intensidade da vinheta" },
    { "Vignette radius", "", "", "", "", "Raio da vinheta" },
    { "Screen-space global illumination", "", "", "", "", "Iluminação global em espaço de tela" },
    { "SSGI radius", "", "", "", "", "Raio do SSGI" },
    { "SSGI intensity", "", "", "", "", "Intensidade do SSGI" },
    { "SSGI samples", "", "", "", "", "Amostras do SSGI" },
    { "Light probes", "", "", "", "", "Sondas de luz" },
    { "Light probe intensity", "", "", "", "", "Intensidade das sondas de luz" },
    { "Cinematic image finishing", "", "", "", "", "Acabamento cinematográfico da imagem" },
    { "Color grading", "", "", "", "", "Gradação de cor" },
    { "Exposure", "", "", "", "", "Exposição" },
    { "Contrast", "", "", "", "", "Contraste" },
    { "Saturation", "", "", "", "", "Saturação" },
    { "Temperature", "", "", "", "", "Temperatura" },
    { "Shadow lift", "", "", "", "", "Clareamento das sombras" },
    { "Highlight rolloff", "", "", "", "", "Suavização das altas luzes" },
    { "Animation presentation", "", "", "", "", "Apresentação das animações" },
    { "Smooth skeletal poses", "", "", "", "", "Suavizar poses do esqueleto" },
    { "Pose smoothing strength", "", "", "", "", "Força da suavização de poses" },
    { "Only rendered joint poses are eased. Root motion, collision and hit timing remain unchanged.", "", "", "", "", "Só as poses desenhadas das articulações são suavizadas. O movimento da raiz, a colisão e o tempo dos golpes não mudam." },
    { "Mouse", "", "", "", "", "Mouse" },
    { "Mouse gameplay (hold left button to walk, double-click and hold to run)", "", "", "", "", "Jogabilidade com mouse (segure o botão esquerdo para andar, clique duas vezes e segure para correr)" },
    { "Combat", "", "", "", "", "Combate" },
    { "Hit back automatically when an enemy strikes you", "", "", "", "", "Contra-atacar automaticamente quando um inimigo atingir você" },
    { "Enemy attack pace", "", "", "", "", "Ritmo dos ataques inimigos" },
    { "Controller behavior", "", "", "", "", "Comportamento do controle" },
    { "Enable controller", "", "", "", "", "Ativar controle" },
    { "Analog deadzone", "", "", "", "", "Zona morta do analógico" },
    { "Analog sensitivity", "", "", "", "", "Sensibilidade do analógico" },
    { "Invert Y axis", "", "", "", "", "Inverter eixo Y" },
    { "Analog movement", "", "", "", "", "Movimento analógico" },
    { "Bindings", "", "", "", "", "Mapeamento de controles" },
    { "Keyboard", "", "", "", "", "Teclado" },
    { "Gamepad", "", "", "", "", "Controle" },
    { "Unbound", "", "", "", "", "Não atribuído" },
    { "Typography", "", "", "", "", "Tipografia" },
    { "TTF fonts", "", "", "", "", "Fontes TTF" },
    { "Font path", "", "", "", "", "Caminho da fonte" },
    { "Font size", "", "", "", "", "Tamanho da fonte" },
    { "Hide original bitmap text", "", "", "", "", "Ocultar texto bitmap original" },
    { "Font changes are fully applied after restarting the game.", "", "", "", "", "As mudanças de fonte só têm efeito completo após reiniciar o jogo." },
    { "Music", "", "", "", "", "Música" },
    { "External music", "", "", "", "", "Música externa" },
    { "Music folder", "", "", "", "", "Pasta de música" },
    { "External music source changes take effect when a track is next loaded or after restart.", "", "", "", "", "As mudanças na fonte da música externa valem a partir da próxima faixa carregada ou após reiniciar." },
    { "Replacement content and development pipelines", "", "", "", "", "Conteúdo de substituição e pipelines de desenvolvimento" },
    { "Load edited HD masks", "", "", "", "", "Carregar máscaras HD editadas" },
    { "Dump generated masks to PNG", "", "", "", "", "Exportar máscaras geradas em PNG" },
    { "Load HD sequence frames", "", "", "", "", "Carregar quadros de sequência HD" },
    { "Dump decoded sequence frames", "", "", "", "", "Exportar quadros de sequência decodificados" },
    { "Dump original backgrounds", "", "", "", "", "Exportar fundos originais" },
    { "Dump options can create many files and are intended for content authors.", "", "", "", "", "As opções de exportação podem criar muitos arquivos e são para autores de conteúdo." },
    { "Game data", "", "", "", "", "Dados do jogo" },
    { "Steamless mode", "", "", "", "", "Modo sem Steam" },
    { "Disables automatic game-data copying and Steam overlay integration after restart.", "", "", "", "", "Desativa a cópia automática dos dados do jogo e a integração com o overlay da Steam após reiniciar." },
    { "Jack in the Dark mode", "", "", "", "", "Modo Jack in the Dark" },
    { "Selects the JACK data set. Restart required.", "", "", "", "", "Seleciona os dados do JACK. É preciso reiniciar." },
    { "Diagnostics", "", "", "", "", "Diagnóstico" },
    { "Graphics API validation", "", "", "", "", "Validação da API gráfica" },
    { "Enables Direct3D/Vulkan validation after restart. DirectX may raise first-chance 0x87A exceptions in a debugger when it detects an invalid GPU call.", "", "", "", "", "Ativa a validação do Direct3D/Vulkan após reiniciar. O DirectX pode gerar exceções 0x87A de primeira chance em um depurador ao detectar uma chamada inválida à GPU." },
    { "Log LIFE script dispatch", "", "", "", "", "Registrar execução dos scripts LIFE" },
    { "Dump LIFE scripts on startup", "", "", "", "", "Exportar scripts LIFE ao iniciar" },
    { "Generate native LIFE scripts", "", "", "", "", "Gerar scripts LIFE nativos" },
    { "Enable native LIFE scripts", "", "", "", "", "Ativar scripts LIFE nativos" },
    { "Diagnostic options may affect performance and require a restart.", "", "", "", "", "As opções de diagnóstico podem afetar o desempenho e exigem reinício." },
    { "Re-Haunted Remaster Options###ReHauntedOptions", "", "", "", "", "Opções do Re-Haunted Remaster###ReHauntedOptions" },
    { "Configure Re-Haunted before continuing  |  Changes preview live where supported", "", "", "", "", "Configure o Re-Haunted antes de continuar  |  As mudanças aparecem na hora quando possível" },
    { "Home or F1 toggles this dialog  |  Changes preview live where supported", "", "", "", "", "Home ou F1 abre e fecha esta janela  |  As mudanças aparecem na hora quando possível" },
    { "Graphics", "", "", "", "", "Gráficos" },
    { "Effects", "", "", "", "", "Efeitos" },
    { "Color & Motion", "", "", "", "", "Cor e movimento" },
    { "UI & Audio", "", "", "", "", "Interface e áudio" },
    { "Content", "", "", "", "", "Conteúdo" },
    { "Advanced", "", "", "", "", "Avançado" },
    { "Don't show this dialog again at startup", "", "", "", "", "Não mostrar mais esta janela ao iniciar" },
    { "(Home or F1 will still open it)", "", "", "", "", "(você ainda pode abri-la com Home ou F1)" },
    { "Save & Continue", "", "", "", "", "Salvar e continuar" },
    { "Save settings", "", "", "", "", "Salvar" },
    { "Settings saved to aitd_remaster.cfg", "", "", "", "", "Configurações salvas em aitd_remaster.cfg" },
    { "Reload saved", "", "", "", "", "Recarregar" },
    { "Saved settings restored", "", "", "", "", "Configurações recarregadas" },
    { "Restore defaults", "", "", "", "", "Restaurar padrões" },
    { "Defaults restored (not saved yet)", "", "", "", "", "Padrões restaurados (ainda não salvos)" },
    { "Continue", "", "", "", "", "Continuar" },
    { "Close", "", "", "", "", "Fechar" },
    { "Normal", "", "", "", "", "Normal" },
    { "Slower", "", "", "", "", "Mais lento" },
    { "Much slower", "", "", "", "", "Bem mais lento" },
    { "Draws a character as its HD model (models_hd/body_<KEY>.hdm, from make import-models) where one exists; every other character, and every model that fails its checks, stays classic.", "", "", "", "", "Desenha o personagem com seu modelo HD (models_hd/body_<KEY>.hdm, de make import-models) quando existir; os demais personagens, e todo modelo que falhar nas verificações, continuam clássicos." },
    { "After an enemy's melee blow, the hero turns and strikes back once with the weapon in hand, or with bare fists. Any input of your own cancels it. Alone in the Dark 1 only.", "", "", "", "", "Depois de um golpe corpo a corpo de um inimigo, o herói se vira e revida uma vez com a arma que tem na mão, ou com os punhos. Qualquer comando seu cancela o revide. Apenas no Alone in the Dark 1." },
    { "Gives you more time between an enemy's melee attacks: two or three times the original gap. Enemies keep their speed. Alone in the Dark 1 only.", "", "", "", "", "Dá mais tempo entre os ataques corpo a corpo de um inimigo: duas ou três vezes o intervalo original. Os inimigos mantêm a velocidade. Apenas no Alone in the Dark 1." },
    { "Up", "", "", "", "", "Para cima" },
    { "Down", "", "", "", "", "Para baixo" },
    { "Left", "", "", "", "", "Para a esquerda" },
    { "Right", "", "", "", "", "Para a direita" },
    { "Confirm", "", "", "", "", "Confirmar" },
    { "Cancel", "", "", "", "", "Cancelar" },
    { "Quick Turn Left", "", "", "", "", "Giro veloz à esquerda" },
    { "Quick Turn Right", "", "", "", "", "Giro veloz à direita" },
    { "Run", "", "", "", "", "Correr" },
    { "Press Key...", "", "", "", "", "Pressione..." },
    { "Press...", "", "", "", "", "Pressione..." },
    { "No Preview", "", "", "", "", "Sem prévia" },
    { "Return to macOS", "", "", "", "", "Voltar ao macOS" },
    { "Return to Linux", "", "", "", "", "Voltar ao Linux" },
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

// Unicode -> CP850, by reverse lookup in cp850.h.
int dosCode(unsigned cp)
{
    if (cp < 0x80) return (int)cp;
    if (cp == 0xA9) return 0xA9;   // the game font draws 0xA9 as (c)
    for (int i = 0; i < 128; i++)
        if (kCp850ToUnicode[i] == cp && i + 0x80 != 0xA9)
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
