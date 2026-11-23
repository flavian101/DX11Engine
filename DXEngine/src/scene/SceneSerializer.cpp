#include "dxpch.h"
#include "SceneSerializer.h"
#include "Scene.h"
#include "models/ModelAsset.h"
#include "utils/Json.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include "utils/material/MaterialSlots.h"
#include "utils/material/MaterialTypes.h"
#include <cmath>
#include <unordered_map>

namespace DXEngine
{
    namespace SceneSerializer
    {
        using Json::Value;

        namespace
        {
            constexpr const char* kFormat = "dxengine.scene";

            // ---------------------------------------------------------------- names of enumerators
            template<class E> struct EnumName { E value; const char* name; };

            template<class E, size_t N>
            const char* NameOf(const EnumName<E> (&table)[N], E value)
            {
                for (const auto& row : table) if (row.value == value) return row.name;
                return table[0].name;
            }
            template<class E, size_t N>
            bool ParseEnum(const EnumName<E> (&table)[N], const std::string& text, E& out)
            {
                for (const auto& row : table) if (text == row.name) { out = row.value; return true; }
                return false;
            }

            const EnumName<LightType> kLightTypes[] = { { LightType::Directional, "directional" }, { LightType::Point, "point" }, { LightType::Spot, "spot" } };
            const EnumName<PlaybackMode> kModes[] = { { PlaybackMode::Once, "once" }, { PlaybackMode::Loop, "loop" }, { PlaybackMode::PingPong, "pingpong" } };
            const EnumName<RHI::BlendMode> kBlend[] = {
                { RHI::BlendMode::Opaque, "Opaque" }, { RHI::BlendMode::AlphaBlend, "AlphaBlend" }, { RHI::BlendMode::Additive, "Additive" },
                { RHI::BlendMode::Multiply, "Multiply" }, { RHI::BlendMode::Premultiplied, "Premultiplied" } };
            const EnumName<RHI::CullMode> kCull[] = { { RHI::CullMode::None, "None" }, { RHI::CullMode::Front, "Front" }, { RHI::CullMode::Back, "Back" } };
            const EnumName<RHI::DepthTestMode> kDepthTest[] = {
                { RHI::DepthTestMode::None, "None" }, { RHI::DepthTestMode::Less, "Less" }, { RHI::DepthTestMode::LessEqual, "LessEqual" },
                { RHI::DepthTestMode::Greater, "Greater" }, { RHI::DepthTestMode::GreaterEqual, "GreaterEqual" }, { RHI::DepthTestMode::Equal, "Equal" },
                { RHI::DepthTestMode::NotEqual, "NotEqual" }, { RHI::DepthTestMode::Always, "Always" } };
            const EnumName<DepthWriteMode> kDepthWrite[] = { { DepthWriteMode::Auto, "Auto" }, { DepthWriteMode::On, "On" }, { DepthWriteMode::Off, "Off" } };
            const EnumName<RenderQueue> kQueue[] = {
                { RenderQueue::Auto, "Auto" }, { RenderQueue::Background, "Background" }, { RenderQueue::Opaque, "Opaque" },
                { RenderQueue::Transparent, "Transparent" }, { RenderQueue::UI, "UI" }, { RenderQueue::Overlay, "Overlay" } };

            // ---------------------------------------------------------------- small JSON helpers
            Value Floats(const float* v, size_t n)
            {
                Value a = Value::MakeArray();
                for (size_t i = 0; i < n; ++i) a.Push(v[i]);
                return a;
            }

            // Reads exactly n finite numbers. False (and 'out' untouched) for anything else: a repaired value is a decision
            // the caller makes, a half-read vector is never handed out.
            bool ReadFloats(const Value& v, float* out, size_t n)
            {
                if (!v.IsArray() || v.Size() != n) return false;
                float tmp[16];
                for (size_t i = 0; i < n; ++i)
                {
                    if (!v.At(i).IsNumber() || !std::isfinite(v.At(i).AsNumber())) return false;
                    tmp[i] = v.At(i).AsFloat();
                }
                for (size_t i = 0; i < n; ++i) out[i] = tmp[i];
                return true;
            }

