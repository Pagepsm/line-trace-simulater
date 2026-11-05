#pragma once
#include <stdint.h>
#ifdef __cplusplus
#define LT_EXTERN extern "C"
#define LT_NOEXCEPT noexcept
#else
#define LT_EXTERN extern
#define LT_NOEXCEPT
#endif
#define LT_EXPORT LT_EXTERN __declspec(dllexport)
#define CONTROLLER_API_VERSION 1u

typedef struct ControllerInput {
    uint32_t struct_size;
    uint32_t sensor_count;
    double time_seconds;
    double dt_seconds;
    const float* sensors; /* borrowed during step only; white=0, black=1 */
} ControllerInput;

typedef struct ControllerOutput {
    uint32_t struct_size;
    uint32_t reserved;
    float left_motor; /* [-1,1], negative=reverse, zero=no drive force */
    float right_motor;
} ControllerOutput;

LT_EXPORT uint32_t controller_api_version(void) LT_NOEXCEPT;
LT_EXPORT uint32_t controller_input_size(void) LT_NOEXCEPT;
LT_EXPORT uint32_t controller_output_size(void) LT_NOEXCEPT;
LT_EXPORT void controller_reset(void) LT_NOEXCEPT;
LT_EXPORT uint32_t controller_step(const ControllerInput*, ControllerOutput*) LT_NOEXCEPT;
/* No exceptions may cross this ABI. Return 1 on success, 0 on failure. */
