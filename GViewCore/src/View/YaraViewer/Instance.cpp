#include "YaraViewer.hpp"
#include <algorithm>
#include <cstdlib> 
#include <iostream>
#include <fstream>   
#include <string>    
#include <codecvt>
#include <locale>

#include <windows.h>
#include <shellapi.h>
#undef MessageBox

using namespace GView::View::YaraViewer;
using namespace AppCUI::Input;
namespace fs = std::filesystem;

Config Instance::config;

constexpr int startingX = 5;

Instance::Instance(Reference<GView::Object> _obj, Settings* _settings)
    : ViewControl("Yara View", UserControlFlags::ShowVerticalScrollBar | UserControlFlags::ScrollBarOutsideControl), settings(nullptr)
{
    this->obj = _obj;

    this->cursorRow     = 0;
    this->cursorCol     = 0;
    this->startViewLine = 0;
    this->leftViewCol   = 0;

    this->selectionActive    = false;
    this->selectionAnchorRow = 0;
    this->selectionAnchorCol = 0;

    // settings
    if ((_settings) && (_settings->data))
    {
        // move settings data pointer
        this->settings.reset((SettingsData*) _settings->data);
        _settings->data = nullptr;
    }
    else
    {
        // default setup
        this->settings.reset(new SettingsData());
    }

    if (config.Loaded == false)
        config.Initialize();

    layout.visibleLines = 1;
    layout.maxCharactersPerLine = 10;
}


bool Instance::GetPropertyValue(uint32 propertyID, PropertyValue& value)
{
    NOT_IMPLEMENTED(false)
}
bool Instance::SetPropertyValue(uint32 propertyID, const PropertyValue& value, String& error)
{
    NOT_IMPLEMENTED(false)
}
void Instance::SetCustomPropertyValue(uint32 propertyID)
{
}
bool Instance::IsPropertyValueReadOnly(uint32 propertyID)
{
    NOT_IMPLEMENTED(false)
}
const vector<Property> Instance::GetPropertiesList()
{
    return {};
}
bool Instance::GoTo(uint64 offset)
{
    NOT_IMPLEMENTED(false)
}
bool Instance::Select(uint64 offset, uint64 size)
{
    NOT_IMPLEMENTED(false)
}
bool Instance::ShowGoToDialog()
{
    NOT_IMPLEMENTED(false)
} 
std::string_view Instance::GetCategoryNameForSerialization() const
{
    return "YaraViewer";
}
bool Instance::AddCategoryBeforePropertyNameWhenSerializing() const
{
    return true;
}
bool Instance::ShowCopyDialog()
{
    NOT_IMPLEMENTED(false)
}

bool Instance::ShowFindDialog()
{
    // 1. Instanțiem și afișăm fereastra noastră
    FindDialog dlg;
    if (dlg.Show() != Dialogs::Result::Ok)
        return true;

    std::string text = dlg.resultText;
    if (text.empty())
        return true;

    this->lastSearchText = text;

    this->isButtonFindPressed = true;
    this->SetFocus();

    // --- PREGĂTIRE CASE-INSENSITIVE ---
    // Facem o copie a textului de căutat și îl transformăm în litere mici
    std::string textToFindLower = text;
    std::transform(textToFindLower.begin(), textToFindLower.end(), textToFindLower.begin(), ::tolower);

    // --- LOGICA DE CĂUTARE ---

    uint32 totalLines = (uint32) yaraLines.size();
    if (totalLines == 0)
        return true;

    // Începem căutarea de la linia următoare cursorului
    uint32 startIdx = this->cursorRow + 1;

    for (uint32 i = 0; i < totalLines; i++) {
        uint32 currentIdx = (startIdx + i) % totalLines;

        const auto& line = yaraLines[currentIdx];

        // --- TRANSFORMARE LINIE CURENTĂ ---
        // Facem o copie a textului liniei și îl transformăm în litere mici
        std::string lineLower = line.text;
        std::transform(lineLower.begin(), lineLower.end(), lineLower.begin(), ::tolower);

        // Căutăm textul mic în linia mică
        size_t foundPos = lineLower.find(textToFindLower);

        if (foundPos != std::string::npos) {
            // A. Expandare automată dacă e ascuns
            if (!line.isVisible) {
                for (int k = (int) currentIdx - 1; k >= 0; k--) {
                    if (yaraLines[k].type == LineType::FileHeader) {
                        if (!yaraLines[k].isExpanded) {
                            ToggleFold(k);
                        }
                        break;
                    }
                }
            }

            // B. Calculăm coordonatele VIZUALE și activăm SELECȚIA
            for (size_t v = 0; v < visibleIndices.size(); v++) {
                if (visibleIndices[v] == currentIdx) {
                    SelectMatch((uint32) v, foundPos, (uint32) textToFindLower.length());
                    return true;
                }
            }
        }
    }

    AppCUI::Dialogs::MessageBox::ShowError("Not Found", "Text '" + text + "' not found!");
    
    return true;
}