            bool ReadFloat(const Value& v, float& out)
            {
                if (!v.IsNumber() || !std::isfinite(v.AsNumber())) return false;
                out = v.AsFloat();
                return true;
            }

            std::string Describe(const std::string& name, Uuid id) { return "'" + name + "' (" + id.ToString() + ")"; }

            // ================================================================ saving
            struct SaveContext
            {
                IAssetBridge& bridge;
                std::vector<std::string>& warnings;
                std::vector<std::shared_ptr<Material>> inlineMaterials;      // written once each, entities point at them
                std::unordered_map<const Material*, int> inlineIndex;
            };

            Value RenderStateToJson(const RenderState& s)
            {
                Value o = Value::MakeObject();
                o.Set("blend", NameOf(kBlend, s.blend)).Set("cull", NameOf(kCull, s.cull)).Set("depthTest", NameOf(kDepthTest, s.depthTest))
                 .Set("depthWrite", NameOf(kDepthWrite, s.depthWrite)).Set("queue", NameOf(kQueue, s.queue));
                return o;
            }

            Value MaterialToJson(const Material& m, SaveContext& ctx, const std::string& owner)
            {
                const MaterialProperties& p = m.GetProperties();
                Value o = Value::MakeObject();
                o.Set("name", m.GetName());
                o.Set("shading", ShadingModels::Name(m.GetShadingModel()));
                o.Set("diffuse", Floats(&p.diffuseColor.x, 4)).Set("specular", Floats(&p.specularColor.x, 4)).Set("emissive", Floats(&p.emissiveColor.x, 4));
                o.Set("shininess", p.shininess).Set("metallic", p.metallic).Set("roughness", p.roughness).Set("alpha", p.alpha);
                o.Set("normalScale", p.normalScale).Set("heightScale", p.heightScale).Set("occlusionStrength", p.occlusionStrength).Set("emissiveIntensity", p.emissiveIntensity);
                o.Set("textureScale", Floats(&p.textureScale.x, 2)).Set("textureOffset", Floats(&p.textureOffset.x, 2));
                o.Set("detailScale", Floats(&p.detailScale.x, 2)).Set("detailOffset", Floats(&p.detailOffset.x, 2));
                o.Set("dataChannels", Floats(&p.dataChannels.x, 4));
                o.Set("state", RenderStateToJson(m.GetRenderState()));

                Value textures = Value::MakeObject();
                const auto& bound = m.GetTextures();
                for (const SlotInfo& info : MaterialSlots::All())
                {
                    const size_t i = static_cast<size_t>(info.slot);
                    if (i >= bound.size() || !bound[i]) continue;
                    const std::string name = ctx.bridge.NameTexture(*bound[i], info.slot);
                    if (name.empty())
                        ctx.warnings.push_back("the " + std::string(info.name) + " texture of material '" + m.GetName() + "' (" + owner + ") has no file and was not saved");
                    else
                        textures.Set(info.name, name);
                }
                if (textures.Size() > 0) o.Set("textures", textures);
                return o;
            }

            // The index of the material in the scene's material table (a material used twice is written once).
            int InlineIndex(const std::shared_ptr<Material>& m, SaveContext& ctx)
            {
                const auto it = ctx.inlineIndex.find(m.get());
                if (it != ctx.inlineIndex.end()) return it->second;
                const int index = static_cast<int>(ctx.inlineMaterials.size());
                ctx.inlineMaterials.push_back(m);
                ctx.inlineIndex.emplace(m.get(), index);
                return index;
            }

