#pragma once

#include "Internal.hpp"


namespace GView::View::YaraViewer
{
    using namespace AppCUI;
    using namespace GView::Utils;

    namespace Commands
    {
        constexpr int CMD_YARA_RUN     = 0x01;
        constexpr int CMD_VIEW_RULES   = 0x02;
        constexpr int CMD_EDIT_RULES   = 0x03;
        constexpr int CMD_SELECT_ALL   = 0x04;
        constexpr int CMD_DESELECT_ALL = 0x05;
        constexpr int CMD_FIND_TEXT    = 0x06;
        constexpr int CMD_FIND_NEXT    = 0x07;
        constexpr int CMD_SAVE_REPORT  = 0x08;
        
        static KeyboardControl YaraRunCommand        = { Input::Key::F6, "Yara Run", "YaraRunCommand explanation", CMD_YARA_RUN };
        static KeyboardControl ViewRulesCommand      = { Input::Key::F7, "View Rules", "View all the rules from folder rules", CMD_VIEW_RULES };
        static KeyboardControl EditRulesCommand      = { Input::Key::F8, "Edit Rules", "Open rules folder to manage files", CMD_EDIT_RULES };
        static KeyboardControl SelectAllCommand      = { Input::Key::Ctrl | Input::Key::A, "Select All", "Select all available rules", CMD_SELECT_ALL };
        static KeyboardControl DeselectAllCommand    = { Input::Key::Ctrl | Input::Key::D, "Deselect All", "Deselect all rules", CMD_DESELECT_ALL };
        static KeyboardControl FindTextCommand       = { Input::Key::Ctrl | Input::Key::F, "Find", "Search for text", CMD_FIND_TEXT };
        static KeyboardControl FindNextCommand       = { Input::Key::F3, "Find Next", "Go to next occurrence", CMD_FIND_NEXT };
        static KeyboardControl SaveReportCommand     = { Input::Key::Ctrl | Input::Key::S, "Save Report", "Save results to text file", CMD_SAVE_REPORT };
    } // namespace Commands

    struct SettingsData
    {
        int analysisLevel;
        SettingsData();
    };

    struct Config
    {
        bool Loaded;

        static void Update(IniSection sect);
        void Initialize();
    };

    struct Layout
    {
        uint32 visibleLines;
        uint32 maxCharactersPerLine;
    };
       
    enum class LineType {
        Normal,
        FileHeader,  // Aici vom avea checkbox [ ] / [x]
        RuleContent, // Pentru fundal colorat
        Info,
        Match
    };

    struct LineInfo {
        std::string text;
        LineType type;
        bool isChecked = false;
        std::filesystem::path filePath;
        bool isExpanded = true; // Default EXPANDAT
        int indentLevel = 0;
        int parentIndex = -1;
        bool isVisible  = true; // Default VIZIBIL
    };

    class Instance : public View::ViewControl
    {
     
        Pointer<SettingsData> settings;
        Reference<GView::Object> obj;
        static Config config;
        Layout layout;

        bool yaraExecuted = false;  
        bool yaraGetRulesFiles = false;
        bool isButtonFindPressed    = false;
        std::vector<LineInfo> yaraLines;

        std::vector<size_t> visibleIndices;

        std::string lastSearchText;

        bool searchActive            = false;
        size_t searchResultRealIndex = 0; // Indexul real din yaraLines (nu cel vizual!)
        uint32 searchResultStartCol  = 0; // Coloana vizuală de start
        uint32 searchResultLen       = 0;

     public:
        uint32 startViewLine;
        uint32 leftViewCol;
        uint32 cursorRow;
        uint32 cursorCol;

        bool selectionActive;
        uint32 selectionAnchorRow;
        uint32 selectionAnchorCol;

        

        Instance(Reference<GView::Object> _obj, Settings* _settings);

