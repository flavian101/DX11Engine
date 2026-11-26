#include "dxpch.h"
#include "AssetsPanel.h"
#include "EditorContext.h"
#include "EditorTheme.h"
#include "PlatformDialogs.h"
#include "SceneEditing.h"
#include "models/ModelAsset.h"
#include "scene/Components.h"

#include "imgui.h"
#include <filesystem>

namespace DXEngine
{
    AssetsPanel::AssetsPanel()
    {
        m_Picker = [] { return PlatformDialogs::OpenModelFile(); };
    }

    void AssetsPanel::SetAssetsFolder(const std::string& folder)
    {
        m_Folder = folder;
        m_Scanned = false;
    }

    void AssetsPanel::SetFilePicker(FilePicker picker)
    {
        m_Picker = picker ? std::move(picker) : FilePicker([] { return PlatformDialogs::OpenModelFile(); });
    }

    void AssetsPanel::Rescan()
    {
        m_Files = Assets::ScanModelFiles(m_Folder);
        m_Scanned = true;
    }

    std::string AssetsPanel::FullPath(const Assets::ModelFile& file) const
    {
        return m_Folder + "/" + file.path;
    }

    void AssetsPanel::RequestImport(const std::string& path, bool addToScene, bool reload)
    {
        if (path.empty() || m_HasRequest) return;               // one at a time: the next click comes after this one is done
        m_Request = { path, addToScene, reload, 0 };
        m_HasRequest = true;
    }

    void AssetsPanel::PickAndImport()
    {
        const std::string path = m_Picker ? m_Picker() : std::string();
        if (!path.empty())
            RequestImport(path, true);
    }

    // Put an imported model into the scene in front of the camera and select it.
    void AssetsPanel::AddToScene(EditorContext& ctx, const std::string& path)
    {
        const ImportedModel* model = ctx.models.Find(path);
        if (!model || !ctx.scene) return;

        Entity root = SceneEditing::InstantiateModel(*ctx.scene, model->asset, model->name, ctx.SpawnPoint());
        ctx.MarkEdited();
        if (!root.IsValid())
        {
            m_Status = "Could not add " + model->name + ": the model is empty";
            m_Log.Error(m_Status);
            return;
        }
        ctx.models.NoteInstance(path);
        ctx.selection.Set(root);
        ctx.revealSelection = true;
        m_Log.Info("Added " + model->name + " to the scene");
    }

    void AssetsPanel::Execute(EditorContext& ctx, const Request& request)
    {
        const ModelImporter::Result result = m_Importer.Import(ctx.models, m_Log, request.path, request.reload);
        m_Status = result.message;
        if (result.model && request.addToScene)
            AddToScene(ctx, request.path);
    }