            void WriteEntity(Scene& scene, Entity e, SaveContext& ctx, Value& entities)
            {
                const NameComponent* nameComponent = e.TryGet<NameComponent>();
                const std::string name = nameComponent ? nameComponent->name : "Entity";
                const Uuid id = scene.GetUuid(e);
                const std::string who = Describe(name, id);

                Value o = Value::MakeObject();
                o.Set("id", id.ToString()).Set("name", name);
                if (Entity parent = scene.GetParent(e); parent.IsValid() && !scene.GetUuid(parent).IsNull())
                    o.Set("parent", scene.GetUuid(parent).ToString());

                if (const TransformComponent* t = e.TryGet<TransformComponent>())
                {
                    Value tj = Value::MakeObject();
                    tj.Set("position", Floats(&t->position.x, 3)).Set("rotation", Floats(&t->rotation.x, 4)).Set("scale", Floats(&t->scale.x, 3));
                    o.Set("transform", tj);
                }

                if (const LightComponent* l = e.TryGet<LightComponent>())
                {
                    Value lj = Value::MakeObject();
                    lj.Set("type", NameOf(kLightTypes, l->type)).Set("color", Floats(&l->color.x, 3)).Set("intensity", l->intensity).Set("range", l->range)
                      .Set("innerCone", l->innerConeDegrees).Set("outerCone", l->outerConeDegrees).Set("castsShadows", l->castsShadows);
                    o.Set("light", lj);
                }

                if (e.Has<SkyboxComponent>())    o.Set("skybox", true);
                if (e.Has<SkinnedMeshComponent>()) o.Set("skinned", true);

                if (const MeshRendererComponent* r = e.TryGet<MeshRendererComponent>())
                {
                    const std::string meshName = r->mesh ? ctx.bridge.NameMesh(*r->mesh) : std::string();
                    if (meshName.empty())
                        ctx.warnings.push_back(std::string(r->mesh ? "the mesh of " : "the mesh renderer of ") + who + (r->mesh ? " has no name a file can use" : " has no mesh") + ": it was not saved");
                    else
                    {
                        Value rj = Value::MakeObject();
                        rj.Set("mesh", meshName);
                        Value materials = Value::MakeArray();
                        for (const auto& m : r->materials)
                        {
                            if (!m) { materials.Push(Value()); continue; }          // "the mesh's own material"
                            const std::string ref = ctx.bridge.NameMaterial(*m);
                            Value entry = Value::MakeObject();
                            if (!ref.empty()) entry.Set("ref", ref);
                            else              entry.Set("material", InlineIndex(m, ctx));
                            materials.Push(entry);
                        }
                        rj.Set("materials", materials);
                        rj.Set("visible", r->visible).Set("castsShadows", r->castsShadows).Set("receivesShadows", r->receivesShadows);
                        o.Set("meshRenderer", rj);
                    }
                }

                if (const AnimatorComponent* a = e.TryGet<AnimatorComponent>())
                {
                    const std::string model = a->skeleton ? ctx.bridge.NameModelOfSkeleton(*a->skeleton) : std::string();
                    if (model.empty())
                        ctx.warnings.push_back("the animator of " + who + " belongs to no model a file can name: it was not saved");
                    else
                    {
                        Value aj = Value::MakeObject();
                        aj.Set("model", model).Set("clip", a->clip).Set("speed", a->speed).Set("mode", NameOf(kModes, a->mode))
                          .Set("playing", a->playing).Set("time", a->time);
                        o.Set("animator", aj);
                    }
                }

                entities.Push(o);

                // children in their order, so loading appends them in the same order
                for (Entity c = scene.FirstChild(e); c.IsValid(); )
                {
                    const Entity next = scene.NextSibling(c);
                    WriteEntity(scene, c, ctx, entities);
                    c = next;
                }
            }

            // ================================================================ loading
            struct LoadContext
            {
                IAssetBridge& bridge;
                std::vector<std::string>& warnings;
                std::vector<std::shared_ptr<Material>> materials;                    // the scene's inline material table
                std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes;       // by name: shared entities stay shared
                std::unordered_map<std::string, std::shared_ptr<Material>> namedMaterials;
                std::unordered_map<std::string, std::shared_ptr<ModelAsset>> models;
                std::unordered_map<std::string, bool> triedMeshes, triedMaterials, triedModels;

