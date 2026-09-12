#include "extra/assetexporter_ui.h"
#include "pc/imgui_localization.h"

#include "extra/assetexporter.h"
#include "extra/modloader.h"
#include "imgui.h"
#include "pc/log.h"
#include <cstdio>
#include <string>

// Asset Exporter window

static const char* kCategoryNames[] = {
    "All",
    "Texture",
    "SkeletalMesh",
    "StaticMesh / Level (legacy)",
    "ETreeMesh",
    "Sound",
    "Data",
    "Level (FULL: JSON + Geometry + Blocks + Collision + Textures + Raw)"
};
static constexpr int kFullLevelCategory = 7;

static bool AssetExporterCategoryMatches(const AssetEntry& entry, int categoryFilter) {
    if (categoryFilter == 0) return true;
    if (categoryFilter == kFullLevelCategory) return entry.category == AssetCategory::StaticMesh;
    return entry.category == static_cast<AssetCategory>(categoryFilter - 1);
}

static char sOutputDir[256] = "~mods/ExportedAssets";
static int sCategoryFilter = 0;
static char sNameFilter[128] = {};

void DrawAssetExporterWindow(bool* pOpen) {
    if (!ImGui::Begin(ImGuiLocalization::Label("Asset Exporter", "IM_ASSET_EXPORTER", "DBG_AssetExporterWindow").c_str(), pOpen)) {
        ImGui::End();
        return;
    }

    AssetExporter& exporter = AssetExporter::Instance();

    // Build catalog button
    if (ImGui::Button("Scan Disk for Assets")) {
        exporter.ClearCatalog();
        exporter.BuildCatalog();
        LOG("[AssetExporter] Catalog rebuilt from disk");
    }

    ImGui::SameLine();
    ImGui::Text("Found: %zu assets", exporter.GetEntries().size());

    // Output directory
    ImGui::InputText("Output Dir", sOutputDir, sizeof(sOutputDir));
    ImGui::TextDisabled("Exports include modern assets plus discovery manifests and preserved raw source data.");
    ImGui::TextDisabled("Existing PNG/WAV/GLB mod paths remain unchanged; the extra indexes are tooling metadata.");

    // Category filter
    ImGui::Combo("Category", &sCategoryFilter, kCategoryNames, IM_ARRAYSIZE(kCategoryNames));

    // Name filter
    ImGui::InputText("Filter", sNameFilter, sizeof(sNameFilter));

    ImGui::Separator();

    // Batch export buttons
    if (ImGui::Button("Export All")) {
        if (sCategoryFilter == 0) {
            exporter.ExportAllCategories(sOutputDir, nullptr);
        }
        else if (sCategoryFilter == kFullLevelCategory) {
            exporter.ExportAll(sOutputDir, AssetCategory::StaticMesh, true, nullptr);
        }
        else {
            AssetCategory filterCat = static_cast<AssetCategory>(sCategoryFilter - 1);
            exporter.ExportAll(sOutputDir, filterCat, true, nullptr);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Export Visible")) {
        for (const auto& entry : exporter.GetEntries()) {
            if (!AssetExporterCategoryMatches(entry, sCategoryFilter)) continue;
            if (sNameFilter[0] != '\0' &&
                entry.name.find(sNameFilter) == std::string::npos) continue;
            exporter.ExportEntry(entry, sOutputDir);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Export Complete Archive")) {
        exporter.ExportAllCategories(sOutputDir, nullptr);
    }

    ImGui::Separator();
    ImGui::TextDisabled("FULL Level export: petal JSON + named geometry + BLK render geometry + collision + textures + manifests + exact raw level stream.");
    ImGui::TextDisabled("Complete Archive also snapshots the full extracted game asset tree (FE/RCHARS/RTARGET/SCR/SOUND/TIM/XC + root files) under raw_game/.");
    ImGui::TextDisabled("RSDIALOG.DLG exports every spoken character/dialog/variant as an override-ready WAV.");

    // Asset list
    if (ImGui::BeginChild("AssetList", ImVec2(0, 0), true)) {
        for (const auto& entry : exporter.GetEntries()) {
            // Apply category filter
            if (!AssetExporterCategoryMatches(entry, sCategoryFilter)) continue;

            // Apply name filter
            if (sNameFilter[0] != '\0') {
                if (entry.name.find(sNameFilter) == std::string::npos) continue;
            }

            char label[512];
            const char* visibleCategory =
                (sCategoryFilter == kFullLevelCategory && entry.category == AssetCategory::StaticMesh)
                    ? "FullLevel"
                    : AssetEntry::CategoryName(entry.category);
            std::snprintf(label, sizeof(label), "%s (%s) [%s]###%s",
                          entry.name.c_str(),
                          visibleCategory,
                          entry.filePath.c_str(),
                          entry.name.c_str());

            if (ImGui::Selectable(label, false)) {
                exporter.ExportEntry(entry, sOutputDir);
            }
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

// Mods window

void DrawModsWindow(bool* pOpen) {
    if (!ImGui::Begin(ImGuiLocalization::Label("Mods", "IM_MODS", "DBG_ModsWindow").c_str(), pOpen)) {
        ImGui::End();
        return;
    }

#ifdef MOD_LOADER
    ModLoader& loader = ModLoader::Instance();

    if (ImGui::Button("Reload Mods")) {
        loader.Reload();
    }

    ImGui::SameLine();
    ImGui::Text("Active mods: %zu", loader.GetMods().size());

    if (!loader.IsEnabled()) {
        ImGui::TextDisabled("Mod loader disabled via ~mods/mods.ini ([general] enabled = false)");
    }
    ImGui::TextDisabled("Reload rescans files; already-instantiated level geometry/parameters apply on the next level or petal load.");

    ImGui::Separator();

    if (ImGui::BeginChild("ModList", ImVec2(0, 0), true)) {
        for (const auto& mod : loader.GetMods()) {
            ImGui::Text("#%d %s", mod.priority, mod.name.c_str());
            ImGui::Separator();

            ImGui::Text("Folder: %s", mod.folder.c_str());
            if (!mod.version.empty()) {
                ImGui::SameLine();
                ImGui::Text("v%s", mod.version.c_str());
            }
            if (!mod.author.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("by %s", mod.author.c_str());
            }

            ImGui::Text("Textures: %d | Models: %d | Sounds: %d | Data: %d",
                        mod.textureCount, mod.modelCount, mod.soundCount, mod.dataCount);

            ImGui::Text("Status: %s", mod.enabled ? "Enabled" : "Disabled");
        }
        ImGui::EndChild();
    }
#else
    ImGui::TextDisabled("Mod loader is disabled (MOD_LOADER not defined).");
    ImGui::TextWrapped("Set MOD_LOADER to 1 in config.h and rebuild to enable mod support.");
#endif

    ImGui::End();
}