bool Instance::FindNext()
{
    if (lastSearchText.empty())
        return false;
    if (visibleIndices.empty() || yaraLines.empty())
        return false;

    // 1. Pregătire text (lowercase)
    std::string textToFindLower = lastSearchText;
    std::transform(textToFindLower.begin(), textToFindLower.end(), textToFindLower.begin(), ::tolower);

    uint32 totalLines = (uint32) yaraLines.size();

    // --- PASUL CRITIC: Aflăm indexul REAL al liniei curente ---
    // cursorRow este vizual. Trebuie să luăm indexul real din visibleIndices.
    uint32 currentRealIndex = 0;
    if (this->cursorRow < visibleIndices.size()) {
        currentRealIndex = (uint32) visibleIndices[this->cursorRow];
    }

    // --- FAZA 1: Căutăm pe restul liniei curente (REALĂ) ---
    {
        const auto& line      = yaraLines[currentRealIndex]; // Folosim indexul REAL
        std::string lineLower = line.text;
        std::transform(lineLower.begin(), lineLower.end(), lineLower.begin(), ::tolower);

        // Calculăm offset-ul
        int searchStartOffset = this->cursorCol;
        if (line.type == LineType::FileHeader)
            searchStartOffset -= 4; // "[ ] "
        else if (line.indentLevel > 0)
            searchStartOffset -= 4; // "    "

        if (searchStartOffset < 0)
            searchStartOffset = 0;

        // Căutăm
        size_t foundPos = lineLower.find(textToFindLower, searchStartOffset);

        if (foundPos != std::string::npos) {
            // Pentru selectare avem nevoie de rândul VIZUAL (care e this->cursorRow)
            SelectMatch(this->cursorRow, foundPos, (uint32) textToFindLower.length());
            return true;
        }
    }

    // --- FAZA 2: Căutăm pe următoarele linii (folosind indecși REALI) ---
    // Folosim i <= totalLines pentru a permite wrap-around complet (inclusiv revenirea la linia curentă la început)
    for (uint32 i = 1; i <= totalLines; i++) {
        uint32 nextRealIdx = (currentRealIndex + i) % totalLines; // Wrap around pe datele reale

        const auto& line      = yaraLines[nextRealIdx];
        std::string lineLower = line.text;
        std::transform(lineLower.begin(), lineLower.end(), lineLower.begin(), ::tolower);

        size_t foundPos = lineLower.find(textToFindLower); // De la 0

        if (foundPos != std::string::npos) {
            // 1. Expandăm dacă e ascuns
            if (!line.isVisible) {
                for (int k = (int) nextRealIdx - 1; k >= 0; k--) {
                    if (yaraLines[k].type == LineType::FileHeader) {
                        if (!yaraLines[k].isExpanded)
                            ToggleFold(k);
                        break;
                    }
                }
            }

            // 2. Găsim noul index VIZUAL pentru linia reală găsită
            // (Deoarece ToggleFold a modificat visibleIndices, trebuie să căutăm unde a ajuns linia)
            for (size_t v = 0; v < visibleIndices.size(); v++) {
                if (visibleIndices[v] == nextRealIdx) {
                    SelectMatch((uint32) v, foundPos, (uint32) textToFindLower.length());
                    return true;
                }
            }
        }
    }

    AppCUI::Dialogs::MessageBox::ShowNotification("Info", "No more occurrences found.");
    return false;
}

// Helper pentru selectare (ca să nu scriem codul de 2 ori)
void Instance::SelectMatch(uint32 visualRow, size_t startRawCol, uint32 length)
{
    // 1. Aflăm indexul REAL
    size_t realIndex = visibleIndices[visualRow];
    const auto& line = yaraLines[realIndex];

    // 2. Calculăm coloana vizuală
    size_t visualCol = startRawCol;
    if (line.type == LineType::FileHeader)
        visualCol += 4; // "[ ] "
    else if (line.indentLevel > 0)
        visualCol += 4; // "    "

    // 3. SETĂM VARIABILELE DE HIGHLIGHT (NU SELECȚIA)
    this->searchActive          = true;
    this->searchResultRealIndex = realIndex; // Reținem linia reală
    this->searchResultStartCol  = (uint32) visualCol;
    this->searchResultLen       = length;

    // 4. Mutăm doar cursorul acolo (fără să activăm selecția)
    this->cursorRow = visualRow;
    this->cursorCol = (uint32) (visualCol + length);

    // Asigurăm-ne că selecția veche e oprită
    this->selectionActive = false;

    MoveTo();
}


void Instance::PaintCursorInformation(AppCUI::Graphics::Renderer& renderer, uint32 width, uint32 height)
{
    if (height == 0)
        return;
    ColorPair cfgColor = { Color::White, Color::Transparent };

    LocalString<128> info;
    info.Format("Ln: %u | Col: %u", this->cursorRow + 1, this->cursorCol + 1);

    renderer.Clear(' ', cfgColor);
    renderer.WriteSingleLineText(1, 0, info.GetText(), cfgColor);
}