                template<class T, class F>
                std::shared_ptr<T> Cached(std::unordered_map<std::string, std::shared_ptr<T>>& cache, std::unordered_map<std::string, bool>& tried,
                                          const std::string& name, F&& resolve)
                {
                    if (tried.count(name)) return cache.count(name) ? cache[name] : nullptr;   // a failure is not retried (or reported) again
                    tried[name] = true;
                    std::shared_ptr<T> r = resolve(name);
                    if (r) cache[name] = r;
                    return r;
                }
            };

            void ReadRenderState(const Value& v, RenderState& s, LoadContext& ctx, const std::string& owner)
            {
                if (!v.IsObject()) return;
                const auto read = [&](const char* key, auto& table, auto& field)
                {
                    if (!v.Has(key)) return;
                    if (!ParseEnum(table, v[key].AsString(), field))
                        ctx.warnings.push_back("material " + owner + ": unknown " + key + " '" + v[key].AsString() + "', kept the default");
                };
                read("blend", kBlend, s.blend);
                read("cull", kCull, s.cull);
                read("depthTest", kDepthTest, s.depthTest);
                read("depthWrite", kDepthWrite, s.depthWrite);
                read("queue", kQueue, s.queue);
            }

            std::shared_ptr<Material> MaterialFromJson(const Value& j, LoadContext& ctx, size_t index)
            {
                const std::string name = j["name"].IsString() ? j["name"].AsString() : "Material";
                const std::string owner = "'" + name + "' (#" + std::to_string(index) + ")";

                ShadingModel model = ShadingModel::Lit;
                if (j.Has("shading") && !ShadingModels::Parse(j["shading"].AsString(), model))
                {
                    ctx.warnings.push_back("material " + owner + ": unknown shading model '" + j["shading"].AsString() + "', used Lit");
                    model = ShadingModel::Lit;
                }
                auto m = std::make_shared<Material>(name, model);
                const MaterialProperties defaults = m->GetProperties();
                MaterialProperties p = defaults;

                const auto vec = [&](const char* key, float* dst, size_t n)
                {
                    if (j.Has(key) && !ReadFloats(j[key], dst, n))
                        ctx.warnings.push_back("material " + owner + ": '" + key + "' is not " + std::to_string(n) + " numbers, kept the default");
                };
                const auto num = [&](const char* key, float& dst)
                {
                    if (j.Has(key) && !ReadFloat(j[key], dst))
                        ctx.warnings.push_back("material " + owner + ": '" + key + "' is not a number, kept the default");
                };
                vec("diffuse", &p.diffuseColor.x, 4); vec("specular", &p.specularColor.x, 4); vec("emissive", &p.emissiveColor.x, 4);
                num("shininess", p.shininess); num("metallic", p.metallic); num("roughness", p.roughness); num("alpha", p.alpha);
                num("normalScale", p.normalScale); num("heightScale", p.heightScale); num("occlusionStrength", p.occlusionStrength); num("emissiveIntensity", p.emissiveIntensity);
                vec("textureScale", &p.textureScale.x, 2); vec("textureOffset", &p.textureOffset.x, 2);
                vec("detailScale", &p.detailScale.x, 2); vec("detailOffset", &p.detailOffset.x, 2);
                vec("dataChannels", &p.dataChannels.x, 4);

                // through the setters: they clamp and tell the material its buffer is stale
                m->SetDiffuseColor(p.diffuseColor); m->SetSpecularColor(p.specularColor); m->SetEmissiveColor(p.emissiveColor);
                m->SetShininess(p.shininess); m->SetMetallic(p.metallic); m->SetRoughness(p.roughness); m->SetAlpha(p.alpha);
                m->SetNormalScale(p.normalScale); m->SetHeightScale(p.heightScale); m->SetOcculsionStrength(p.occlusionStrength); m->SetEmissiveIntensity(p.emissiveIntensity);
                m->SetTextureScale(p.textureScale); m->SetTextureOffset(p.textureOffset);
                m->SetDetailTextureScale(p.detailScale); m->SetDetailTextureOffset(p.detailOffset);
                m->SetDataChannel(TextureSlot::Roughness, static_cast<int>(p.dataChannels.x));
                m->SetDataChannel(TextureSlot::Metallic, static_cast<int>(p.dataChannels.y));
                m->SetDataChannel(TextureSlot::AmbientOcclusion, static_cast<int>(p.dataChannels.z));
                m->SetNormalFlipY(p.dataChannels.w > 0.5f);

                RenderState state = m->GetRenderState();
                ReadRenderState(j["state"], state, ctx, owner);
                m->SetRenderState(state);

                const Value& textures = j["textures"];
                for (const std::string& key : textures.Keys())
                {
                    TextureSlot slot = TextureSlot::Count;
                    for (const SlotInfo& info : MaterialSlots::All()) if (key == info.name) slot = info.slot;
                    if (slot == TextureSlot::Count) { ctx.warnings.push_back("material " + owner + ": unknown texture slot '" + key + "'"); continue; }
                    auto texture = ctx.bridge.ResolveTexture(textures[key].AsString(), slot);
                    if (!texture) ctx.warnings.push_back("material " + owner + ": the " + key + " texture '" + textures[key].AsString() + "' could not be loaded");
                    else m->SetTexture(slot, texture);
                }
                return m;
            }