        bool GetPropertyValue(uint32 propertyID, PropertyValue& value) override;
        bool SetPropertyValue(uint32 propertyID, const PropertyValue& value, String& error) override;
        void SetCustomPropertyValue(uint32 propertyID) override;
        bool IsPropertyValueReadOnly(uint32 propertyID) override;
        const vector<Property> GetPropertiesList() override;
        bool GoTo(uint64 offset) override;
        bool Select(uint64 offset, uint64 size) override;
        bool ShowGoToDialog() override;
        bool ShowFindDialog() override;
        bool ShowCopyDialog() override;
        void PaintCursorInformation(AppCUI::Graphics::Renderer& renderer, uint32 width, uint32 height) override;
        std::string_view GetCategoryNameForSerialization() const override;
        bool AddCategoryBeforePropertyNameWhenSerializing() const override;
        void Paint(Graphics::Renderer& renderer) override;
        bool OnUpdateCommandBar(Application::CommandBar& commandBar) override;
        bool UpdateKeys(KeyboardControlsInterface* interfaceParam) override;
        bool OnEvent(Reference<Control>, Event eventType, int ID) override;
        void OnAfterResize(int newWidth, int newHeight) override;
        bool OnKeyEvent(AppCUI::Input::Key keyCode, char16 characterCode) override;
        void MoveTo();
        bool OnMouseWheel(int x, int y, AppCUI::Input::MouseWheel direction, AppCUI::Input::Key key) override;
        bool OnMouseDrag(int x, int y, Input::MouseButton button, Input::Key keyCode) override;
        void OnMousePressed(int x, int y, Input::MouseButton button, Input::Key keyCode) override;
        void CopySelectionToClipboard();
        void ComputeMouseCoords(int x, int y, uint32 startViewLine, uint32 leftViewCol, const std::vector<LineInfo>& lines, uint32& outRow, uint32& outCol);

        void RunYara();  
        void GetRulesFiles();

        void ToggleSelection();
        void SelectAllRules();   
        void DeselectAllRules(); 

        void UpdateVisibleIndices();   
        void ToggleFold(size_t index); 
        
        void SelectMatch(uint32 row, size_t startRawCol, uint32 length);
        bool FindNext();

        void ExportResults();

        std::vector<std::string> ExtractHexContextFromYaraMatch(const std::string& yaraLine, const std::string& exePath, size_t contextSize = 64);
        std::string GetSectionFromOffset(const std::string& exePath, uint64_t offset);

    };


    class FindDialog : public AppCUI::Controls::Window
    {
        Reference<AppCUI::Controls::TextField> input;

      public:
        std::string resultText;

        FindDialog() : Window("Find Text", "d:c,w:60,h:8", WindowFlags::ProcessReturn)
        {
            // Eticheta
            AppCUI::Controls::Factory::Label::Create(this, "Search for:", "x:1,y:1,w:56");

            // Câmpul de text
            input = AppCUI::Controls::Factory::TextField::Create(this, "", "x:1,y:2,w:56");
            input->SetHotKey('S'); // Alt+S sare aici

            input->SetFocus();
            // Butoanele
            AppCUI::Controls::Factory::Button::Create(this, "&Find", "l:16,b:0,w:13", 100);
            AppCUI::Controls::Factory::Button::Create(this, "&Cancel", "l:31,b:0,w:13", 101);
        }

        bool OnEvent(Reference<Control> sender, Event eventType, int controlID) override
        {
            if (eventType == Event::ButtonClicked || eventType == Event::WindowAccept) {
                if ((eventType == Event::ButtonClicked && controlID == 100) || eventType == Event::WindowAccept) {
                    resultText = std::string(input->GetText());
                    Exit(Dialogs::Result::Ok);
                    return true;
                }
                if (controlID == 101) // ID-ul butonului Cancel
                {
                    Exit(Dialogs::Result::Cancel);
                    return true;
                }
            }
            return Window::OnEvent(sender, eventType, controlID);
        }
    };

}; // namespace GView