void Instance::Paint(Graphics::Renderer& renderer)
{
    ColorPair textNormal    = ColorPair{ Color::White, Color::Transparent };
    ColorPair textCursor    = ColorPair{ Color::Black, Color::White };
    ColorPair arrowColor    = ColorPair{ Color::Gray, Color::Transparent };
    ColorPair marginColor   = ColorPair{ Color::Gray, Color::Transparent };
    ColorPair textSelection = ColorPair{ Color::Black, Color::Silver };

    ColorPair ruleBlockColor = ColorPair{ Color::Green, Color::Transparent };
    ColorPair checkboxColor  = ColorPair{ Color::Red, Color::Transparent };    // pt [x]
    ColorPair headerColor    = ColorPair{ Color::Yellow, Color::Transparent }; // pt nume fisier
    ColorPair infoColor      = ColorPair{ Color::Red, Color::Transparent };
    ColorPair matchColor     = ColorPair{ Color::Yellow, Color::Transparent };

    ColorPair foldColor = ColorPair{ Color::Aqua, Color::Transparent };

    uint32 rows           = this->GetHeight();
    uint32 width          = this->GetWidth();
    const int LEFT_MARGIN = 4;

    uint32 selStartRow = this->cursorRow;
    uint32 selStartCol = this->cursorCol;
    uint32 selEndRow   = this->selectionAnchorRow;
    uint32 selEndCol   = this->selectionAnchorCol;

    GetRulesFiles();

    if (visibleIndices.empty() && !yaraLines.empty())
        UpdateVisibleIndices();

    if (this->selectionActive) {
        // Ordonăm: Start trebuie să fie < End
        if (selStartRow > selEndRow || (selStartRow == selEndRow && selStartCol > selEndCol)) {
            std::swap(selStartRow, selEndRow);
            std::swap(selStartCol, selEndCol);
        }
    }

    for (uint32 tr = 0; tr < rows; tr++) {
        // 1. Mapăm linia ecranului la indexul din cache
        uint32 visualIndex = this->startViewLine + tr;
        if (visualIndex >= visibleIndices.size())
            break;

        // 2. Mapăm indexul din cache la datele reale
        size_t realIndex     = visibleIndices[visualIndex];
        const LineInfo& info = yaraLines[realIndex];

        std::string displayText = info.text;


        int checkboxStart = -1;
        int checkboxEnd   = -1;
        int arrowStart    = -1;

        // --- ADAUGĂM VIZUAL SĂGEATA DE EXPANDARE ---
        if (info.type == LineType::FileHeader) {
            // 1. Prefix: Checkbox [ ]
            std::string prefix = info.isChecked ? "[x] " : "[ ] ";
            displayText        = prefix + displayText;

            checkboxStart = 0;
            checkboxEnd   = 4; // "[ ] " are 4 caractere (indecșii 0,1,2,3)

            // 2. Suffix: Săgeată > sau v
            std::string arrow = info.isExpanded ? " v" : " >";
            arrowStart        = (int) displayText.length();
            displayText += arrow;
        } else if (info.indentLevel > 0) {
            displayText.insert(0, "    ");
        }

        renderer.WriteSpecialCharacter(LEFT_MARGIN - 1, tr, SpecialChars::BoxVerticalSingleLine, marginColor);
        if (visualIndex == this->cursorRow) {
            renderer.WriteSingleLineText(1, tr, "->", arrowColor);
        }

        if (displayText.length() > this->leftViewCol) {
            std::string_view view = displayText;
            view                  = view.substr(this->leftViewCol);

            for (uint32 i = 0; i < view.length(); i++) {
                if (LEFT_MARGIN + i >= width)
                    break;

                uint32 absCol          = this->leftViewCol + i;
                ColorPair currentColor = textNormal;

                if (info.type == LineType::RuleContent)
                    currentColor = ruleBlockColor;
                else if (info.type == LineType::Info)
                    currentColor = infoColor;
                else if (info.type == LineType::Match)
                    currentColor = matchColor;
                else if (info.type == LineType::FileHeader) {
                    if (absCol >= checkboxStart && absCol < checkboxEnd) {
                        currentColor = checkboxColor; // [x]
                    }
                    // AICI E CORECTAT: Dacă arrowStart e valid și suntem după el
                    else if (arrowStart != -1 && absCol >= arrowStart) {
                        currentColor = foldColor; // > sau v
                    } else {
                        currentColor = headerColor; // Nume fișier
                    }
                }

                if (this->searchActive && realIndex == this->searchResultRealIndex) {
                    if (absCol >= this->searchResultStartCol && absCol < (this->searchResultStartCol + this->searchResultLen)) {
                        // Culoarea pentru textul găsit (ex: Negru pe Galben sau Teal)
                        currentColor = ColorPair{ Color::Black, Color::Yellow };
                    }
                }

                if (this->selectionActive) {
                    bool isSelected = false;
                    // Logica de selecție multi-line
                    if (visualIndex > selStartRow && visualIndex < selEndRow) {
                        isSelected = true; // Linie complet interioară
                    } else if (visualIndex == selStartRow && visualIndex == selEndRow) {
                        // Selecție pe o singură linie
                        if (absCol >= selStartCol && absCol < selEndCol)
                            isSelected = true;
                    } else if (visualIndex == selStartRow) {
                        // Prima linie a selecției
                        if (absCol >= selStartCol)
                            isSelected = true;
                    } else if (visualIndex == selEndRow) {
                        // Ultima linie a selecției
                        if (absCol < selEndCol)
                            isSelected = true;
                    }

                    if (isSelected) {
                        currentColor = textSelection;
                    }
                }

                // Cursor highlight (Prioritate maximă)
                if (visualIndex == this->cursorRow && absCol == this->cursorCol) {
                    currentColor = textCursor;
                }

                renderer.WriteCharacter(LEFT_MARGIN + i, tr, view[i], currentColor);
            }
            if (info.type == LineType::RuleContent) {
                int startFill = LEFT_MARGIN + (int) view.length();
                for (int k = startFill; k < (int) width; k++) {
                    // Verificăm dacă cursorul e pe spațiul gol
                    bool isCursorHere = (visualIndex == this->cursorRow && (this->leftViewCol + (k - LEFT_MARGIN)) == this->cursorCol);
                    renderer.WriteCharacter(k, tr, ' ', isCursorHere ? textCursor : ruleBlockColor);
                }
            }
        }
        if (visualIndex == this->cursorRow && this->cursorCol == displayText.length()) {
            if (this->cursorCol >= this->leftViewCol) {
                int screenX = (int) (this->cursorCol - this->leftViewCol) + LEFT_MARGIN;
                if (screenX < (int) width) {
                    renderer.WriteCharacter(screenX, tr, ' ', textCursor);
                }
            }
        }
       
    }
}

bool Instance::OnUpdateCommandBar(Application::CommandBar& commandBar)
{
    if (!yaraExecuted) {
        commandBar.SetCommand(Commands::YaraRunCommand.Key, Commands::YaraRunCommand.Caption, Commands::YaraRunCommand.CommandId);
    }
    commandBar.SetCommand(Commands::ViewRulesCommand.Key, Commands::ViewRulesCommand.Caption, Commands::ViewRulesCommand.CommandId);
    commandBar.SetCommand(Commands::EditRulesCommand.Key, Commands::EditRulesCommand.Caption, Commands::EditRulesCommand.CommandId);
    commandBar.SetCommand(Commands::SelectAllCommand.Key, "Select All", Commands::SelectAllCommand.CommandId);
    commandBar.SetCommand(Commands::DeselectAllCommand.Key, "Deselect All", Commands::DeselectAllCommand.CommandId);
    commandBar.SetCommand(Commands::FindTextCommand.Key, "Find Text", Commands::FindTextCommand.CommandId);

    if (isButtonFindPressed) {
        commandBar.SetCommand(Commands::FindNextCommand.Key, "Find Next", Commands::FindNextCommand.CommandId);
    }
    if (yaraExecuted) {
        commandBar.SetCommand(Commands::SaveReportCommand.Key, "Save Report", Commands::SaveReportCommand.CommandId);
    }
    return false;
}

bool Instance::UpdateKeys(KeyboardControlsInterface* interfaceParam)
{
    interfaceParam->RegisterKey(&Commands::YaraRunCommand);
    interfaceParam->RegisterKey(&Commands::ViewRulesCommand);
    interfaceParam->RegisterKey(&Commands::EditRulesCommand);
    interfaceParam->RegisterKey(&Commands::SelectAllCommand);
    interfaceParam->RegisterKey(&Commands::DeselectAllCommand);
    interfaceParam->RegisterKey(&Commands::FindTextCommand);
    interfaceParam->RegisterKey(&Commands::FindNextCommand);
    interfaceParam->RegisterKey(&Commands::SaveReportCommand);

    return true;
}