            void ApplyEntity(Scene& scene, Entity e, const Value& j, const std::unordered_map<Uuid, Entity>& byId, LoadContext& ctx)
            {
                const std::string who = Describe(scene.Registry().get<NameComponent>(e.Id()).name, scene.GetUuid(e));

                // ---- parent (it may be listed later in a hand-edited file: every entity exists by now)
                if (j.Has("parent"))
                {
                    Uuid pid;
                    if (!Uuid::Parse(j["parent"].AsString(), pid)) ctx.warnings.push_back(who + ": the parent id is not valid, it is a root");
                    else
                    {
                        const auto it = byId.find(pid);
                        if (it == byId.end()) ctx.warnings.push_back(who + ": its parent " + pid.ToString() + " is not in the file, it is a root");
                        else if (!scene.SetParent(e, it->second)) ctx.warnings.push_back(who + ": its parent would make a cycle, it is a root");
                    }
                }

                // ---- transform
                if (j.Has("transform"))
                {
                    TransformComponent& t = e.Get<TransformComponent>();
                    const Value& tj = j["transform"];
                    if (tj.Has("position") && !ReadFloats(tj["position"], &t.position.x, 3)) ctx.warnings.push_back(who + ": position is not 3 numbers, kept the default");
                    if (tj.Has("scale") && !ReadFloats(tj["scale"], &t.scale.x, 3)) ctx.warnings.push_back(who + ": scale is not 3 numbers, kept the default");
                    if (tj.Has("rotation"))
                    {
                        float q[4];
                        if (!ReadFloats(tj["rotation"], q, 4)) ctx.warnings.push_back(who + ": rotation is not 4 numbers, kept the default");
                        else
                        {
                            const float len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
                            if (len < 1e-6f) ctx.warnings.push_back(who + ": rotation is a zero quaternion, kept the default");
                            else if (std::fabs(len - 1.0f) <= 1e-4f) t.rotation = { q[0], q[1], q[2], q[3] };      // already unit: stored bit for bit
                            else t.rotation = { q[0] / len, q[1] / len, q[2] / len, q[3] / len };                  // a hand-written one: made unit
                        }
                    }
                }

                // ---- light
                if (j["light"].IsObject())
                {
                    const Value& lj = j["light"];
                    LightComponent l;
                    if (lj.Has("type") && !ParseEnum(kLightTypes, lj["type"].AsString(), l.type)) ctx.warnings.push_back(who + ": unknown light type '" + lj["type"].AsString() + "', used point");
                    if (lj.Has("color") && !ReadFloats(lj["color"], &l.color.x, 3)) ctx.warnings.push_back(who + ": light color is not 3 numbers, kept the default");
                    const auto num = [&](const char* key, float& dst) { if (lj.Has(key) && !ReadFloat(lj[key], dst)) ctx.warnings.push_back(who + ": light '" + key + "' is not a number, kept the default"); };
                    num("intensity", l.intensity); num("range", l.range); num("innerCone", l.innerConeDegrees); num("outerCone", l.outerConeDegrees);
                    l.castsShadows = lj["castsShadows"].AsBool(l.castsShadows);
                    e.Add<LightComponent>(l);
                }

                if (j["skybox"].AsBool())  e.Add<SkyboxComponent>();
                if (j["skinned"].AsBool()) e.Add<SkinnedMeshComponent>();

                // ---- mesh renderer
                if (j["meshRenderer"].IsObject())
                {
                    const Value& rj = j["meshRenderer"];
                    const std::string meshName = rj["mesh"].AsString();
                    auto mesh = ctx.Cached(ctx.meshes, ctx.triedMeshes, meshName, [&](const std::string& n) { return ctx.bridge.ResolveMesh(n); });
                    if (!mesh) ctx.warnings.push_back(who + ": the mesh '" + meshName + "' could not be loaded, it has no mesh renderer");
                    else
                    {
                        MeshRendererComponent r;
                        r.mesh = mesh;
                        const Value& materials = rj["materials"];
                        for (size_t i = 0; i < materials.Size(); ++i)
                        {
                            const Value& entry = materials.At(i);
                            std::shared_ptr<Material> m;
                            if (entry.IsObject() && entry.Has("ref"))
                            {
                                const std::string ref = entry["ref"].AsString();
                                m = ctx.Cached(ctx.namedMaterials, ctx.triedMaterials, ref, [&](const std::string& n) { return ctx.bridge.ResolveMaterial(n); });
                                if (!m) ctx.warnings.push_back(who + ": the material '" + ref + "' could not be loaded, the mesh's own is used");
                            }
                            else if (entry.IsObject() && entry.Has("material"))
                            {
                                const int index = entry["material"].AsInt(-1);
                                if (index >= 0 && static_cast<size_t>(index) < ctx.materials.size()) m = ctx.materials[static_cast<size_t>(index)];
                                else ctx.warnings.push_back(who + ": material #" + std::to_string(index) + " is not in the file, the mesh's own is used");
                            }
                            r.materials.push_back(m);                         // null = the mesh's own material
                        }
                        r.visible = rj["visible"].AsBool(true);
                        r.castsShadows = rj["castsShadows"].AsBool(true);
                        r.receivesShadows = rj["receivesShadows"].AsBool(true);
                        e.Add<MeshRendererComponent>(std::move(r));
                    }
                }

                // ---- animator
                if (j["animator"].IsObject())
                {
                    const Value& aj = j["animator"];
                    const std::string modelName = aj["model"].AsString();
                    auto model = ctx.Cached(ctx.models, ctx.triedModels, modelName, [&](const std::string& n) { return ctx.bridge.ResolveModel(n); });
                    if (!model || !model->skeleton) ctx.warnings.push_back(who + ": the model '" + modelName + "' could not be loaded (or has no skeleton), it has no animator");
                    else
                    {
                        AnimatorComponent a;
                        a.skeleton = model->skeleton;
                        a.clips = model->clips;
                        a.clip = aj["clip"].AsInt(0);
                        const auto num = [&](const char* key, float& dst) { if (aj.Has(key) && !ReadFloat(aj[key], dst)) ctx.warnings.push_back(who + ": animator '" + key + "' is not a number, kept the default"); };
                        num("speed", a.speed); num("time", a.time);
                        if (aj.Has("mode") && !ParseEnum(kModes, aj["mode"].AsString(), a.mode)) ctx.warnings.push_back(who + ": unknown playback mode '" + aj["mode"].AsString() + "', used loop");
                        a.playing = aj["playing"].AsBool(true);
                        e.Add<AnimatorComponent>(std::move(a));
                    }
                }
            }
        }

