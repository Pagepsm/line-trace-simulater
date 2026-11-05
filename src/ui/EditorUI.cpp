#include "ui/EditorUI.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <rlImGui.h>
#include <algorithm>
#include <numbers>
#include "ui/PointerEvents.h"
#include "platform/Paths.h"
#include <fstream>
#include <cstring>

namespace lt {
EditorUI::EditorUI() {
    rlImGuiSetLoadFontsCallback([]() {
        auto& io = ImGui::GetIO();
        std::ifstream file(applicationDirectory() / "assets/fonts/NotoSansCJKjp-Regular.otf",
                           std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(file)), {});
        if (!bytes.empty()) {
            auto data = IM_ALLOC(bytes.size());
            std::memcpy(data, bytes.data(), bytes.size());
            io.Fonts->AddFontFromMemoryTTF(data, static_cast<int>(bytes.size()), 17.f);
        } else
            io.Fonts->AddFontDefault();
    });
    rlImGuiSetup(true);
    ImGui::GetIO().IniFilename = nullptr;
    auto& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.15f);
    style.FrameRounding = 4;
    style.FramePadding = {10, 6};
}

EditorUI::~EditorUI() {
    rlImGuiShutdown();
}

UIActions EditorUI::draw(const Simulation& sim, const ProjectEditor& editor, CodeWorkspace& code,
                         ProjectPanel& project) {
    UIActions actions;
    rlImGuiBegin();
    auto& io = ImGui::GetIO();
    auto display = io.DisplaySize;
    ImGui::SetNextWindowPos({8, 8});
    ImGui::SetNextWindowSize({display.x - 16, display.y - 16});
    ImGui::Begin("ライントレース・シミュレータ", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    const auto& model = editor.model();
    if (selected_ >= static_cast<int>(model.track.strokes.size()))
        selected_ = -1;
    auto cancel = [&] {
        drawing_ = moving_ = placing_ = false;
        draft_.points.clear();
        moveDelta_ = {};
    };
    auto edit = [&](EditCommand c) {
        actions.simulation = SimulationAction::Pause;
        actions.edits.push_back(std::move(c));
    };
    bool busy = drawing_ || moving_ || placing_ || code.busy();
    ImGui::BeginDisabled(busy);
    if (ImGui::Button("新規"))
        actions.project = ProjectAction::New;
    ImGui::SameLine();
    if (ImGui::Button("セーブ"))
        actions.project = ProjectAction::Save;
    ImGui::SameLine();
    if (ImGui::Button("ロード"))
        actions.project = ProjectAction::Load;
    ImGui::SameLine();
    if (ImGui::Button("保存先・読込先"))
        ImGui::OpenPopup("保存先");
    if (ImGui::BeginPopup("保存先")) {
        ImGui::TextUnformatted("プロジェクトの保存先（JSON）");
        ImGui::SetNextItemWidth(650);
        ImGui::InputText("##path", &project.path);
        ImGui::TextUnformatted("コース・設定・C++コードをまとめて保存します。");
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    if (!project.message.empty())
        ImGui::TextWrapped("%s", project.message.c_str());
    ImGui::Separator();
    if (ImGui::RadioButton("1 コース", page_ == 0)) {
        cancel();
        page_ = 0;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("2 制御コード", page_ == 1)) {
        cancel();
        page_ = 1;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(busy);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{.12f, .43f, .32f, 1});
    if (ImGui::Button("3 実行"))
        actions.simulation = SimulationAction::Run;
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button("一時停止"))
        actions.simulation = SimulationAction::Pause;
    ImGui::SameLine();
    if (ImGui::Button("リセット"))
        actions.simulation = SimulationAction::Reset;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Text("%s  %.2f 秒",
                sim.running()                                ? "実行中"
                : code.busy()                                ? "ビルド中"
                : (!sim.error().empty() || code.buildFailed) ? "エラー"
                : code.requiresBuild                         ? "未ビルド"
                                                             : "停止中",
                sim.time());
    if (!sim.error().empty())
        ImGui::TextUnformatted(
            "制御コードの出力を確認してください。詳細は実行データを開いて確認できます。");
    if (ImGui::CollapsingHeader("実行データ・詳細")) {
        auto motor = sim.motors();
        auto b = sim.body();
        ImGui::Text("モータ 左 %.3f / 右 %.3f    位置 %.3f, %.3f m    角度 %.3f rad", motor.left,
                    motor.right, b.pose.position.x, b.pose.position.y, b.pose.angle);
        ImGui::TextUnformatted("センサ（左から順に、1=黒）");
        ImGui::SameLine();
        for (auto sensor : sim.sensors()) {
            ImGui::Text("%d", sensor.black);
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::BeginDisabled(busy || sim.running());
        if (ImGui::Button("1ステップ"))
            actions.simulation = SimulationAction::Step;
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(busy);
        if (ImGui::Button("テスト：追従"))
            actions.simulation = SimulationAction::Follow;
        ImGui::SameLine();
        if (ImGui::Button("テスト：直進"))
            actions.simulation = SimulationAction::Equal;
        ImGui::SameLine();
        if (ImGui::Button("テスト：旋回"))
            actions.simulation = SimulationAction::Differential;
        ImGui::SameLine();
        ImGui::BeginDisabled(!code.runtime->loaded());
        if (ImGui::Button("ビルド済みコードを使用"))
            actions.simulation = SimulationAction::UseDll;
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ImGui::Text("制御：%s", sim.usingDll() ? "C++コード" : "テスト用制御");
        if (!sim.error().empty())
            ImGui::TextWrapped("%s", sim.error().c_str());
    }
    ImGui::Separator();
    if (page_ == 1) {
        takePointerEvents();
        ImGui::BeginDisabled(code.busy());
        if (ImGui::Button("ビルドして反映"))
            actions.code = CodeAction::Build;
        ImGui::SameLine();
        ImGui::TextUnformatted(code.busy() ? "ビルドしています…"
                               : code.buildFailed
                                   ? "ビルドに失敗しました。下の結果を確認してください。"
                               : code.activeGeneration.empty()
                                   ? "コードを編集し、ビルドしてください。"
                                   : "ビルド済みです。変更後はもう一度ビルドしてください。");
        if (ImGui::CollapsingHeader("コードの詳細")) {
            if (ImGui::Button("ソースを保存"))
                actions.code = CodeAction::Save;
            ImGui::SameLine();
            if (ImGui::Button("ソースを読込"))
                actions.code = CodeAction::Load;
            ImGui::Text("コンパイラ：%s",
                        code.compiler.empty() ? "見つかりません" : "GCC（インストール済み）");
            ImGui::TextWrapped("DLL：%s", code.activeGeneration.empty()
                                              ? "未読込"
                                              : code.activeGeneration.c_str());
            ImGui::TextUnformatted(
                "信頼できる本人のC++コードを実行してください。クラッシュや無限ループは隔離しません。");
        }
        auto space = ImGui::GetContentRegionAvail();
        ImGui::InputTextMultiline("##controller_source", &code.source,
                                  {space.x, std::max(100.f, space.y - 140)},
                                  ImGuiInputTextFlags_AllowTabInput);
        ImGui::EndDisabled();
        ImGui::TextUnformatted("ビルド結果");
        ImGui::BeginChild("BuildLog", {0, 0}, true);
        ImGui::TextWrapped("%s", code.log.c_str());
        ImGui::EndChild();
        ImGui::End();
        rlImGuiEnd();
        return actions;
    }
    auto body = sim.body();
    auto toolButton = [&](const char* label, Tool tool) {
        if (ImGui::RadioButton(label, tool_ == tool)) {
            cancel();
            tool_ = tool;
        }
        ImGui::SameLine();
    };
    toolButton("描画", Tool::Draw);
    toolButton("選択・移動", Tool::Select);
    toolButton("消去", Tool::Erase);
    toolButton("開始位置", Tool::Start);
    ImGui::BeginDisabled(drawing_ || moving_ || placing_);
    float widthMm = width_ * 1000;
    ImGui::SetNextItemWidth(145);
    if (ImGui::InputFloat("線幅 mm", &widthMm, 5, 10, "%.0f"))
        width_ = std::clamp(widthMm * .001f, .001f, .2f);
    if (!std::isfinite(width_))
        width_ = .025f;
    if (ImGui::IsItemActivated())
        actions.simulation = SimulationAction::Pause;
    if (selected_ >= 0) {
        if (ImGui::Button("選択した線に線幅を適用")) {
            EditCommand c{EditKind::Width};
            c.index = selected_;
            c.width = width_;
            edit(c);
        }
        ImGui::SameLine();
        if (ImGui::Button("選択した線を削除")) {
            EditCommand c{EditKind::Remove};
            c.index = selected_;
            edit(c);
            selected_ = -1;
        }
    }
    ImGui::BeginDisabled(!editor.canUndo());
    if (ImGui::Button("元に戻す")) {
        edit({EditKind::Undo});
        selected_ = -1;
        poseDirty_ = false;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!editor.canRedo());
    if (ImGui::Button("やり直す")) {
        edit({EditKind::Redo});
        selected_ = -1;
        poseDirty_ = false;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("全消去")) {
        edit({EditKind::Clear});
        selected_ = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("表示を戻す")) {
        camera_.center = {};
        zoom_ = 1;
    }
    ImGui::EndDisabled();
    if (tool_ == Tool::Start && ImGui::CollapsingHeader("開始位置を数値で指定")) {
        if (!poseDirty_) {
            poseFields_[0] = model.start.position.x;
            poseFields_[1] = model.start.position.y;
            poseFields_[2] = model.start.angle * 180 / std::numbers::pi_v<float>;
        }
        ImGui::SetNextItemWidth(300);
        if (ImGui::InputFloat3("X / Y (m) / 向き (度)", poseFields_, "%.3f"))
            poseDirty_ = true;
        if (ImGui::IsItemActive())
            actions.simulation = SimulationAction::Pause;
        if (ImGui::Button("開始位置を適用")) {
            EditCommand c{EditKind::Start};
            c.pose = {{poseFields_[0], poseFields_[1]},
                      poseFields_[2] * std::numbers::pi_v<float> / 180};
            if (std::isfinite(c.pose.angle) && std::isfinite(c.pose.position.x) &&
                std::isfinite(c.pose.position.y) && std::abs(c.pose.position.x) <= 10000 &&
                std::abs(c.pose.position.y) <= 10000) {
                edit(c);
                poseDirty_ = false;
            }
        }
    }
    ImGui::TextUnformatted(placing_              ? "次に、車を向けたい方向をクリックしてください。"
                           : tool_ == Tool::Draw ? "ドラッグして線を描きます。"
                           : tool_ == Tool::Select ? "線をドラッグして移動します。"
                           : tool_ == Tool::Erase  ? "線をクリックすると、その線を丸ごと消します。"
                                                   : "車を置く位置をクリックしてください。");
    if (drawing_ || moving_ || placing_) {
        ImGui::SameLine();
        if (ImGui::Button("キャンセル"))
            cancel();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("操作のヒント");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("ホイール：拡大縮小\n中ボタンドラッグ：表示移動\n右クリック：操作をキャンセル\nCtrl+Z / Ctrl+Y：元に戻す / やり直す\nDelete：選択した線を削除");
    ImGui::BeginChild("CoursePane", {0, 0}, false);
    auto top = ImGui::GetCursorScreenPos(), available = ImGui::GetContentRegionAvail();
    available.x = std::max(available.x, 10.f);
    available.y = std::max(available.y, 10.f);
    ImGui::InvisibleButton("World viewport", available,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle |
                               ImGuiButtonFlags_MouseButtonRight);
    bool hover = ImGui::IsItemHovered();
    camera_.origin = {top.x + available.x * .5f, top.y + available.y * .5f};
    float base = std::min(available.x, available.y) / 1.65f;
    camera_.pixelsPerMeter = base * zoom_;
    Vec2 mouse{io.MousePos.x, io.MousePos.y};
    if (!drawing_ && !moving_ && !placing_) {
        if (hover && io.MouseWheel != 0) {
            auto anchor = camera_.toWorld(mouse);
            zoom_ = std::clamp(zoom_ * std::pow(1.15f, io.MouseWheel), .2f, 12.f);
            camera_.pixelsPerMeter = base * zoom_;
            camera_.center = camera_.center + anchor - camera_.toWorld(mouse);
        }
    }
    auto world = camera_.toWorld(mouse);
    if (hover && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        cancel();
    for (const auto& event : takePointerEvents()) {
        bool inside = event.screen.x >= top.x && event.screen.x < top.x + available.x &&
                      event.screen.y >= top.y && event.screen.y < top.y + available.y;
        pan_.process(event, inside, !drawing_ && !moving_ && !placing_, camera_);
        auto point = camera_.toWorld(event.screen);
        if (event.kind == PointerKind::Cancel) {
            cancel();
            continue;
        }
        if (event.kind == PointerKind::Down && inside) {
            actions.simulation = SimulationAction::Pause;
            if (tool_ == Tool::Draw) {
                drawing_ = true;
                draft_.points = {TrackPoint{point, width_}};
            } else if (tool_ == Tool::Select) {
                selected_ = model.track.pick(point, 5 / camera_.pixelsPerMeter);
                moving_ = selected_ >= 0;
                dragStart_ = point;
                moveDelta_ = {};
                if (selected_ >= 0 && !model.track.strokes[selected_].points.empty())
                    width_ = model.track.strokes[selected_].points[0].width;
            } else if (tool_ == Tool::Erase) {
                int index = model.track.pick(point, 5 / camera_.pixelsPerMeter);
                if (index >= 0) {
                    EditCommand c{EditKind::Remove};
                    c.index = index;
                    edit(c);
                    selected_ = -1;
                }
            } else if (!placing_) {
                placing_ = true;
                pendingStart_ = point;
            } else {
                auto d = point - pendingStart_;
                EditCommand c{EditKind::Start};
                c.pose = {pendingStart_,
                          dot(d, d) > 1e-10f ? std::atan2(d.y, d.x) : model.start.angle};
                edit(c);
                placing_ = false;
                poseDirty_ = false;
            }
        }
        if (event.kind == PointerKind::Move || event.kind == PointerKind::Up) {
            if (drawing_ && inside) {
                auto d = point - draft_.points.back().position;
                if (dot(d, d) > .000001f)
                    draft_.points.push_back({point, width_});
            }
            if (moving_)
                moveDelta_ = point - dragStart_;
        }
        if (event.kind == PointerKind::Up) {
            if (drawing_) {
                EditCommand c{EditKind::Add};
                c.stroke = draft_;
                edit(c);
                draft_.points.clear();
                drawing_ = false;
                selected_ = static_cast<int>(model.track.strokes.size());
            }
            if (moving_) {
                EditCommand c{EditKind::Move};
                c.index = selected_;
                c.delta = moveDelta_;
                edit(c);
                moving_ = false;
                moveDelta_ = {};
            }
        }
    }
    if (!io.WantTextInput && !drawing_ && !moving_ && !placing_) {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && selected_ >= 0) {
            EditCommand c{EditKind::Remove};
            c.index = selected_;
            edit(c);
            selected_ = -1;
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            edit({EditKind::Undo});
            selected_ = -1;
            poseDirty_ = false;
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
            edit({EditKind::Redo});
            selected_ = -1;
            poseDirty_ = false;
        }
    }
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(top, {top.x + available.x, top.y + available.y}, true);
    draw->AddRectFilled(top, {top.x + available.x, top.y + available.y},
                        IM_COL32(239, 242, 245, 255));
    auto screen = [&](Vec2 p) {
        auto s = camera_.toScreen(p);
        return ImVec2{s.x, s.y};
    };
    auto low = camera_.toWorld({top.x, top.y + available.y}),
         high = camera_.toWorld({top.x + available.x, top.y});
    float grid = zoom_ < .5f ? .5f : .1f;
    for (float x = std::floor(low.x / grid) * grid; x <= high.x; x += grid)
        draw->AddLine(screen({x, low.y}), screen({x, high.y}), IM_COL32(215, 222, 229, 255));
    for (float y = std::floor(low.y / grid) * grid; y <= high.y; y += grid)
        draw->AddLine(screen({low.x, y}), screen({high.x, y}), IM_COL32(215, 222, 229, 255));
    auto strokeDraw = [&](const Stroke& stroke, Vec2 offset, ImU32 color, float extra) {
        for (size_t i = 0; i < stroke.points.size(); ++i) {
            auto p = stroke.points[i];
            float width = p.width * camera_.pixelsPerMeter + extra;
            draw->AddCircleFilled(screen(p.position + offset), width * .5f, color);
            if (i)
                draw->AddLine(screen(stroke.points[i - 1].position + offset),
                              screen(p.position + offset), color, width);
        }
    };
    for (size_t i = 0; i < model.track.strokes.size(); ++i) {
        Vec2 offset = (moving_ && static_cast<int>(i) == selected_) ? moveDelta_ : Vec2{};
        if (static_cast<int>(i) == selected_)
            strokeDraw(model.track.strokes[i], offset, IM_COL32(255, 165, 40, 255), 5);
        strokeDraw(model.track.strokes[i], offset, IM_COL32(25, 28, 32, 255), 0);
    }
    if (drawing_)
        strokeDraw(draft_, {}, IM_COL32(70, 110, 180, 230), 0);
    auto start = placing_ ? pendingStart_ : model.start.position;
    float heading = placing_ ? std::atan2(world.y - start.y, world.x - start.x) : model.start.angle;
    draw->AddCircle(screen(start), 8, IM_COL32(175, 65, 185, 255), 0, 2);
    draw->AddLine(screen(start), screen(start + rotate({.12f, 0}, heading)),
                  IM_COL32(175, 65, 185, 255), 2);
    auto& trail = sim.trajectory();
    for (size_t i = 1; i < trail.size(); ++i)
        draw->AddLine(screen(trail[i - 1]), screen(trail[i]), IM_COL32(44, 125, 210, 180), 2);
    auto rectangle = [&](Vec2 center, float length, float width, ImU32 color) {
        ImVec2 points[4];
        Vec2 corners[] = {{-length / 2, -width / 2},
                          {length / 2, -width / 2},
                          {length / 2, width / 2},
                          {-length / 2, width / 2}};
        for (int i = 0; i < 4; ++i)
            points[i] = screen(worldPoint(body.pose, center + corners[i]));
        draw->AddConvexPolyFilled(points, 4, color);
    };
    auto& c = sim.config();
    rectangle({}, c.length, c.width, IM_COL32(49, 114, 191, 240));
    rectangle({0, c.wheelTrack / 2}, c.wheelLength, c.wheelWidth, IM_COL32(51, 58, 67, 255));
    rectangle({0, -c.wheelTrack / 2}, c.wheelLength, c.wheelWidth, IM_COL32(51, 58, 67, 255));
    draw->AddLine(screen(body.pose.position), screen(worldPoint(body.pose, {c.length * .5f, 0})),
                  IM_COL32(255, 255, 255, 255), 3);
    draw->AddTriangleFilled(screen(worldPoint(body.pose, {c.length * .5f, 0})),
                            screen(worldPoint(body.pose, {c.length * .25f, .015f})),
                            screen(worldPoint(body.pose, {c.length * .25f, -.015f})),
                            IM_COL32(255, 255, 255, 255));
    for (auto s : sim.sensors())
        draw->AddCircleFilled(screen(s.position), 5,
                              s.black ? IM_COL32(0, 184, 110, 255) : IM_COL32(232, 98, 58, 255));

    draw->PopClipRect();
    ImGui::EndChild();
    ImGui::End();
    rlImGuiEnd();
    return actions;
}
}
