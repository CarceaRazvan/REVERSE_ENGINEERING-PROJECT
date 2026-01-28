#pragma once

#include "Internal.hpp"


namespace GView::View::YaraViewer
{
    using namespace AppCUI;
    using namespace GView::Utils;

    namespace Commands
    {
        constexpr int DEMOVIEWER_SOME_CMD = 0x01;
        constexpr int DEMOVIEWER_SOME_CMD_VIEW_RULES = 0x02;
        constexpr int CMD_EDIT_RULES                 = 0x03;
        constexpr int CMD_SELECT_ALL                 = 0x04;
        constexpr int CMD_DESELECT_ALL               = 0x05;
        static KeyboardControl SomeCommand           = { Input::Key::F6, "Yara Run", "SomeCommand explanation", DEMOVIEWER_SOME_CMD };
        static KeyboardControl SomeCommandViewRules  = { Input::Key::F7, "View Rules", "View all the rules from folder rules", DEMOVIEWER_SOME_CMD_VIEW_RULES };
        static KeyboardControl EditRulesCommand      = { Input::Key::F8, "Edit Rules", "Open rules folder to manage files", CMD_EDIT_RULES };
        static KeyboardControl SelectAllCommand      = { Input::Key::Ctrl | Input::Key::A, "Select All", "Select all available rules", CMD_SELECT_ALL };
        static KeyboardControl DeselectAllCommand    = { Input::Key::Ctrl | Input::Key::D, "Deselect All", "Deselect all rules", CMD_DESELECT_ALL };
    }

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
        //std::vector<std::string> yaraOutput;  
        std::vector<LineInfo> yaraLines;

        std::vector<size_t> visibleIndices;

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

        void UpdateVisibleIndices();   // Recalculează ce e vizibil
        void ToggleFold(size_t index); // Deschide/Închide folder

        std::vector<std::string> ExtractHexContextFromYaraMatch(const std::string& yaraLine, const std::string& exePath, size_t contextSize = 64);
        std::string GetSectionFromOffset(const std::string& exePath, uint64_t offset);

    };

}; // namespace GView