bool Instance::OnEvent(Reference<Control>, Event eventType, int ID)
{
    if (eventType != Event::Command)
        return false;

    if (ID == Commands::YaraRunCommand.CommandId) {
        auto result = AppCUI::Dialogs::MessageBox::ShowOkCancel("Run Yara", "Are you sure?");


        if (result == Dialogs::Result::Ok) {
        
            yaraExecuted = false;
            RunYara();
        }

    } else if (ID == Commands::ViewRulesCommand.CommandId) {
        yaraGetRulesFiles = false;
        yaraExecuted      = false;
        GetRulesFiles();

    } else if (ID == Commands::EditRulesCommand.CommandId) {

        yaraGetRulesFiles = false;
        GetRulesFiles();

        fs::path currentPath = fs::current_path();
        fs::path rootPath    = currentPath.parent_path().parent_path();
        fs::path yaraRules   = rootPath / "3rdPartyLibs" / "rules";

        if (!fs::exists(yaraRules)) {
            fs::create_directories(yaraRules);
        }

        // 1. Deschidem folderul
        ShellExecuteA(NULL, "explore", yaraRules.string().c_str(), NULL, NULL, SW_SHOWNORMAL);

        // 2. Afișăm un mesaj actualizat
        auto result = AppCUI::Dialogs::MessageBox::ShowOkCancel(
              "Edit Rules", "Rules folder opened.\n\nYou can Add, Edit or Delete .yara files now.\n\nClick [OK] when you are done to refresh the list.");

        // 3. Refresh automat la OK
        if (result == Dialogs::Result::Ok) {
            yaraGetRulesFiles = false;
            GetRulesFiles();
        }
    } else if (ID == Commands::SelectAllCommand.CommandId) {
        SelectAllRules();
        return true;
    } else if (ID == Commands::DeselectAllCommand.CommandId) {
        DeselectAllRules();
        return true;
    } else if (ID == Commands::FindNextCommand.CommandId) {
        FindNext();
        return true;
    } else if (ID == Commands::SaveReportCommand.CommandId) {
        ExportResults();
        return true;
    } else if (ID == Commands::FindTextCommand.CommandId) {
        ShowFindDialog();
        return true;
    }

    return false;
}

void Instance::OnAfterResize(int newWidth, int newHeight)
{
    layout.visibleLines = GetHeight();
    if (layout.visibleLines > 0)
        layout.visibleLines--;
    layout.maxCharactersPerLine = GetWidth() - startingX /*left*/ - startingX /*right*/;
}

bool Instance::OnKeyEvent(AppCUI::Input::Key keyCode, char16 charCode)
{
    if (keyCode == (Key::Ctrl | Key::C)) {
        CopySelectionToClipboard();
        return true;
    }

    // Tasta SPACE pentru a bifa/debifa reguli
    if (keyCode == Key::Space) {
        if (cursorRow < visibleIndices.size()) {
            size_t realIndex = visibleIndices[cursorRow];
            if (yaraLines[realIndex].type == LineType::FileHeader) {
                // Schimbăm selecția
                yaraLines[realIndex].isChecked = !yaraLines[realIndex].isChecked;
            }
        }
        return true;
    }

    if (keyCode == Key::Enter) {
        if (cursorRow < visibleIndices.size()) {
            size_t realIndex = visibleIndices[cursorRow];
            if (yaraLines[realIndex].type == LineType::FileHeader) {
                ToggleFold(realIndex);
            }
        }
        return true;
    }

    if (keyCode == Key::Escape) {
        this->selectionActive = false; 
        this->searchActive    = false; 
        this->isButtonFindPressed = false;
        this->SetFocus();
        this->lastSearchText.clear();  
        return true;
    }

    switch (keyCode) {
    case Key::Right:
        if (cursorCol < yaraLines[cursorRow].text.length()) { // Atenție: text.length() nu length() direct
            cursorCol++;
        } else if (cursorRow + 1 < yaraLines.size()) {
            cursorRow++;
            cursorCol = 0;
        }
        MoveTo();
        return true;

    case Key::Left:
        if (cursorCol > 0)
            cursorCol--;
        else if (cursorRow > 0) {
            cursorRow--;
            cursorCol = (uint32) yaraLines[cursorRow].text.length();
        }
        MoveTo();
        return true;

    case Key::Down:
        if (cursorRow + 1 < visibleIndices.size()) {
            cursorRow++;
            if (cursorCol > yaraLines[cursorRow].text.length())
                cursorCol = (uint32) yaraLines[cursorRow].text.length();
        }
        MoveTo();
        return true;

    case Key::Up:
        if (cursorRow > 0) {
            cursorRow--;
            if (cursorCol > yaraLines[cursorRow].text.length())
                cursorCol = (uint32) yaraLines[cursorRow].text.length();
        }
        MoveTo();
        return true;
        // Home/End la fel, folosind yaraLines[cursorRow].text.length()
    }
    return false;
}

void Instance::MoveTo()
{
    uint32 height         = this->GetHeight();
    uint32 width          = this->GetWidth();
    const int LEFT_MARGIN = 4;

    uint32 totalVisible = (uint32) visibleIndices.size();

    // Spațiul disponibil pentru text
    uint32 textWidth = (width > LEFT_MARGIN) ? (width - LEFT_MARGIN) : 1;

    // 1. SCROLL VERTICAL (Rânduri)
    if (this->cursorRow < this->startViewLine) {
        this->startViewLine = this->cursorRow;
    } else if (this->cursorRow >= this->startViewLine + height) {
        this->startViewLine = this->cursorRow - height + 1;
    }

    if (this->startViewLine > totalVisible)
        this->startViewLine = 0;

    // 2. SCROLL ORIZONTAL (Coloane)
    if (this->cursorCol < this->leftViewCol) {
        this->leftViewCol = this->cursorCol;
    } else if (this->cursorCol >= this->leftViewCol + textWidth) {
        this->leftViewCol = this->cursorCol - textWidth + 1;
    }
}

bool Instance::OnMouseWheel(int x, int y, AppCUI::Input::MouseWheel direction, AppCUI::Input::Key key)
{
    switch (direction) {
    case MouseWheel::Up:
        // Când dai de rotiță în sus, simulăm tasta Săgeată Sus
        return OnKeyEvent(Key::Up, false);

    case MouseWheel::Down:
        // Când dai de rotiță în jos, simulăm tasta Săgeată Jos
        return OnKeyEvent(Key::Down, false);

    case MouseWheel::Left:
        // Scroll orizontal stânga (dacă ai mouse cu tilt sau touchpad)
        return OnKeyEvent(Key::Left, false);

    case MouseWheel::Right:
        // Scroll orizontal dreapta
        return OnKeyEvent(Key::Right, false);
    }

    return false;
}