    void AssetsPanel::Update(EditorContext& ctx)
    {
        if (!m_HasRequest) return;

        // The frame the request arrives only shows the message; the work (which blocks) starts the frame after.
        if (m_Request.framesWaited >= 1)
        {
            const Request request = m_Request;
            m_HasRequest = false;
            Execute(ctx, request);
            return;
        }
        ++m_Request.framesWaited;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->GetCenter().x, viewport->GetCenter().y), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;
        if (ImGui::Begin("##ImportingModel", nullptr, flags))
        {
            const std::string name = std::filesystem::path(m_Request.path).filename().string();
            ImGui::Text("%s %s ...", m_Request.reload ? "Reimporting" : "Importing", name.c_str());
        }
        ImGui::End();
    }

    void AssetsPanel::OnImGui(EditorContext& ctx, bool* open)
    {
        if (open && !*open) return;
        if (!ImGui::Begin("Assets", open)) { ImGui::End(); return; }

        if (!m_Scanned) Rescan();

        if (ImGui::Button("Import file...")) PickAndImport();
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) Rescan();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##filter", "Search models", m_Filter, sizeof(m_Filter));

        if (!m_Status.empty())
            ImGui::TextColored(EditorTheme::Colors().textDim, "%s", m_Status.c_str());

        if (ImGui::CollapsingHeader("Files", ImGuiTreeNodeFlags_DefaultOpen))
            DrawFiles(ctx);
        if (ImGui::CollapsingHeader("Imported", ImGuiTreeNodeFlags_DefaultOpen))
            DrawImported(ctx);
        if (ImGui::CollapsingHeader("Log"))
            DrawLog();

        ImGui::End();
    }

    void AssetsPanel::DrawFiles(EditorContext& ctx)
    {
        const EditorTheme::Palette& colors = EditorTheme::Colors();
        const std::vector<size_t> shown = Assets::FilterModelFiles(m_Files, m_Filter);

        if (m_Files.empty())
        {
            ImGui::TextColored(colors.textDim, "No model files in '%s'.", m_Folder.c_str());
            ImGui::TextColored(colors.textDim, "Put models there, or use Import file...");
            return;
        }
        if (shown.empty())
        {
            ImGui::TextColored(colors.textDim, "Nothing matches '%s'.", m_Filter);
            return;
        }

        std::string lastFolder = "\x01";                        // (no folder is called that: the first row always prints its header)
        for (size_t index : shown)
        {
            const Assets::ModelFile& file = m_Files[index];
            if (file.folder != lastFolder)
            {
                lastFolder = file.folder;
                ImGui::TextColored(colors.textDim, "%s", file.folder.empty() ? "(root)" : file.folder.c_str());
            }

            ImGui::PushID(static_cast<int>(index));
            const std::string full = FullPath(file);
            const ImportedModel* imported = ctx.models.Find(full);

            ImGui::Indent();
            if (ImGui::Selectable(file.name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns) &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                RequestImport(full, true);

            if (ImGui::BeginPopupContextItem("##fileMenu"))
            {
                if (ImGui::MenuItem("Add to scene")) RequestImport(full, true);
                if (ImGui::MenuItem(imported ? "Reimport" : "Import only", nullptr, false)) RequestImport(full, false, imported != nullptr);
                ImGui::EndPopup();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s  (%s)\nDouble click: add to the scene", file.path.c_str(), Assets::FormatBytes(file.sizeBytes).c_str());

            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f * EditorTheme::Scale());
            if (imported) ImGui::TextColored(colors.accent, "imported");
            else          ImGui::TextColored(colors.textDim, "%s", Assets::FormatBytes(file.sizeBytes).c_str());
            ImGui::Unindent();
            ImGui::PopID();
        }
    }

    void AssetsPanel::DrawImported(EditorContext& ctx)
    {
        const EditorTheme::Palette& colors = EditorTheme::Colors();
        if (ctx.models.Count() == 0)
        {
            ImGui::TextColored(colors.textDim, "Nothing imported yet.");
            return;
        }

        std::string unload;                                      // removed after the loop: the list must not change under it
        for (const ImportedModel& model : ctx.models.All())
        {
            const ModelAsset& asset = *model.asset;
            ImGui::PushID(model.key.c_str());
            const bool node = ImGui::TreeNodeEx(model.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
            if (!asset.warnings.empty())
            {
                ImGui::SameLine();
                ImGui::TextColored(colors.warning, "(%zu warnings)", asset.warnings.size());
            }
            if (node)
            {
                ImGui::TextColored(colors.textDim, "%zu meshes   %zu triangles   %zu materials   %zu nodes",
                    asset.meshes.size(), asset.GetTotalTriangleCount(), asset.materials.size(), asset.nodes.size());
                if (asset.IsSkinned())
                    ImGui::TextColored(colors.textDim, "%zu bones   %zu animations", asset.skeleton->GetBoneCount(), asset.clips.size());
                ImGui::TextColored(colors.textDim, "imported in %.0f ms   added to the scene %zu times", model.importMs, model.instances);

                if (ImGui::Button("Add to scene")) AddToScene(ctx, model.path);
                ImGui::SameLine();
                if (ImGui::Button("Reimport")) RequestImport(model.path, false, true);
                ImGui::SameLine();
                if (ImGui::Button("Unload")) unload = model.path;

                for (const std::string& w : asset.warnings)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, colors.warning);
                    ImGui::TextWrapped("%s", w.c_str());
                    ImGui::PopStyleColor();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (!unload.empty())
        {
            ctx.models.Remove(unload);
            m_Log.Info("Unloaded " + Assets::FileStem(unload) + " (what is in the scene stays)");
        }
    }

    void AssetsPanel::DrawLog()
    {
        const EditorTheme::Palette& colors = EditorTheme::Colors();
        if (ImGui::SmallButton("Clear")) m_Log.Clear();

        ImGui::BeginChild("##importLog", ImVec2(0.0f, 120.0f * EditorTheme::Scale()), true);
        for (const Assets::LogLine& line : m_Log.Lines())
        {
            const ImVec4& color = line.severity == Assets::LogSeverity::Error   ? colors.danger
                                : line.severity == Assets::LogSeverity::Warning ? colors.warning : colors.text;
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", line.text.c_str());
            ImGui::PopStyleColor();
        }
        if (m_Log.Lines().size() != m_LogSeen)                    // new lines: keep the newest in view
        {
            ImGui::SetScrollHereY(1.0f);
            m_LogSeen = m_Log.Lines().size();
        }
        ImGui::EndChild();
    }
}
