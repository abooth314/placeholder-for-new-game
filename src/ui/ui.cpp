#include "ui/ui.hpp"
#include "ui/console.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <imgui.h>
#include <numbers>

namespace curved {
static ImVec4 tint(std::array<float, 3> c) {
    return {c[0], c[1], c[2], 1};
}
static ImU32 color(std::array<float, 3> c, float alpha = 1) {
    return ImGui::ColorConvertFloat4ToU32({c[0], c[1], c[2], alpha});
}
static std::string shortcut(const char *label, Action action, const Input &input) {
    return std::string(label) + " [" + SDL_GetScancodeName(input.binding(action)) + "]";
}
void focus_battle(World &world, CameraRig &camera) {
    world.release();
    const auto handles = world.actors();
    const auto &h = world.terrain().space();
    h3::Vec chosen = world.terrain().surface(h3::origin());
    double highest = -1;
    const std::size_t stride = std::max<std::size_t>(1, (handles.size() + 63) / 64);
    for (std::size_t i = 0; i < handles.size(); i += stride) {
        const auto *actor = world.inspect(handles[i]);
        double score = 0;
        for (auto handle : handles)
            if (const auto *other = world.inspect(handle)) {
                const double distance = h.dist(actor->position, other->position);
                if (distance < 20)
                    score +=
                        (world.relation(actor->faction, other->faction) == -1 ? 3 : 1) * (1 - distance / 20);
            }
        if (score > highest) {
            highest = score;
            chosen = actor->position;
        }
    }
    camera.focus(chosen, 36);
}
static constexpr ImGuiWindowFlags hud = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus |
                                        ImGuiWindowFlags_NoFocusOnAppearing;
void style_ui() {
    auto &s = ImGui::GetStyle();
    s.WindowRounding = 7;
    s.FrameRounding = 4;
    s.PopupRounding = 7;
    s.WindowPadding = {16, 14};
    s.FramePadding = {10, 7};
    s.ItemSpacing = {9, 9};
    s.ScrollbarSize = 11;
    s.WindowBorderSize = 1;
    auto *c = s.Colors;
    c[ImGuiCol_TitleBg] = {.08f, .15f, .10f, 1};
    c[ImGuiCol_TitleBgActive] = {.18f, .25f, .15f, 1};
    c[ImGuiCol_Text] = {.91f, .88f, .79f, 1};
    c[ImGuiCol_TextDisabled] = {.57f, .67f, .58f, 1};
    c[ImGuiCol_WindowBg] = {.055f, .105f, .085f, .96f};
    c[ImGuiCol_PopupBg] = {.055f, .105f, .085f, .99f};
    c[ImGuiCol_Border] = {.6f, .54f, .32f, .38f};
    c[ImGuiCol_FrameBg] = {.12f, .19f, .14f, 1};
    c[ImGuiCol_FrameBgHovered] = {.22f, .30f, .19f, 1};
    c[ImGuiCol_Button] = {.16f, .23f, .16f, 1};
    c[ImGuiCol_ButtonHovered] = {.30f, .36f, .21f, 1};
    c[ImGuiCol_ButtonActive] = {.46f, .43f, .25f, 1};
    c[ImGuiCol_Header] = {.22f, .31f, .20f, 1};
    c[ImGuiCol_HeaderHovered] = {.33f, .38f, .25f, 1};
    c[ImGuiCol_CheckMark] = {.83f, .72f, .43f, 1};
    c[ImGuiCol_SliderGrab] = {.79f, .68f, .41f, 1};
    c[ImGuiCol_PlotHistogram] = {.62f, .74f, .43f, 1};
}
void toggle_possession(UiState &ui, World &w, CameraRig &camera) {
    if (w.possessed()) {
        w.release();
        ui.message = "Unit returned to its company.";
        camera.distance = 30;
        camera.pitch = .85;
    } else if (w.possess(ui.selected)) {
        const auto *a = w.inspect(ui.selected);
        camera.focus(a->position, 11);
        camera.pitch = .33;
        ui.message = "You possess this unit. Movement bindings move; left mouse uses its capability. Possess "
                     "again to release.";
    } else
        ui.message = "Select a live unit to possess it.";
    ui.player_target = {};
}
UiRequests draw_ui(UiState &ui, World &w, Renderer &r, Audio &audio, CameraRig &camera, Input &input,
                   Settings &settings, int width, int height, double frame_ms) {
    UiRequests requests;
    const auto &c = w.catalog();
    const auto &terrain = w.terrain();
    const auto &h = terrain.space();
    const auto view = camera.view(h, static_cast<double>(width) / height);
    ImGui::SetNextWindowPos({16, 14});
    ImGui::SetNextWindowSize({static_cast<float>(width - 32), 76});
    ImGui::Begin("Battle status", nullptr, hud);
    ImGui::BeginGroup();
    ImGui::TextColored({.84f, .74f, .46f, 1}, "ASTERHALLOW GAMES");
    ImGui::SetWindowFontScale(1.15f);
    ImGui::TextUnformatted("CURVED EARTH");
    ImGui::SetWindowFontScale(1);
    ImGui::EndGroup();
    for (int team = 0; team < 4; ++team) {
        ImGui::SameLine((width < 1200 ? 170 : 232) + team * ((width - (width < 1200 ? 170 : 232) - 160) / 4));
        ImGui::PushID(team);
        ImGui::BeginGroup();
        ImGui::TextColored(tint(c.factions[team].color), "%s   %u", c.factions[team].short_name.c_str(),
                           w.factions()[team].alive);
        if (ImGui::SmallButton(width < 1200 ? "Visit" : "Visit fortress")) {
            w.release();
            camera.focus(w.fortresses()[team].position, 40);
        }
        if (width >= 1200) {
            ImGui::SameLine();
            ImGui::TextDisabled("%u kills", w.factions()[team].kills);
        }
        ImGui::EndGroup();
        ImGui::PopID();
    }
    ImGui::SameLine(static_cast<float>(width - 180));
    ImGui::BeginGroup();
    ImGui::Text("%02d:%02d  %s", static_cast<int>(w.time()) / 60, static_cast<int>(w.time()) % 60,
                ui.paused ? "PAUSED" : "LIVE");
    ImGui::TextDisabled("%.0f FPS", 1000 / std::max(.001, frame_ms));
    ImGui::EndGroup();
    ImGui::End();
    ImGui::SetNextWindowPos({16, 103});
    ImGui::SetNextWindowSize({178, 0});
    ImGui::Begin("View commands", nullptr, hud);
    if (ImGui::Button(shortcut("Battlefields", Action::Maps, input).c_str(), {-1, 33}))
        ui.maps = true;
    if (ImGui::Button(shortcut("Diplomacy", Action::Diplomacy, input).c_str(), {-1, 33}))
        ui.diplomacy = true;
    if (ImGui::Button(shortcut("War council", Action::Council, input).c_str(), {-1, 33})) {
        ui.council = !ui.council;
        ui.inspector = false;
    }
    if (ImGui::Button(shortcut("Inspector", Action::Inspect, input).c_str(), {-1, 33})) {
        ui.inspector = !ui.inspector;
        ui.council = false;
    }
    if (ImGui::Button(shortcut("Field guide", Action::Help, input).c_str(), {-1, 29}))
        ui.help = true;
    ImGui::Spacing();
    ImGui::TextWrapped("%s", terrain.recipe().name.c_str());
    ImGui::TextDisabled("Seed %u  |  L %.0f m", terrain.seed(), h.radius());
    ImGui::End();
    ImGui::SetNextWindowPos({16, static_cast<float>(height - 249)});
    ImGui::SetNextWindowSize({214, 233});
    ImGui::Begin("Tactical atlas", nullptr, hud);
    ImGui::TextColored({.75f, .72f, .51f, 1}, "TACTICAL ATLAS");
    ImGui::SameLine();
    ImGui::TextDisabled("Klein");
    const ImVec2 atlas_origin = ImGui::GetCursorScreenPos();
    ImGui::Image(static_cast<ImTextureID>(r.atlas_texture()), {180, 180});
    const bool atlas_hover = ImGui::IsItemHovered();
    auto *draw = ImGui::GetWindowDrawList();
    auto atlas_point = [&](h3::Vec p) {
        const auto k = h.klein(p);
        return ImVec2{atlas_origin.x + 90 + static_cast<float>(k[0] / terrain.extent() * 82.8),
                      atlas_origin.y + 90 + static_cast<float>(k[1] / terrain.extent() * 82.8)};
    };
    for (auto handle : w.actors())
        if (auto *a = w.inspect(handle))
            draw->AddCircleFilled(atlas_point(a->position), handle == ui.selected ? 3.4f : 1.7f,
                                  color(c.factions[a->faction].color));
    for (auto &f : w.fortresses()) {
        auto p = atlas_point(f.position);
        draw->AddRectFilled({p.x - 3, p.y - 3}, {p.x + 3, p.y + 3}, color(c.factions[f.faction].color));
    }
    draw->AddCircle(atlas_point(camera.target), 5.5, IM_COL32(250, 228, 156, 255), 20, 1.3f);
    if (atlas_hover && ImGui::IsMouseClicked(0) && !w.possessed()) {
        const auto mouse = ImGui::GetMousePos();
        double x = (mouse.x - atlas_origin.x - 90) / 82.8 * terrain.extent(),
               z = (mouse.y - atlas_origin.y - 90) / 82.8 * terrain.extent();
        const double n = std::hypot(x, z), max = terrain.extent() * .95;
        if (n > max) {
            x *= max / n;
            z *= max / n;
        }
        camera.focus(terrain.surface(h.from_klein(x, z)), camera.distance);
    }
    ImGui::End();
    ImGui::SetNextWindowPos({std::max(244.f, (width - 764.f) * .5f), static_cast<float>(height - 71)});
    ImGui::SetNextWindowSize({std::min(764.f, width - 260.f), 55});
    ImGui::Begin("Time and camera", nullptr, hud);
    if (ImGui::Button(shortcut(ui.paused ? "Resume" : "Pause", Action::Pause, input).c_str()))
        ui.paused = !ui.paused;
    for (double speed : {.5, 1., 2., 4.}) {
        ImGui::SameLine();
        char label[16];
        SDL_snprintf(label, sizeof(label), "%gx", speed);
        if (ImGui::Button(label)) {
            settings.speed = speed;
            requests.save = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(shortcut("Battle", Action::Focus, input).c_str())) {
        focus_battle(w, camera);
    }
    ImGui::SameLine();
    if (ImGui::Button(shortcut("Overview", Action::Overview, input).c_str())) {
        w.release();
        camera.overview(terrain.surface(h3::origin()));
    }
    ImGui::SameLine();
    if (ImGui::Button(shortcut(camera.free ? "Ground" : "Fly", Action::Freecam, input).c_str())) {
        w.release();
        camera.toggle_free(h, view.aspect);
    }
    ImGui::SameLine();
    if (ImGui::Button(settings.mute ? "Unmute" : "Mute")) {
        settings.mute = !settings.mute;
        requests.save = true;
    }
    ImGui::End();
    if (!ui.message.empty()) {
        ImGui::SetNextWindowPos({std::max(244.f, width * .5f - 340), 103});
        ImGui::SetNextWindowSize({std::min(680.f, width - 480.f), 0});
        ImGui::SetNextWindowBgAlpha(.80f);
        ImGui::Begin("Message", nullptr, hud | ImGuiWindowFlags_NoInputs);
        ImGui::TextWrapped("%s", ui.message.c_str());
        ImGui::End();
    }
    if (ui.maps)
        ImGui::OpenPopup("Choose your battlefield");
    ImGui::SetNextWindowSize({660, 390}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Choose your battlefield", &ui.maps, ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("Seeded terrain and legal routes on a genuinely curved surface.");
        ImGui::InputInt("World seed", &settings.seed);
        settings.seed = std::clamp(settings.seed, 1, 99999999);
        for (int i = 0; i < static_cast<int>(c.maps.size()); ++i) {
            ImGui::PushID(i);
            ImGui::Separator();
            ImGui::TextColored({.86f, .75f, .48f, 1}, "%s", c.maps[i].name.c_str());
            ImGui::TextWrapped("%s", c.maps[i].description.c_str());
            if (ImGui::Button("Enter battlefield", {210, 34})) {
                requests.rebuild = true;
                requests.map = i;
                ui.maps = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Return to battle")) {
            ui.maps = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ui.diplomacy)
        ImGui::OpenPopup("The table of kingdoms");
    ImGui::SetNextWindowSize({740, 450}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("The table of kingdoms", &ui.diplomacy, ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("Each row decides its outgoing attitude. Click to cycle enemy, ally and neutral.");
        for (auto &p : c.presets) {
            if (ImGui::Button(p.name.c_str()))
                w.set_preset(p.id);
            ImGui::SameLine();
        }
        ImGui::NewLine();
        if (ImGui::BeginTable("Relations", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("ROW VIEWS ->");
            for (auto &f : c.factions) {
                ImGui::TableNextColumn();
                ImGui::TextColored(tint(f.color), "%s", f.short_name.c_str());
            }
            for (int from = 0; from < 4; ++from) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(tint(c.factions[from].color), "%s", c.factions[from].short_name.c_str());
                for (int to = 0; to < 4; ++to) {
                    ImGui::TableNextColumn();
                    if (from == to) {
                        ImGui::TextDisabled("Own kingdom");
                        continue;
                    }
                    ImGui::PushID(from * 4 + to);
                    const auto value = w.relation(from, to);
                    if (ImGui::Button(value < 0 ? "Enemy" : value > 0 ? "Ally" : "Neutral", {-1, 32}))
                        w.set_relation(from, to, value < 0 ? 1 : value > 0 ? 0 : -1, ui.mirror);
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
        ImGui::Checkbox("Mirror edits in both directions", &ui.mirror);
        ImGui::TextWrapped("Neutral kingdoms hold fire and offer no support. Asymmetric relations are "
                           "allowed; neutral units do not automatically retaliate.");
        if (ImGui::Button("Return to battle")) {
            ui.diplomacy = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ui.council) {
        ImGui::SetNextWindowPos({static_cast<float>(width - 366), 103});
        ImGui::SetNextWindowSize({350, static_cast<float>(height - 186)});
        ImGui::Begin("War council", &ui.council, ImGuiWindowFlags_NoSavedSettings);
        if (ImGui::BeginTabBar("Council tabs")) {
            if (ImGui::BeginTabItem("Armies")) {
                int cap = static_cast<int>(w.population_limit);
                if (ImGui::SliderInt("Population", &cap, 40, 384)) {
                    w.population_limit = cap;
                    settings.population = cap;
                    requests.save = true;
                }
                float reinforce = static_cast<float>(w.reinforce_rate);
                if (ImGui::SliderFloat("Reinforce", &reinforce, 0, 3, "%.2fx")) {
                    w.reinforce_rate = reinforce;
                    settings.reinforce_rate = reinforce;
                    requests.save = true;
                }
                if (ImGui::Checkbox("Autonomous warfare", &w.combat)) {
                    settings.combat = w.combat;
                    requests.save = true;
                }
                ImGui::SeparatorText("Call a detachment");
                ImGui::Combo(
                    "Faction", &ui.spawn_faction,
                    [](void *data, int i) {
                        return static_cast<const Catalog *>(data)->factions[i].name.c_str();
                    },
                    const_cast<Catalog *>(&c), 4);
                std::vector<const UnitDef *> available;
                for (auto &[id, u] : c.units)
                    if (u.discover && u.menu)
                        available.push_back(&u);
                if (!available.empty()) {
                    ui.spawn_unit = std::clamp(ui.spawn_unit, 0, static_cast<int>(available.size()) - 1);
                    ImGui::Combo(
                        "Unit", &ui.spawn_unit,
                        [](void *data, int i) {
                            return (*static_cast<std::vector<const UnitDef *> *>(data))[i]->name.c_str();
                        },
                        &available, static_cast<int>(available.size()));
                    if (ImGui::Button("Deploy 8 soldiers", {-1, 34})) {
                        auto made = w.spawn_detachment(available[ui.spawn_unit]->id, ui.spawn_faction, 8);
                        ui.message = made.empty()
                                         ? "Deployment rejected by spawn policy or population budget."
                                         : "Detachment deployed at its fortress.";
                    }
                }
                if (ImGui::Button("Restart this battlefield", {-1, 30})) {
                    requests.rebuild = true;
                    requests.map = settings.map;
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("World")) {
                float L = static_cast<float>(settings.curvature);
                if (ImGui::SliderFloat("Curvature L", &L, 30, 100, "%.0f m"))
                    settings.curvature = L;
                if (ImGui::Button("Rebuild with this curvature", {-1, 32})) {
                    requests.rebuild = true;
                    requests.map = settings.map;
                }
                requests.save |= ImGui::Checkbox("Detailed scenery", &settings.detailed);
                requests.save |= ImGui::Checkbox("Wounded health bars", &settings.health_bars);
                if (ImGui::Checkbox("VSync", &settings.vsync)) {
                    requests.vsync_changed = true;
                    requests.save = true;
                }
                ImGui::SeparatorText("Semantic bindings");
                ImGui::TextWrapped("Select a binding, then press a key. Escape cancels.");
                for (unsigned i = 0; i < static_cast<unsigned>(Action::Count); ++i) {
                    ImGui::PushID(i);
                    ImGui::TextUnformatted(action_name(static_cast<Action>(i)));
                    ImGui::SameLine(200);
                    if (ImGui::Button(SDL_GetScancodeName(input.binding(static_cast<Action>(i))), {100, 0}))
                        input.rebind(static_cast<Action>(i));
                    ImGui::PopID();
                }
                if (input.binding())
                    ImGui::TextColored({1, .8f, .4f, 1}, "Waiting for a key...");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Sound")) {
                requests.save |= ImGui::Checkbox("Mute", &settings.mute);
                requests.save |= ImGui::SliderFloat("Master", &settings.master, 0, 1, "%.2f");
                requests.save |= ImGui::SliderFloat("Effects", &settings.effects, 0, 1, "%.2f");
                requests.save |= ImGui::SliderFloat("Ambience", &settings.ambience, 0, 1, "%.2f");
                ImGui::Text("Native device: %s", audio.stats().device ? "open" : "unavailable");
                ImGui::Text("Voices %u / 24; peak %u", audio.stats().voices, audio.stats().peak_voices);
                if (!audio.stats().error.empty())
                    ImGui::TextWrapped("%s", audio.stats().error.c_str());
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }
    if (auto *a = w.inspect(ui.selected); a && !ui.council && !ui.inspector) {
        const auto &u = c.units.at(a->base);
        const auto &attack = c.attacks.at(u.attack);
        ImGui::SetNextWindowPos({static_cast<float>(width - 324), static_cast<float>(height - 438)});
        ImGui::SetNextWindowSize({308, 350});
        ImGui::Begin("Selected unit", nullptr, hud);
        ImGui::TextColored(tint(c.factions[a->faction].color), "%s", c.factions[a->faction].name.c_str());
        ImGui::SetWindowFontScale(1.25);
        ImGui::TextUnformatted(u.name.c_str());
        ImGui::SetWindowFontScale(1);
        ImGui::ProgressBar(static_cast<float>(a->values.health() / a->values.effective(Value::MaxHealth)),
                           {-1, 9}, "");
        ImGui::Text("Health %.0f / %.0f | Morale %.0f%%", a->values.health(),
                    a->values.effective(Value::MaxHealth), a->values.morale * 100);
        ImGui::TextWrapped("%s", state_name(a->state));
        ImGui::Separator();
        ImGui::Text("%s %.0f  |  Range %.1f m", attack.mode == AttackMode::Heal ? "Healing" : "Damage",
                    attack.damage * a->values.effective(attack.mode == AttackMode::Heal
                                                            ? Value::HealMultiplier
                                                            : Value::DamageMultiplier),
                    attack.range);
        ImGui::Text("Speed %.2f m/s  |  last step %.2f", a->values.effective(Value::MoveSpeed),
                    a->values.velocity);
        ImGui::Text("Cooldown %.2f s | Squad %u", a->cooldown, a->squad);
        if (ImGui::Button(
                shortcut(w.possessed() == a->handle ? "Release unit" : "Possess unit", Action::Possess, input)
                    .c_str(),
                {-1, 34}))
            toggle_possession(ui, w, camera);
        if (ImGui::Button("Follow")) {
            camera.follow = true;
            camera.free = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Center"))
            camera.focus(a->position, 24);
        ImGui::SameLine();
        if (ImGui::Button("Inspect"))
            ui.inspector = true;
        ImGui::TextDisabled(w.possessed() ? "Movement bindings | LMB primary action"
                                          : "Select any compatible unit to take control.");
        ImGui::End();
    }
    if (ui.inspector) {
        ImGui::SetNextWindowPos({static_cast<float>(width - 414), 103});
        ImGui::SetNextWindowSize({398, static_cast<float>(height - 185)});
        ImGui::Begin("Native inspector", &ui.inspector, ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextColored({.85f, .75f, .48f, 1}, "THE CURVED EARTH");
        ImGui::Text("L %.1f m  |  K %.6f m^-2", h.radius(), -1 / (h.radius() * h.radius()));
        ImGui::Text("20 m circle perimeter %.2f m",
                    2 * std::numbers::pi * h.radius() * std::sinh(20 / h.radius()));
        ImGui::Text("Camera height %.2f m  |  %s", h.height(view.position),
                    camera.free ? "free flight" : "ground/orbit");
        ImGui::Text("Navigation %zu nodes", terrain.nodes().size());
        ImGui::Text("Actors %zu / %u; shots %zu", w.actors().size(), w.population_limit,
                    w.projectiles().size());
        ImGui::Text("Tick %.3f ms; frame %.2f ms", w.stats().last_tick_ms, frame_ms);
        ImGui::Text("Draws %u; triangles %u", r.stats().draw_calls, r.stats().triangles);
        ImGui::Text("Meshes %u; buffers %u; textures %u", r.stats().models, r.stats().buffers,
                    r.stats().textures);
        ImGui::Text("GPU timer: unavailable");
        ImGui::Text("Lua calls %llu / errors %llu", static_cast<unsigned long long>(w.stats().lua_calls),
                    static_cast<unsigned long long>(w.stats().lua_errors));
        ImGui::TextWrapped("GPU: %s", r.stats().gpu.c_str());
        ImGui::Checkbox("Show company routes", &ui.show_paths);
        if (auto *a = w.inspect(ui.selected)) {
            ImGui::SeparatorText("Live actor");
            ImGui::TextWrapped("BaseId %s", a->base.c_str());
            ImGui::Text("RefId %llu", static_cast<unsigned long long>(a->handle.ref));
            ImGui::Text("Handle valid; generation %u / epoch %u", a->handle.generation, a->handle.epoch);
            ImGui::Text("Faction %d / controller %s", a->faction,
                        a->controller == Controller::Player ? "player" : "squad AI");
            ImGui::Text("P (%.4f, %.4f, %.4f, %.4f)", a->position.t, a->position.x, a->position.y,
                        a->position.z);
            ImGui::Text("Camera geodesic %.2f m", h.dist(a->position, view.position));
            if (ImGui::Button("Measure surface route to home")) {
                auto path = terrain.path(terrain.nearest(a->position), terrain.homes()[a->faction]);
                ui.nav_length = path.empty() ? -1 : terrain.route_length(path);
            }
            if (ui.nav_length >= 0)
                ImGui::Text("Sampled route %.2f m", ui.nav_length);
            for (unsigned i = 0; i < static_cast<unsigned>(Value::Count); ++i) {
                const auto v = static_cast<Value>(i);
                if (ActorValues::writable(v))
                    ImGui::Text("%s: %.3f (base %.3f)", ActorValues::name(v).data(), a->values.effective(v),
                                a->values.base(v));
                else
                    ImGui::Text("%s: %.3f (read only)", ActorValues::name(v).data(), a->values.effective(v));
            }
            for (auto &m : a->values.modifiers())
                ImGui::TextWrapped("%s: %s +%.2f x%.2f until %llu", m.source.c_str(),
                                   ActorValues::name(m.value).data(), m.additive, m.multiplier,
                                   static_cast<unsigned long long>(m.expires));
            const auto &def = c.units.at(a->base);
            ImGui::Text("Capabilities: 0x%x", def.capabilities);
            if (ImGui::Button(
                    shortcut(w.possessed() ? "Release" : "Possess", Action::Possess, input).c_str()))
                toggle_possession(ui, w, camera);
        }
        ImGui::SeparatorText("Positioned objects");
        for (auto &f : w.fortresses()) {
            ImGui::PushID(f.faction);
            if (ImGui::SmallButton(c.factions[f.faction].short_name.c_str()))
                ui.selected = f.handle;
            ImGui::SameLine();
            ImGui::Text("Fortress %.0f / %.0f %s", f.health, f.max_health, f.ruined ? "ruined" : "standing");
            if (ui.selected == f.handle)
                ImGui::Text("Ref %llu / H %.1fm", static_cast<unsigned long long>(f.handle.ref),
                            h.height(f.position));
            ImGui::PopID();
        }
        ImGui::SeparatorText("Native console");
        if (ImGui::InputText("##console", ui.command, sizeof(ui.command),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            auto result = execute_console(w, ui.command);
            ui.message = result.text;
            if (result.selection)
                ui.selected = result.selection;
            ui.command[0] = '\0';
        }
        ImGui::TextWrapped("inspect <RefId> | spawn <BaseId> <faction>\nset <RefId> <value> <base> | mod / "
                           "unmod\npossess <RefId> | release | relation");
        for (auto &e : w.errors())
            ImGui::TextWrapped("%s", e.c_str());
        for (auto &pack : c.packs)
            ImGui::TextDisabled("Enabled pack: %s", pack.c_str());
        ImGui::End();
    }
    if (ui.help)
        ImGui::OpenPopup("Field guide");
    ImGui::SetNextWindowSize({680, 550}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Field guide", &ui.help, ImGuiWindowFlags_NoResize)) {
        ImGui::TextColored({.85f, .75f, .48f, 1}, "WATCH THE WAR. JOIN ANY COMPANY.");
        ImGui::TextWrapped(
            "Select a soldier and possess it. Every supported weapon, healing action, cooldown, movement "
            "modifier and faction rule continues through the same native actor mechanics.");
        for (unsigned i = 0; i < static_cast<unsigned>(Action::Count); ++i) {
            ImGui::Text("%-25s  %s", action_name(static_cast<Action>(i)),
                        SDL_GetScancodeName(input.binding(static_cast<Action>(i))));
        }
        ImGui::TextWrapped(
            "Mouse wheel zooms. Right-drag orbits or looks in free flight. Left click selects; while "
            "possessed, hold it to attack or heal an eligible target. WASD moves the unit relative to the "
            "camera. V releases possession for free flight. R/C rise/drop; Shift moves the camera faster. "
            "Escape closes panels or releases the unit. UI input is captured before world actions.");
        if (ImGui::Button("Return to the battle")) {
            ui.help = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return requests;
}
void draw_world_overlays(const UiState &ui, const World &w, const Camera &camera, int width, int height,
                         bool bars) {
    auto *draw = ImGui::GetBackgroundDrawList();
    const auto &h = w.terrain().space();
    for (auto handle : w.actors())
        if (auto *a = w.inspect(handle)) {
            if (bars &&
                (a->values.health() < a->values.effective(Value::MaxHealth) || handle == ui.selected)) {
                const auto p = CameraRig::project(h.offset(a->position, 0, 2.8, 0), camera, width, height);
                if (p.visible) {
                    draw->AddRectFilled({static_cast<float>(p.x - 17), static_cast<float>(p.y - 3)},
                                        {static_cast<float>(p.x + 17), static_cast<float>(p.y + 3)},
                                        IM_COL32(12, 24, 17, 230));
                    draw->AddRectFilled(
                        {static_cast<float>(p.x - 16), static_cast<float>(p.y - 2)},
                        {static_cast<float>(p.x - 16 +
                                            32 * a->values.health() / a->values.effective(Value::MaxHealth)),
                         static_cast<float>(p.y + 2)},
                        color(w.catalog().factions[a->faction].color));
                }
            }
            if (handle == ui.selected) {
                ImVec2 points[49];
                int used = 0;
                for (int i = 0; i <= 48; ++i) {
                    const double angle = i * 2 * std::numbers::pi / 48;
                    const auto p = CameraRig::project(
                        h.offset(a->position, std::cos(angle) * 1.3, .15, std::sin(angle) * 1.3), camera,
                        width, height);
                    if (p.visible)
                        points[used++] = {static_cast<float>(p.x), static_cast<float>(p.y)};
                }
                if (used > 2)
                    draw->AddPolyline(points, used, IM_COL32(255, 229, 150, 240), ImDrawFlags_None, 2);
            }
        }
    if (ui.show_paths)
        for (auto &squad : w.squads()) {
            const auto from = CameraRig::project(squad.center, camera, width, height),
                       to = CameraRig::project(squad.objective, camera, width, height);
            if (from.visible && to.visible)
                draw->AddLine({static_cast<float>(from.x), static_cast<float>(from.y)},
                              {static_cast<float>(to.x), static_cast<float>(to.y)},
                              color(w.catalog().factions[squad.faction].color, .65f), 1.3f);
        }
}
} // namespace curved