void Instance::ComputeMouseCoords(int x, int y, uint32 startViewLine, uint32 leftViewCol, const std::vector<LineInfo>& lines, uint32& outRow, uint32& outCol)
{
    const int LEFT_MARGIN = 4;

    // 1. Calculăm Rândul Vizual
    uint32 r = startViewLine + y;

    // Validăm limitele vizuale
    if (visibleIndices.empty()) {
        r = 0;
    } else if (r >= visibleIndices.size()) {
        r = (uint32) visibleIndices.size() - 1;
    }
    outRow = r;

    // 2. Calculăm Coloana Vizuală
    int val = (int) leftViewCol + (x - LEFT_MARGIN);
    if (val < 0)
        val = 0;
    uint32 c = (uint32) val;

    // 3. Limităm coloana la lungimea textului de pe ecran
    if (!visibleIndices.empty()) {
        size_t realIndex = visibleIndices[outRow]; // Mapăm vizual -> real
        const auto& line = lines[realIndex];

        // Calculăm lungimea vizuală a textului
        size_t displayLen = line.text.length();

        if (line.type == LineType::FileHeader) {
            displayLen += 6; // 4 caractere "[ ] " + 2 caractere " >"
        } else if (line.indentLevel > 0) {
            displayLen += 4; // 4 spații indentare
        }

        if (c > displayLen)
            c = (uint32) displayLen;
    } else {
        c = 0;
    }

    outCol = c;
}

void Instance::OnMousePressed(int x, int y, AppCUI::Input::MouseButton button, AppCUI::Input::Key keyCode)
{
    if ((button & MouseButton::Left) == MouseButton::None)
        return;
    if (visibleIndices.empty() || yaraLines.empty())
        return;

    // 1. Calculăm rândul vizual
    uint32 clickVisualRow = this->startViewLine + y;
    if (clickVisualRow >= visibleIndices.size())
        return;

    // 2. Calculăm coordonatele exacte (Rând și Coloană)
    // Această funcție va actualiza this->cursorRow și this->cursorCol
    ComputeMouseCoords(x, y, this->startViewLine, this->leftViewCol, this->yaraLines, this->cursorRow, this->cursorCol);

    // 3. --- FIXUL ESTE AICI ---
    // Resetăm ancora de selecție la poziția curentă a cursorului
    this->selectionAnchorRow = this->cursorRow;
    this->selectionAnchorCol = this->cursorCol; 
    this->selectionActive    = false;

    // 4. Logica pentru Header (Checkbox / Fold)
    size_t realIndex = visibleIndices[clickVisualRow];
    LineInfo& line   = yaraLines[realIndex];

    // Calculăm X relativ la începutul textului (pentru detecție click pe elemente grafice)
    int relX = (x - 4) + this->leftViewCol;

    if (line.type == LineType::FileHeader) {
        // Zona Checkbox: "[ ]" (index 0..3)
        if (relX >= 0 && relX <= 2) {
            line.isChecked = !line.isChecked;
            return;
        }

        // Zona Săgeată: la finalul textului
        int arrowStart = 4 + (int) line.text.length(); // 4 chars prefix + lungime nume
        if (relX >= arrowStart && relX <= arrowStart + 1) {
            ToggleFold(realIndex);
            return;
        }
    }

    MoveTo();
}


bool Instance::OnMouseDrag(int x, int y, AppCUI::Input::MouseButton button, AppCUI::Input::Key keyCode)
{
    if ((button & MouseButton::Left) == MouseButton::None)
        return false;
    if (yaraLines.empty())
        return false;

    ComputeMouseCoords(x, y, this->startViewLine, this->leftViewCol, this->yaraLines, this->cursorRow, this->cursorCol);

    if (this->cursorRow != this->selectionAnchorRow || this->cursorCol != this->selectionAnchorCol) {
        this->selectionActive = true;
    }
    MoveTo();
    return true;
}

void Instance::CopySelectionToClipboard()
{
    // Variabila în care vom construi textul
    std::string textToCopy;

    // 1. ZONA CRITICĂ (Citim datele protejat)
    {
        // Dacă ai definit mutex-ul în header, decomentează linia de mai jos:
        // std::lock_guard<std::mutex> lock(this->linesMutex);

        if (!this->selectionActive || this->yaraLines.empty())
            return;

        // a) Ordonăm coordonatele (Start trebuie să fie înainte de End)
        uint32 r1 = this->cursorRow;
        uint32 c1 = this->cursorCol;
        uint32 r2 = this->selectionAnchorRow;
        uint32 c2 = this->selectionAnchorCol;

        // Dacă cursorul e deasupra ancorei, facem swap
        if (r1 > r2 || (r1 == r2 && c1 > c2)) {
            std::swap(r1, r2);
            std::swap(c1, c2);
        }

        // b) Iterăm prin liniile selectate
        for (uint32 r = r1; r <= r2; r++) {
            if (r >= this->yaraLines.size())
                break;

            // --- MODIFICAREA PRINCIPALĂ ESTE AICI ---
            // Accesăm câmpul .text din structura LineInfo
            const std::string& line = this->yaraLines[r].text;
            // ----------------------------------------

            // Calculăm indecșii de tăiere pentru linia curentă
            uint32 start = (r == r1) ? c1 : 0;
            uint32 end   = (r == r2) ? c2 : (uint32) line.length();

            // Validări de siguranță (să nu crape substr)
            if (start > line.length())
                start = (uint32) line.length();
            if (end > line.length())
                end = (uint32) line.length();

            if (end > start) {
                textToCopy += line.substr(start, end - start);
            }

            // Adăugăm NewLine dacă nu suntem la ultima linie
            if (r < r2) {
                textToCopy += "\r\n"; // Format standard Windows
            }
        }
    }
    // AICI se termină lock-ul.

    // 2. TRIMITEM LA CLIPBOARD
    if (!textToCopy.empty()) {
        AppCUI::OS::Clipboard::SetText(textToCopy);
    }
}

void Instance::ToggleSelection()
{
    if (this->cursorRow < yaraLines.size()) {
        if (yaraLines[this->cursorRow].type == LineType::FileHeader) {
            yaraLines[this->cursorRow].isChecked = !yaraLines[this->cursorRow].isChecked;
        }
    }
}

void Instance::UpdateVisibleIndices()
{
    visibleIndices.clear();
    for (size_t i = 0; i < yaraLines.size(); i++) {
        if (yaraLines[i].isVisible) {
            visibleIndices.push_back(i);
        }
    }
}

