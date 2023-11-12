#include "Views/HelpPanel.h"

#include <unordered_map>

#include <ftxui/component/component.hpp>
#include <range/v3/algorithm.hpp>

#include "UI/Common.h"

using namespace sdb;
using namespace ftxui;

static std::unordered_map<std::string, PanelCommandsInfo>& gCommandsInfo() {
    static std::unordered_map<std::string, PanelCommandsInfo> map;
    return map;
}

static PanelCommandsInfo& gCommandsInfo(std::string const& panelName) {
    return gCommandsInfo()[panelName];
}

static void sort(PanelCommandsInfo& list) {
    ranges::sort(list, ranges::less{}, &CommandInfo::hotkey);
}

void sdb::addPanelCommandsInfo(std::string panelName, CommandInfo info) {
    auto& list = gCommandsInfo(panelName);
    list.push_back(std::move(info));
    sort(list);
}

void sdb::setPanelCommandsInfo(std::string panelName, PanelCommandsInfo info) {
    auto& list = gCommandsInfo(panelName);
    list = std::move(info);
    sort(list);
}

ModalView sdb::HelpPanel() {
    auto body = Renderer([] {
        std::vector<Element> panels;
        for (auto& [name, commandInfos]: gCommandsInfo()) {
            std::vector<Element> elems = { text(name) | bold | underlined };
            for (auto& info: commandInfos) {
                elems.push_back(hbox({ text(" "),
                                       text(info.hotkey) | bold,
                                       text(" : "),
                                       text(info.message) }));
            }
            panels.push_back(vbox(std::move(elems)));
        }
        return vbox(std::move(panels));
    });

    return ModalView("Help", body);
}
