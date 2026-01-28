#include "YaraViewer.hpp"
#include <algorithm>
#include <cstdlib> // pentru std::system
#include <iostream>
#include <fstream>   // pentru std::ifstream
#include <string>    // pentru std::string
#include <codecvt>
#include <locale>

#include <windows.h>
#include <shellapi.h>
#undef MessageBox
//#define MessageBox MessageBoxW 


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

bool Instance::ShowFindDialog()
{
    NOT_IMPLEMENTED(false)
}

bool Instance::ShowCopyDialog()
{
    NOT_IMPLEMENTED(false)
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


std::string_view Instance::GetCategoryNameForSerialization() const
{
    return "YaraViewer";
}

bool Instance::AddCategoryBeforePropertyNameWhenSerializing() const
{
    return true;
}

void Instance::Paint(Graphics::Renderer& renderer)
{
    ColorPair textNormal  = ColorPair{ Color::White, Color::Transparent };
    ColorPair textCursor  = ColorPair{ Color::Black, Color::White };
    ColorPair arrowColor  = ColorPair{ Color::Green, Color::Transparent };
    ColorPair marginColor = ColorPair{ Color::Gray, Color::Transparent };

    uint32 rows           = this->GetHeight();
    uint32 width          = this->GetWidth();
    const int LEFT_MARGIN = 4;

    GetRulesFiles();


    // Iterăm prin rândurile de pe ecran
    for (uint32 tr = 0; tr < rows; tr++) {
        // Calculăm ce linie din vector trebuie afișată
        uint32 lineIndex = this->startViewLine + tr;

        // Dacă am depășit numărul de linii din vector, ne oprim
        if (lineIndex >= yaraOutput.size())
            break;

        // Referință la linia curentă
        const std::string& currentLineStr = yaraOutput[lineIndex];

        // 1. Desenăm Săgeata și Marginea
        renderer.WriteSpecialCharacter(LEFT_MARGIN - 1, tr, SpecialChars::BoxVerticalSingleLine, marginColor);
        if (lineIndex == this->cursorRow) {
            renderer.WriteSingleLineText(1, tr, "->", arrowColor);
        }

        // 2. Calculăm ce porțiune din text se vede (Scroll Orizontal)
        if (currentLineStr.length() > this->leftViewCol) {
            std::string_view view = currentLineStr;
            // Tăiem partea din stânga care nu se vede
            view = view.substr(this->leftViewCol);

            // Desenăm caracter cu caracter pentru a trata cursorul
            for (uint32 i = 0; i < view.length(); i++) {
                // Dacă depășim lățimea ecranului, ne oprim
                if (LEFT_MARGIN + i >= width)
                    break;

                // Verificăm dacă cursorul e pe acest caracter
                // Cursorul e pe linia curentă ȘI la coloana curentă (leftViewCol + i)
                bool isCursor = (lineIndex == this->cursorRow) && ((this->leftViewCol + i) == this->cursorCol);

                renderer.WriteCharacter(LEFT_MARGIN + i, tr, view[i], isCursor ? textCursor : textNormal);
            }
        }

        // 3. Desenăm cursorul dacă e la finalul liniei sau pe o linie goală
        // (Când cursorCol este egal cu lungimea liniei)
        if (lineIndex == this->cursorRow && this->cursorCol == currentLineStr.length()) {
            // Verificăm dacă poziția cursorului este vizibilă pe ecran
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
    commandBar.SetCommand(Commands::SomeCommand.Key, Commands::SomeCommand.Caption, Commands::SomeCommand.CommandId);
    return false;
}

bool Instance::UpdateKeys(KeyboardControlsInterface* interfaceParam)
{
    interfaceParam->RegisterKey(&Commands::SomeCommand);
    return true;
}

bool Instance::OnEvent(Reference<Control>, Event eventType, int ID)
{
    if (eventType != Event::Command)
        return false;

    if (ID == Commands::SomeCommand.CommandId) {
        auto result = AppCUI::Dialogs::MessageBox::ShowOkCancel("Run Yara", "Are you sure?");


        if (result == Dialogs::Result::Ok) {
        
            RunYara();
        }
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
    switch (keyCode) {
    case Key::Right:
        // Putem merge până la lungimea liniei (pentru a putea adăuga text teoretic)
        if (cursorCol < yaraOutput[cursorRow].length()) {
            cursorCol++;
        } else if (cursorRow + 1 < yaraOutput.size()) {
            // Dacă dăm dreapta la final de linie, mergem la începutul următoarei
            cursorRow++;
            cursorCol = 0;
        }
        MoveTo();
        return true;

    case Key::Left:
        if (cursorCol > 0) {
            cursorCol--;
        } else if (cursorRow > 0) {
            // Dacă dăm stânga la început de linie, mergem la finalul anterioarei
            cursorRow--;
            cursorCol = (uint32) yaraOutput[cursorRow].length();
        }
        MoveTo();
        return true;

    case Key::Down:
        if (cursorRow + 1 < yaraOutput.size()) {
            cursorRow++;
            // Dacă linia nouă e mai scurtă, mutăm cursorul la finalul ei
            if (cursorCol > yaraOutput[cursorRow].length())
                cursorCol = (uint32) yaraOutput[cursorRow].length();
        }
        MoveTo();
        return true;

    case Key::Up:
        if (cursorRow > 0) {
            cursorRow--;
            // Dacă linia nouă e mai scurtă, mutăm cursorul la finalul ei
            if (cursorCol > yaraOutput[cursorRow].length())
                cursorCol = (uint32) yaraOutput[cursorRow].length();
        }
        MoveTo();
        return true;

    case Key::Home:
        cursorCol = 0;
        MoveTo();
        return true;

    case Key::End:
        cursorCol = (uint32) yaraOutput[cursorRow].length();
        MoveTo();
        return true;
    }
    return false;
}

void Instance::MoveTo()
{
    uint32 height         = this->GetHeight();
    uint32 width          = this->GetWidth();
    const int LEFT_MARGIN = 4;

    // Spațiul disponibil pentru text
    uint32 textWidth = (width > LEFT_MARGIN) ? (width - LEFT_MARGIN) : 1;

    // 1. SCROLL VERTICAL (Rânduri)
    if (this->cursorRow < this->startViewLine) {
        this->startViewLine = this->cursorRow;
    } else if (this->cursorRow >= this->startViewLine + height) {
        this->startViewLine = this->cursorRow - height + 1;
    }

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

void Instance::GetRulesFiles()
{
    if (yaraGetRulesFiles)
        return; // deja executat


     fs::path currentPath = fs::current_path();
     fs::path rootPath    = currentPath.parent_path().parent_path();
     fs::path yaraRules = rootPath / "3rdPartyLibs" / "rules";

     if (!fs::exists(yaraRules) || !fs::is_directory(yaraRules)) {
         std::cout << "Folderul " << yaraRules.string() << " nu există.\n";
         return;
     }

     yaraOutput = { 
         "=== YARA RULES ===", 
         "",
         
         
         "Rules path: ",
         "    " + yaraRules.string(),
         "Rules files: "
     };

     for (auto& entry : fs::directory_iterator(yaraRules)) {
         if (entry.is_regular_file()) // luăm doar fișiere, ignorăm foldere
         {
             std::string filename = entry.path().filename().string();

             yaraOutput.push_back("    " + filename); // 4 spații în față
             yaraOutput.push_back("      ______________________________________");
             // deschidem fișierul pentru citire
             std::ifstream in(entry.path());
             if (!in.is_open()) {
                 yaraOutput.push_back("        [Eroare la citirea fișierului]");
                 continue;
             }

             std::string line;
             while (std::getline(in, line)) {
                 // eliminăm eventuale carriage return / newline
                 while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                     line.pop_back();

                 // afișăm fiecare linie cu indentare suplimentară
                 yaraOutput.push_back("      |  " + line); // 8 spații față de marginile principale
             }
             yaraOutput.push_back("      ______________________________________");
             yaraOutput.push_back("");
         }
     }

     yaraGetRulesFiles = true;


}

void Instance::RunYara()
{
    if (yaraExecuted)
        return; // deja executat

    this->cursorRow     = 0;
    this->cursorCol     = 0;
    this->startViewLine = 0;
    this->leftViewCol   = 0;

    // Căi calculate dinamic
    fs::path currentPath = fs::current_path();
    fs::path currentFile = this->obj->GetPath();
    fs::path rootPath    = currentPath.parent_path().parent_path();
    fs::path yaraExe    = rootPath / "3rdPartyLibs" / "yara-win64" / "yara64.exe";
    fs::path yaraRule   = rootPath / "3rdPartyLibs" / "rules" / "helloworld.yara";
    fs::path outputFile = rootPath / "3rdPartyLibs" / "output" / "output.txt";

    // Debug: afișăm toate căile
    yaraOutput = { "=== YARA SCAN ===",
                   "",
                   "Target: " + currentFile.string(),      
                   //"YARA exe: " + yaraExe.string(),
                   "YARA rule: " + yaraRule.string(),
                   //"Output file: " + outputFile.string() 
                   ""
                   };

    // Verificări existență fișiere
    if (!fs::exists(yaraExe)) {
        yaraOutput.push_back("EROARE: YARA exe NU a fost gasit!");
        return;
    }

    if (!fs::exists(yaraRule)) {
        yaraOutput.push_back("EROARE: YARA rule NU a fost gasit!");
        return;
    }

    // Construim comanda EXACT ca cea hardcodata
    std::string yaraCmd = "\"" + yaraExe.string() +
                          "\" -g -m -s -r "
                          "\"" +
                          yaraRule.string() +
                          "\" "
                          "\"" +
                          currentFile.string() +
                          "\" "
                          "> \"" +
                          outputFile.string() + "\"";

    // cmd.exe arguments
    std::string cmdArgs = "/C " + yaraCmd;

    // rulăm fără a bloca UI-ul
    HINSTANCE hInst = ShellExecuteA(
          nullptr,         // HWND
          "open",          // verb
          "cmd.exe",       // executable
          cmdArgs.c_str(), // arguments
          nullptr,         // working directory
          SW_HIDE          // ascundem fereastra
    );

    if ((INT_PTR) hInst <= 32) {
        yaraOutput.push_back("EROARE: Nu s-a putut porni YARA!");
        return;
    }


    // Debug: cod de ieșire
    //yaraOutput.push_back("Cod Rezultat: " + std::to_string(result));

    // Verificăm dacă s-a creat fișierul de output
    //if (fs::exists(outputFile)) {
    //    yaraOutput.push_back("Output file a fost creat cu succes.");
    //} else {
    //    yaraOutput.push_back("EROARE: Output file NU a fost creat!");
    //}


    std::ifstream in(outputFile);
    if (!in.is_open()) {
        yaraOutput.push_back("Eroare la citirea fișierului: " + outputFile.string());
        return;
    }

    //std::string ruleName;
    //std::string tags;
    //std::string filePath;

    std::string metaStr; // va conține tot ce e între al doilea [ ]
    //std::string author, severity;

    int matchCount     = 0;
    bool foundAnything = false;

    std::string line;
    std::vector<std::string> matchedStrings;

    std::string currentRule, currentTags, currentAuthor, currentSeverity;

    while (std::getline(in, line)) {
        // eliminăm CR/LF
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();

        if (line.empty())
            continue;

        foundAnything = true;

        // Detectăm linia header pentru o regulă
        if (line.find(currentFile.string()) != std::string::npos && line.find('[') != std::string::npos && line.find(']') != std::string::npos) {
            // Salvăm regula precedentă
            if (!currentRule.empty() && !matchedStrings.empty()) {
                matchCount++;
                yaraOutput.push_back("    Rule: " + currentRule);
                yaraOutput.push_back("    Tags: " + currentTags);
                if (!currentAuthor.empty())
                    yaraOutput.push_back("    Author: " + currentAuthor);
                if (!currentSeverity.empty())
                    yaraOutput.push_back("    Severity: " + currentSeverity);

                for (auto& s : matchedStrings)
                    yaraOutput.push_back("        " + s);
                yaraOutput.push_back("");

                matchedStrings.clear();
                currentAuthor.clear();
                currentSeverity.clear();
                currentTags.clear();
            }

            // Extragem Rule name
            size_t spacePos = line.find(' ');
            currentRule     = line.substr(0, spacePos);

            // Extragem Tags
            size_t firstBracketStart = line.find('[');
            size_t firstBracketEnd   = line.find(']');
            if (firstBracketStart != std::string::npos && firstBracketEnd != std::string::npos)
                currentTags = line.substr(firstBracketStart + 1, firstBracketEnd - firstBracketStart - 1);

            // Extragem meta
            size_t metaStart = line.find('[', firstBracketEnd + 1);
            size_t metaEnd   = line.find(']', metaStart);
            if (metaStart != std::string::npos && metaEnd != std::string::npos) {
                std::string metaStr = line.substr(metaStart + 1, metaEnd - metaStart - 1);

                // Author
                size_t authorPos = metaStr.find("author=\"");
                if (authorPos != std::string::npos) {
                    size_t start = authorPos + strlen("author=\"");
                    size_t end   = metaStr.find("\"", start);
                    if (end != std::string::npos)
                        currentAuthor = metaStr.substr(start, end - start);
                }

                // Severity
                size_t sevPos = metaStr.find("severity=\"");
                if (sevPos != std::string::npos) {
                    size_t start = sevPos + strlen("severity=\"");
                    size_t end   = metaStr.find("\"", start);
                    if (end != std::string::npos)
                        currentSeverity = metaStr.substr(start, end - start);
                }
            }

            continue; // nu adăugăm linia header la matched strings
        }

        // Altfel, este o linie detectată (matched string)
        matchedStrings.push_back(line);
    }

    // Salvăm ultima regulă detectată
    if (!currentRule.empty() && !matchedStrings.empty()) {
        matchCount++;
        yaraOutput.push_back("    Rule: " + currentRule);
        yaraOutput.push_back("    Tags: " + currentTags);
        if (!currentAuthor.empty())
            yaraOutput.push_back("    Author: " + currentAuthor);
        if (!currentSeverity.empty())
            yaraOutput.push_back("    Severity: " + currentSeverity);

        for (auto& s : matchedStrings)
            yaraOutput.push_back("        " + s);
    }

    // ================= SUMMARY =================
    //yaraOutput.insert(yaraOutput.begin() + 4, ""); // linie goală
    yaraOutput.insert(yaraOutput.begin() + 5, "Summary:");
    yaraOutput.insert(yaraOutput.begin() + 6, "    Matches: " + std::to_string(matchCount));
    yaraOutput.insert(yaraOutput.begin() + 7, matchCount > 0 ? "    Status: INFECTED" : "    Status: CLEAN");
    yaraOutput.insert(yaraOutput.begin() + 8, ""); // linie goală
    yaraOutput.insert(yaraOutput.begin() + 9, "Matches:"); 



    yaraExecuted = true;
}