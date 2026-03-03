#include "imgui.h"

namespace GameForest {
	void RenderUI() {
        if (ImGui::Begin("Game Forest", &open, ImGuiWindowFlags_NoCollapse)) {

            if (ImGui::Button("Add Game", ImVec2{ 0, 0 })) {
                showPathToExe = !showPathToExe;
                showChooseFileButton = !showChooseFileButton;

                if (!showPathToExe && strlen(pathToExe) > 0) {
                    addGameData(pathToExe);
                }
            }

            std::string gamesInstalledText = std::format("{} Games Installed", games.size());

            ImGui::Text(gamesInstalledText.c_str());

            if (showPathToExe) {
                ImGui::InputText("Path to .exe", pathToExe, IM_ARRAYSIZE(pathToExe));
            }

            if (showChooseFileButton && ImGui::Button("Select File")) {
                fileSelector();
            }

            for (auto& game : games) {
                if (ImGui::Button(game.name.c_str())) {
                    openGame(game.path);
                    done = true;
                }
            }
        }

        ImGui::End(); // End =========================================================================================================================================

        myStyle();
	}
}