void Instance::ToggleFold(size_t index)
{
    if (index >= yaraLines.size())
        return;

    if (yaraLines[index].type == LineType::FileHeader) {
        // Inversăm starea
        bool newState               = !yaraLines[index].isExpanded;
        yaraLines[index].isExpanded = newState;

        // Iterăm în jos
        for (size_t i = index + 1; i < yaraLines.size(); i++) {
            if (yaraLines[i].type == LineType::FileHeader || yaraLines[i].type == LineType::Info || yaraLines[i].type == LineType::Match) {
                break;
            }

            // --- AICI E SCHIMBAREA ---
            // Ascundem/Arătăm DOAR conținutul efectiv al regulii
            if (yaraLines[i].type == LineType::RuleContent) {
                yaraLines[i].isVisible = newState;
            }
            // Liniile 'Normal' (spații goale, separatoare) rămân vizibile sau le legăm și pe ele de header
            // Dacă vrei să ascunzi TOT ce e sub header până la următorul, scoate 'if'-ul de mai sus
        }
        UpdateVisibleIndices();
    }
}

void Instance::SelectAllRules()
{
    bool changesMade = false;
    for (auto& line : yaraLines) {
        if (line.type == LineType::FileHeader) {
            if (!line.isChecked) {
                line.isChecked = true;
                changesMade    = true;
            }
        }
    }
  
}

void Instance::DeselectAllRules()
{
    bool changesMade = false;
    for (auto& line : yaraLines) {
        if (line.type == LineType::FileHeader) {
            if (line.isChecked) {
                line.isChecked = false;
                changesMade    = true;
            }
        }
    }
}


void Instance::GetRulesFiles()
{
    if (yaraGetRulesFiles)
        return;

    // ---------------------------------------------------------
    // RESETARE INTERFATA
    // ---------------------------------------------------------
    this->cursorRow          = 0;
    this->cursorCol          = 0;
    this->startViewLine      = 0;
    this->leftViewCol        = 0;
    this->selectionActive    = false;
    this->selectionAnchorRow = 0;
    this->selectionAnchorCol = 0;

    yaraLines.clear();

    fs::path currentPath = fs::current_path();
    fs::path rootPath    = currentPath.parent_path().parent_path();
    fs::path yaraRules   = rootPath / "3rdPartyLibs" / "rules";

    if (!fs::exists(yaraRules) || !fs::is_directory(yaraRules))
        return;

    // Header info
    yaraLines.push_back({ "=== YARA RULES SELECTION ===", LineType::Normal });
    yaraLines.push_back({ "", LineType::Normal });
    yaraLines.push_back({ "Controls:", LineType::Match });
    yaraLines.push_back({ "   [Space] or [Click] : Toggle selection", LineType::Normal });
    yaraLines.push_back({ "   [Ctrl + A]         : Select All", LineType::Normal });
    yaraLines.push_back({ "   [Ctrl + D]         : Deselect All", LineType::Normal });
    yaraLines.push_back({ "   [F6] Run | [F7] View Rules | [F8] Edit", LineType::Normal });

    // --- 2. DELIMITARE ---
    yaraLines.push_back({ "_________________________________________", LineType::Normal });
    yaraLines.push_back({ "", LineType::Normal });

    // --- 3. TITLU LISTĂ ---
    yaraLines.push_back({ "Rules list:", LineType::Info }); // Folosesc Info ca să fie galben (opțional) sau Normal
    yaraLines.push_back({ "", LineType::Normal });

    for (auto& entry : fs::directory_iterator(yaraRules)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();

            // Adăugăm HEADER-ul fișierului (care va avea checkbox)
            LineInfo header;
            header.text       = filename;
            header.type       = LineType::FileHeader;
            header.isChecked  = false;
            header.filePath   = entry.path();
            header.isExpanded = false; // <--- START PLIAT
            header.isVisible  = true;
            yaraLines.push_back(header);

            std::ifstream in(entry.path());
            if (in.is_open()) {
                std::string line;
                while (std::getline(in, line)) {
                    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                        line.pop_back();

                    // CONȚINUT (Invizibil la start)
                    LineInfo content;
                    content.text        = line;
                    content.type        = LineType::RuleContent;
                    content.isVisible   = false; // <--- ASCUNS PÂNĂ DAI ENTER
                    content.indentLevel = 1;
                    yaraLines.push_back(content);
                }
            }
            yaraLines.push_back({ "", LineType::Normal });
        }
    }
    UpdateVisibleIndices();
    yaraGetRulesFiles = true;
}

