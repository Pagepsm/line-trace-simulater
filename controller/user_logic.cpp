#include "controller_api.h"

// This source runs inside the simulator. Use only trusted code.
static float last_error = 0.0f;

uint32_t controller_api_version() noexcept {
    return CONTROLLER_API_VERSION;
}

uint32_t controller_input_size() noexcept {
    return sizeof(ControllerInput);
}

uint32_t controller_output_size() noexcept {
    return sizeof(ControllerOutput);
}

void controller_reset() noexcept {
    last_error = 0.0f;
}

uint32_t controller_step(const ControllerInput* in, ControllerOutput* out) noexcept {
    if (!in || !out || in->struct_size != sizeof(ControllerInput) ||
        out->struct_size != sizeof(ControllerOutput) || !in->sensor_count || !in->sensors)
        return 0;

    float error = 0.0f, weight = 0.0f;
    for (uint32_t i = 0; i < in->sensor_count; ++i) {
        float position = in->sensor_count > 1 ? 1.0f - 2.0f * i / (in->sensor_count - 1) : 0.0f;
        error += position * in->sensors[i];
        weight += in->sensors[i];
    }

    if (weight > 0)
        last_error = error / weight;

    out->left_motor = 0.28f - 0.22f * last_error;
    out->right_motor = 0.28f + 0.22f * last_error;
    return 1;
}