        // ==================================================================== public
        SaveResult Save(Scene& scene, IAssetBridge& bridge)
        {
            SaveResult result;
            SaveContext ctx{ bridge, result.warnings, {}, {} };

            // Roots in the order their ids were stored (creation order; a view would walk it newest first). Loading creates
            // entities in file order, so a saved-then-loaded scene saves the same text again.
            Value entities = Value::MakeArray();
            const auto& ids = scene.Registry().storage<IdComponent>();
            std::vector<entt::entity> order(ids.data(), ids.data() + ids.size());
            for (entt::entity id : order)
            {
                Entity e = scene.Wrap(id);
                if (!scene.GetParent(e).IsValid())               // each root is followed by its subtree
                    WriteEntity(scene, e, ctx, entities);
            }

            Value materials = Value::MakeArray();
            for (size_t i = 0; i < ctx.inlineMaterials.size(); ++i)      // (the table can grow while earlier ones are written: textures only)
                materials.Push(MaterialToJson(*ctx.inlineMaterials[i], ctx, "#" + std::to_string(i)));

            Value doc = Value::MakeObject();
            doc.Set("format", kFormat).Set("version", kVersion);
            if (materials.Size() > 0) doc.Set("materials", materials);
            doc.Set("entities", entities);
            result.text = doc.Dump(2);
            return result;
        }