void Instance::RunYara()
{
    // ---------------------------------------------------------
    // 1. SELECTIE REGULI
    // ---------------------------------------------------------
    std::vector<fs::path> selectedRules;
    for (const auto& line : yaraLines) {
        if (line.type == LineType::FileHeader && line.isChecked) {
            selectedRules.push_back(line.filePath);
        }
    }

    if (selectedRules.empty()) {
        AppCUI::Dialogs::MessageBox::ShowError("Error", "No rules selected!");
        return;
    }

    // ---------------------------------------------------------
    // 2. SETUP PATH-URI
    // ---------------------------------------------------------
    fs::path currentPath = fs::current_path();
    fs::path rootPath    = currentPath.parent_path().parent_path();
    fs::path yaraExe     = rootPath / "3rdPartyLibs" / "yara-win64" / "yara64.exe";
    fs::path outputFile  = rootPath / "3rdPartyLibs" / "output" / "output.txt";
    fs::path currentFile = this->obj->GetPath();

    fs::path outputDir = outputFile.parent_path();
    if (!fs::exists(outputDir))
        fs::create_directories(outputDir);

    if (!fs::exists(yaraExe)) {
        AppCUI::Dialogs::MessageBox::ShowError("Error", "Yara exe not found!");
        return;
    }

    // ---------------------------------------------------------
    // 3. RESETARE INTERFATA
    // ---------------------------------------------------------
    this->cursorRow     = 0;
    this->cursorCol     = 0;
    this->startViewLine = 0;
    this->leftViewCol   = 0;
    this->selectionActive    = false;
    this->selectionAnchorRow = 0;
    this->selectionAnchorCol = 0;

    yaraLines.clear();

    // Header temporar (va fi completat la final cu Summary)
    yaraLines.push_back({ "=== SCANNING RESULTS ===", LineType::Normal });
    yaraLines.push_back({ "", LineType::Normal });
    yaraLines.push_back({ "scanning...", LineType::Normal }); // Placeholder pt Summary

    int globalMatchCount = 0;

    // ---------------------------------------------------------
    // 4. EXECUTIE PER REGULA
    // ---------------------------------------------------------
    for (const auto& rulePath : selectedRules) {
        std::string ruleName = rulePath.filename().string();

        // Separator vizual între reguli
        yaraLines.push_back({ "-------------------------------------------------------------", LineType::Normal });
        yaraLines.push_back({ "Scanning with rule: " + ruleName, LineType::Info }); // Folosim FileHeader ca să apară colorat (chiar dacă nu e checkbox)

        // Comanda YARA
        std::string cmdArgs = "/C \"\"" + yaraExe.string() +
                              "\" -g -m -s -r "
                              "\"" +
                              rulePath.string() +
                              "\" "
                              "\"" +
                              currentFile.string() +
                              "\" "
                              "> \"" +
                              outputFile.string() + "\"\"";

        SHELLEXECUTEINFOA shExecInfo{};
        shExecInfo.cbSize       = sizeof(SHELLEXECUTEINFOA);
        shExecInfo.fMask        = SEE_MASK_NOCLOSEPROCESS;
        shExecInfo.hwnd         = nullptr;
        shExecInfo.lpVerb       = "open";
        shExecInfo.lpFile       = "cmd.exe";
        shExecInfo.lpParameters = cmdArgs.c_str();
        shExecInfo.nShow        = SW_HIDE;

        if (ShellExecuteExA(&shExecInfo)) {
            WaitForSingleObject(shExecInfo.hProcess, INFINITE);
            CloseHandle(shExecInfo.hProcess);

            // ---------------------------------------------------------
            // 5. PARSARE REZULTATE (Logica ta veche)
            // ---------------------------------------------------------
            std::ifstream in(outputFile);
            if (!in.is_open()) {
                yaraLines.push_back({ "    [ERROR] Cannot open output file.", LineType::Normal });
                continue;
            }

            std::string line;
            std::vector<std::string> matchedStrings;
            std::string currentRule, currentTags, currentAuthor, currentSeverity;

            bool foundAnyInFile = false;

            while (std::getline(in, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                    line.pop_back();
                if (line.empty())
                    continue;

                foundAnyInFile = true;

                // Detectăm linia header YARA: "rule_name [tags] [meta] path"
                if (line.find(currentFile.string()) != std::string::npos && line.find('[') != std::string::npos && line.find(']') != std::string::npos) {
                    // === DUMP REGULA PRECEDENTĂ (Dacă există) ===
                    if (!currentRule.empty()) {
                        globalMatchCount++;
                        yaraLines.push_back({ "    [MATCH] Rule: " + currentRule, LineType::Match });
                        if (!currentTags.empty())
                        yaraLines.push_back({ "    Tags: " + currentTags, LineType::Normal });
                        if (!currentAuthor.empty())
                            yaraLines.push_back({ "    Author: " + currentAuthor, LineType::Normal });
                        if (!currentSeverity.empty())
                            yaraLines.push_back({ "    Severity: " + currentSeverity, LineType::Normal });

                        // Strings & Hex Context
                        for (auto& s : matchedStrings) {
                            yaraLines.push_back({ "        " + s, LineType::RuleContent }); // Fundal verde
                            yaraLines.push_back({ "        At:", LineType::Normal });

                            // Apelează funcția ta de extragere
                            auto ctx = ExtractHexContextFromYaraMatch(s, currentFile.string());
                            for (auto& ctxLine : ctx)
                                yaraLines.push_back({ "                " + ctxLine, LineType::Normal });
                        }
                        yaraLines.push_back({ "", LineType::Normal });
                    }

                    // === RESETARE PENTRU REGULA NOUĂ ===
                    matchedStrings.clear();
                    currentAuthor.clear();
                    currentSeverity.clear();
                    currentTags.clear();

                    // === PARSARE HEADER NOU ===
                    // 1. Rule Name
                    size_t spacePos = line.find(' ');
                    currentRule     = line.substr(0, spacePos);

                    // 2. Tags
                    size_t firstBracketStart = line.find('[');
                    size_t firstBracketEnd   = line.find(']');
                    if (firstBracketStart != std::string::npos && firstBracketEnd != std::string::npos)
                        currentTags = line.substr(firstBracketStart + 1, firstBracketEnd - firstBracketStart - 1);

                    // 3. Meta (Author, Severity)
                    size_t metaStart = line.find('[', firstBracketEnd + 1);
                    size_t metaEnd   = line.find(']', metaStart);
                    if (metaStart != std::string::npos && metaEnd != std::string::npos) {
                        std::string metaStr = line.substr(metaStart + 1, metaEnd - metaStart - 1);

                        auto extractMeta = [&](const std::string& key) -> std::string {
                            size_t keyPos = metaStr.find(key + "=\"");
                            if (keyPos != std::string::npos) {
                                size_t start = keyPos + key.length() + 2;
                                size_t end   = metaStr.find("\"", start);
                                if (end != std::string::npos)
                                    return metaStr.substr(start, end - start);
                            }
                            return "";
                        };

                        currentAuthor   = extractMeta("author");
                        currentSeverity = extractMeta("severity");
                    }
                } else {
                    // Este o linie cu un string ($s1: ...)
                    matchedStrings.push_back(line);
                }
            }
            in.close();

            // === DUMP ULTIMA REGULĂ DIN FIȘIER ===
            if (!currentRule.empty()) {
                globalMatchCount++;
                yaraLines.push_back({ "    [MATCH] Rule: " + currentRule, LineType::Normal });
                if (!currentTags.empty())
                yaraLines.push_back({ "    Tags: " + currentTags, LineType::Normal });
                if (!currentAuthor.empty())
                    yaraLines.push_back({ "    Author: " + currentAuthor, LineType::Normal });
                if (!currentSeverity.empty())
                    yaraLines.push_back({ "    Severity: " + currentSeverity, LineType::Normal });

                for (auto& s : matchedStrings) {
                    yaraLines.push_back({ "        " + s, LineType::RuleContent });
                    yaraLines.push_back({ "        At:", LineType::Normal });

                    auto ctx = ExtractHexContextFromYaraMatch(s, currentFile.string());
                    for (auto& ctxLine : ctx)
                        yaraLines.push_back({ "                " + ctxLine, LineType::Normal });
                }
                yaraLines.push_back({ "", LineType::Normal });
            } else if (!foundAnyInFile) {
                // Dacă nu a găsit nimic în acest fișier de reguli
                yaraLines.push_back({ "    [CLEAN] No matches for this rule.", LineType::Normal });
            }

        } else {
            yaraLines.push_back({ "    [ERROR] Execution failed.", LineType::Normal });
        }
    }

    // ---------------------------------------------------------
    // 6. SUMAR FINAL (Inserat la început)
    // ---------------------------------------------------------
    // Modificăm liniile rezervate la început
    yaraLines[2].text = "Summary:";
    yaraLines.insert(yaraLines.begin() + 3, { "    Total Matches: " + std::to_string(globalMatchCount), LineType::Normal });
    yaraLines.insert(yaraLines.begin() + 4, { globalMatchCount > 0 ? "    Status: INFECTED" : "    Status: CLEAN", LineType::Normal });
    yaraLines.insert(yaraLines.begin() + 5, { "", LineType::Normal });

    yaraLines.push_back({ "=== SCAN COMPLETE ===", LineType::Normal });

    yaraExecuted      = true;
    yaraGetRulesFiles = true; // Previne reîncărcarea automată
    UpdateVisibleIndices();
}


std::vector<std::string> Instance::ExtractHexContextFromYaraMatch(
      const std::string& yaraLine,
      const std::string& exePath,
      size_t contextSize // bytes before & after
    )
{
    std::vector<std::string> output;

    // 1️ Extragem offset-ul (0x....)
    size_t pos = yaraLine.find(':');
    if (pos == std::string::npos)
        return output;

    std::string offsetStr = yaraLine.substr(0, pos);
    uint64_t offset       = 0;
    try {
        offset = std::stoull(offsetStr, nullptr, 16);
    } catch (...) {
        output.push_back("EROARE: Offset invalid în linia YARA.");
        return output;
    }

    // 2️ Deschidem fișierul EXE
    std::ifstream file(exePath, std::ios::binary);
    if (!file.is_open()) {
        output.push_back("EROARE: Nu pot deschide fișierul.");
        return output;
    }

    // 3️ Calculăm zona de citire
    uint64_t startOffset = (offset > contextSize) ? offset - contextSize : 0;
    size_t readSize      = contextSize * 2;

    file.seekg(startOffset, std::ios::beg);

    std::vector<uint8_t> buffer(readSize);
    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    size_t bytesRead = file.gcount();

    // 4 Format HEX + ASCII (16 bytes pe linie)
    size_t lineSize = 16;
    for (size_t i = 0; i < bytesRead; i += lineSize) {
        std::ostringstream line;
        std::ostringstream ascii;

        // hex și ascii
        for (size_t j = 0; j < lineSize; j++) {
            if (i + j < bytesRead) {
                uint8_t b = buffer[i + j];
                line << std::setw(2) << std::setfill('0') << std::hex << std::uppercase << static_cast<int>(b) << ' ';
                ascii << (std::isprint(b) ? static_cast<char>(b) : '.');
            } else {
                line << "   "; // padding pentru ultima linie
            }
        }

        std::ostringstream fullLine;
        fullLine << "0x" << std::setw(8) << std::setfill('0') << std::hex << (startOffset + i) << "  " << line.str() << " " << ascii.str();

        output.push_back(fullLine.str());
    }

    // 5️ Header cu offset original
    std::ostringstream header;
    header << "File offset: 0x" << std::hex << std::uppercase << offset;


    std::ostringstream section;
    std::string sectionName = GetSectionFromOffset(exePath, offset);
    section << "Section: " << sectionName;

    output.insert(output.begin(), header.str());
    output.insert(output.begin(), section.str());
    return output;
}

std::string Instance::GetSectionFromOffset(const std::string& exePath, uint64_t offset)
{
    std::ifstream file(exePath, std::ios::binary);
    if (!file)
        return "UNKNOWN";

    IMAGE_DOS_HEADER dos{};
    file.read(reinterpret_cast<char*>(&dos), sizeof(dos));

    if (dos.e_magic != IMAGE_DOS_SIGNATURE)
        return "NOT_A_PE";

    file.seekg(dos.e_lfanew, std::ios::beg);

    IMAGE_NT_HEADERS nt{};
    file.read(reinterpret_cast<char*>(&nt), sizeof(nt));

    IMAGE_SECTION_HEADER section{};

    for (int i = 0; i < nt.FileHeader.NumberOfSections; i++) {
        file.read(reinterpret_cast<char*>(&section), sizeof(section));

        DWORD start = section.PointerToRawData;
        DWORD end   = start + section.SizeOfRawData;

        if (offset >= start && offset < end) {
            return std::string(reinterpret_cast<char*>(section.Name), strnlen(reinterpret_cast<char*>(section.Name), 8));
        }
    }

    return "UNKNOWN";
}

void Instance::ExportResults()
{
    // 1. Verificăm dacă avem ce salva
    if (yaraLines.empty()) {
        AppCUI::Dialogs::MessageBox::ShowError("Error", "No scan results to save!");
        return;
    }

    // 2. ÎNTREBAREA DE CONFIRMARE (Acesta este pasul cerut)
    auto response = AppCUI::Dialogs::MessageBox::ShowOkCancel("Save Report", "Do you want to save the scan results to a text file?");

    if (response != Dialogs::Result::Ok) {
        return; // Dacă userul dă Cancel sau X, ieșim și nu salvăm nimic.
    }

    // 3. Generăm numele cu data curentă
    auto now       = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S");
    std::string fileName = "results_" + ss.str() + ".txt";

    // 4. Pregătim calea: root/3rdPartyLibs/results/
    fs::path currentPath = fs::current_path();
    fs::path rootPath    = currentPath.parent_path().parent_path();
    fs::path resultsDir  = rootPath / "3rdPartyLibs" / "results";

    if (!fs::exists(resultsDir)) {
        fs::create_directories(resultsDir);
    }

    fs::path fullPath = resultsDir / fileName;

    // 5. Scrierea efectivă
    std::ofstream out(fullPath);
    if (!out.is_open()) {
        AppCUI::Dialogs::MessageBox::ShowError("Error", "Could not create file!");
        return;
    }

    for (const auto& line : yaraLines) {
        // Păstrăm indentarea
        for (int i = 0; i < line.indentLevel; i++)
            out << "\t";

        // Păstrăm checkbox-ul vizual
        if (line.type == LineType::FileHeader) {
            out << (line.isChecked ? "[x] " : "[ ] ");
        }

        out << line.text << "\n";
    }
    out.close();

    // 6. Notificare finală și deschidere folder
    AppCUI::Dialogs::MessageBox::ShowNotification("Saved", "Report saved successfully!\nOpening folder...");
    ShellExecuteA(NULL, "explore", resultsDir.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
}