#include "YaraViewer.hpp"
#include <algorithm>
#include <yara.h>


using namespace GView::View::YaraViewer;
using namespace AppCUI::Input;

Config Instance::config;

constexpr int startingX = 5;

Instance::Instance(Reference<GView::Object> _obj, Settings* _settings)
    : ViewControl("Yara View", UserControlFlags::ShowVerticalScrollBar | UserControlFlags::ScrollBarOutsideControl), settings(nullptr)
{
    this->obj = _obj;


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
    renderer.WriteSingleLineText(0, 0, "Cursor data", ColorPair{ Color::Yellow, Color::Transparent });
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

    ColorPair textColor            = ColorPair{ Color::Yellow, Color::Transparent };
    std::vector<std::string> lines = { 
        "line0", 
        "line1", 
        "line2", 
        "line3", 
    };


    int lineIndex = 1;
    for (const auto& line : lines) {
        if (lineIndex >= layout.visibleLines)
            break;
        renderer.WriteSingleLineText(startingX, lineIndex++, line, textColor);
    }
}

bool Instance::OnUpdateCommandBar(Application::CommandBar& commandBar)
{
    commandBar.SetCommand(Commands::SomeCommand.Key, Commands::SomeCommand.Caption, Commands::SomeCommand.CommandId);
    return false;
}

bool GView::View::YaraViewer::Instance::UpdateKeys(KeyboardControlsInterface* interface)
{
    interface->RegisterKey(&Commands::SomeCommand);
    return true;
}

bool Instance::OnEvent(Reference<Control>, Event eventType, int ID)
{
    if (eventType != Event::Command)
        return false;

    if (ID == Commands::SomeCommand.CommandId) {
        auto result = AppCUI::Dialogs::MessageBox::ShowOkCancel("SomeCMD", "MSG yes no");
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