        LoadResult Load(const std::string& text, Scene& into, IAssetBridge& bridge)
        {
            LoadResult result;
            const Json::ParseResult parsed = Json::Parse(text);
            if (!parsed.ok) { result.error = "not a valid scene file: " + parsed.error; return result; }

            const Value& root = parsed.value;
            if (!root.IsObject() || root["format"].AsString() != kFormat) { result.error = "not a DXEngine scene file (the 'format' key is missing or different)"; return result; }
            const int version = root["version"].AsInt(0);
            if (version < 1) { result.error = "the scene file has no valid version"; return result; }
            if (version > kVersion) { result.error = "the scene file is version " + std::to_string(version) + ", this build reads up to version " + std::to_string(kVersion) + ": update the engine"; return result; }
            if (!root["entities"].IsArray()) { result.error = "the scene file has no 'entities' list"; return result; }

            LoadContext ctx{ bridge, result.warnings, {}, {}, {}, {}, {}, {}, {} };

            const Value& materials = root["materials"];
            for (size_t i = 0; i < materials.Size(); ++i)
            {
                if (!materials.At(i).IsObject()) { result.warnings.push_back("material #" + std::to_string(i) + " is not an object, replaced by a default one"); ctx.materials.push_back(std::make_shared<Material>()); continue; }
                ctx.materials.push_back(MaterialFromJson(materials.At(i), ctx, i));
            }

            Scene fresh;                                   // built on the side: a failure half way leaves the caller's scene alone
            const Value& entities = root["entities"];
            std::vector<Entity> created(entities.Size());
            std::unordered_map<Uuid, Entity> byId;

            // pass 1: every entity exists, so a parent listed after its child can still be found
            for (size_t i = 0; i < entities.Size(); ++i)
            {
                const Value& j = entities.At(i);
                if (!j.IsObject()) { result.warnings.push_back("entity #" + std::to_string(i) + " is not an object, skipped"); continue; }
                const std::string name = j["name"].IsString() ? j["name"].AsString() : "Entity";
                Uuid id;
                const bool hasId = Uuid::Parse(j["id"].AsString(), id) && !id.IsNull();
                Entity e = hasId ? fresh.CreateEntityWithUuid(name, id) : Entity();
                if (!e.IsValid())
                {
                    result.warnings.push_back(hasId ? "entity '" + name + "' repeats the id " + id.ToString() + ", it got a new one"
                                                    : "entity '" + name + "' has no valid id, it got a new one");
                    e = fresh.CreateEntity(name);
                }
                else byId.emplace(id, e);
                created[i] = e;
            }

            // pass 2: parents, transforms, components
            for (size_t i = 0; i < entities.Size(); ++i)
                if (created[i].IsValid())
                    ApplyEntity(fresh, created[i], entities.At(i), byId, ctx);

            result.entities = fresh.EntityCount();
            into = std::move(fresh);
            result.ok = true;
            return result;
        }
    }
}
