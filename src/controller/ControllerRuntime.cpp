#include "controller/ControllerRuntime.h"
#include "controller_api.h"
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <type_traits>

namespace lt {
static_assert(sizeof(void*) == 8, "Controller ABI v1 requires Windows x64");
static_assert(std::is_standard_layout_v<ControllerInput> &&
              std::is_trivially_copyable_v<ControllerInput>);
static_assert(sizeof(ControllerInput) == 32 && sizeof(ControllerOutput) == 16);

struct ControllerRuntime::Impl {
    HMODULE module = nullptr;
    using Value = uint32_t (*)() noexcept;
    using Reset = void (*)() noexcept;
    using Step = uint32_t (*)(const ControllerInput*, ControllerOutput*) noexcept;
    Reset reset = nullptr;
    Step step = nullptr;

    ~Impl() {
        if (module)
            FreeLibrary(module);
    }
};

ControllerRuntime::ControllerRuntime() : impl_(std::make_unique<Impl>()) {}

ControllerRuntime::~ControllerRuntime() = default;

bool ControllerRuntime::loaded() const {
    return impl_->module != nullptr;
}

bool ControllerRuntime::load(const std::filesystem::path& path, std::string& error) {
    auto candidate = std::make_unique<Impl>();
    candidate->module =
        LoadLibraryExW(std::filesystem::absolute(path).c_str(), nullptr,
                       LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!candidate->module) {
        error = "DLLを読み込めませんでした（Windowsエラー " + std::to_string(GetLastError()) + "）";
        return false;
    }

    auto symbol = [&](const char* name) { return GetProcAddress(candidate->module, name); };
    auto version = reinterpret_cast<Impl::Value>(symbol("controller_api_version"));
    auto inputSize = reinterpret_cast<Impl::Value>(symbol("controller_input_size"));
    auto outputSize = reinterpret_cast<Impl::Value>(symbol("controller_output_size"));
    candidate->reset = reinterpret_cast<Impl::Reset>(symbol("controller_reset"));
    candidate->step = reinterpret_cast<Impl::Step>(symbol("controller_step"));
    if (!version || !inputSize || !outputSize || !candidate->reset || !candidate->step) {
        error = "DLLに必要な関数がありません。以前のDLLを維持します。";
        return false;
    }
    if (version() != CONTROLLER_API_VERSION || inputSize() != sizeof(ControllerInput) ||
        outputSize() != sizeof(ControllerOutput)) {
        error = "DLLのAPIバージョンまたは構造体サイズが一致しません。以前のDLLを維持します。";
        return false;
    }

    // 新しいDLLの検査が済むまでは、動作中のDLLを残しておく。
    candidate->reset();
    impl_.swap(candidate);
    error.clear();
    return true;
}

void ControllerRuntime::reset() {
    if (loaded())
        impl_->reset();
}

bool ControllerRuntime::step(double time, double dt, const std::vector<float>& sensors,
                             MotorCommand& result, std::string& error) {
    if (!loaded()) {
        error = "制御DLLが読み込まれていません。";
        return false;
    }

    ControllerInput input{sizeof(ControllerInput), static_cast<uint32_t>(sensors.size()), time, dt,
                          sensors.data()};
    ControllerOutput output{sizeof(ControllerOutput), 0, 0, 0};
    if (impl_->step(&input, &output) != 1 || output.struct_size != sizeof(ControllerOutput) ||
        output.reserved != 0 || !std::isfinite(output.left_motor) ||
        !std::isfinite(output.right_motor)) {
        error = "制御DLLの処理に失敗したか、不正な指令が返されたため、走行を停止しました。";
        return false;
    }

    result = {std::clamp(output.left_motor, -1.f, 1.f), std::clamp(output.right_motor, -1.f, 1.f)};
    return true;
}
}
