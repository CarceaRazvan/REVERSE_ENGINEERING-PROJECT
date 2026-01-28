#pragma once

#include "Internal.hpp"


namespace GView::View::YaraViewer
{
    using namespace AppCUI;
    using namespace GView::Utils;

    namespace Commands
    {
        constexpr int DEMOVIEWER_SOME_CMD = 0x01;
        static KeyboardControl SomeCommand = { Input::Key::F6, "Yara Run", "SomeCommand explanation", DEMOVIEWER_SOME_CMD };

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
       
    class Instance : public View::ViewControl
    {
     
        Pointer<SettingsData> settings;
        Reference<GView::Object> obj;
        static Config config;
        Layout layout;

        bool yaraExecuted = false;  
        bool yaraGetRulesFiles = false;
        std::vector<std::string> yaraOutput;  



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

        void RunYara();  
        void GetRulesFiles();

    };

}; // namespace GView