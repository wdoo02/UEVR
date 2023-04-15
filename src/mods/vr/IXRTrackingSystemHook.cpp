#include <unordered_map>

#include <spdlog/spdlog.h>
#include <utility/Scan.hpp>
#include <utility/Module.hpp>

#include "utility/Logging.hpp"

#include <sdk/Utility.hpp>

#include "../VR.hpp"

#include "vtables/IXRTrackingSystemVTables.hpp"
#include "IXRTrackingSystemHook.hpp"

detail::IXRTrackingSystemVT& get_tracking_system_vtable() {
    const auto version = sdk::get_file_version_info();

    // >= 5.1
    if (version.dwFileVersionMS >= 0x50001) {
        return ue5_1::IXRTrackingSystemVT::get();
    }

    // >= 5.0
    if (version.dwFileVersionMS == 0x50000) {
        return ue5_00::IXRTrackingSystemVT::get();
    }

    // 4.27
    if (version.dwFileVersionMS == 0x4001B) {
        return ue4_27::IXRTrackingSystemVT::get();
    }

    // 4.26
    if (version.dwFileVersionMS == 0x4001A) {
        return ue4_26::IXRTrackingSystemVT::get();
    }
    
    // 4.25
    if (version.dwFileVersionMS == 0x40019) {
        return ue4_25::IXRTrackingSystemVT::get();
    }

    // 4.24
    if (version.dwFileVersionMS == 0x40018) {
        return ue4_24::IXRTrackingSystemVT::get();
    }

    // 4.23
    if (version.dwFileVersionMS == 0x40017) {
        return ue4_23::IXRTrackingSystemVT::get();
    }

    // 4.22
    if (version.dwFileVersionMS == 0x40016) {
        return ue4_22::IXRTrackingSystemVT::get();
    }

    // 4.21
    if (version.dwFileVersionMS == 0x40015) {
        return ue4_21::IXRTrackingSystemVT::get();
    }

    // 4.20
    if (version.dwFileVersionMS == 0x40014) {
        return ue4_20::IXRTrackingSystemVT::get();
    }

    // 4.19
    if (version.dwFileVersionMS == 0x40013) {
        return ue4_19::IXRTrackingSystemVT::get();
    }

    // <= 4.18
    if (version.dwFileVersionMS <= 0x40012) {
        return ue4_18::IXRTrackingSystemVT::get();
    }

    return detail::IXRTrackingSystemVT::get();
}


detail::IXRCameraVT& get_camera_vtable() {
    const auto version = sdk::get_file_version_info();

    // >= 5.1
    if (version.dwFileVersionMS >= 0x50001) {
        return ue5_1::IXRCameraVT::get();
    }

    // >= 5.0
    if (version.dwFileVersionMS == 0x50000) {
        return ue5_00::IXRCameraVT::get();
    }

    // 4.27
    if (version.dwFileVersionMS == 0x4001B) {
        return ue4_27::IXRCameraVT::get();
    }

    // 4.26
    if (version.dwFileVersionMS == 0x4001A) {
        return ue4_26::IXRCameraVT::get();
    }

    // 4.25
    if (version.dwFileVersionMS == 0x40019) {
        return ue4_25::IXRCameraVT::get();
    }

    // 4.24
    if (version.dwFileVersionMS == 0x40018) {
        return ue4_24::IXRCameraVT::get();
    }

    // 4.23
    if (version.dwFileVersionMS == 0x40017) {
        return ue4_23::IXRCameraVT::get();
    }

    // 4.22
    if (version.dwFileVersionMS == 0x40016) {
        return ue4_22::IXRCameraVT::get();
    }

    // 4.21
    if (version.dwFileVersionMS == 0x40015) {
        return ue4_21::IXRCameraVT::get();
    }

    // 4.20
    if (version.dwFileVersionMS == 0x40014) {
        return ue4_20::IXRCameraVT::get();
    }

    // 4.19
    if (version.dwFileVersionMS == 0x40013) {
        return ue4_19::IXRCameraVT::get();
    }

    // <= 4.18
    if (version.dwFileVersionMS <= 0x40012) {
        return ue4_18::IXRCameraVT::get();
    }

    return detail::IXRCameraVT::get();
}

IXRTrackingSystemHook* g_hook = nullptr;

namespace detail {
struct FunctionInfo {
    size_t functions_within{0};
    bool calls_xr_camera{false};
    bool calls_update_player_camera{false};
    bool calls_apply_hmd_rotation{false};
    bool process_view_rotation_analysis_failed{false};
};

std::mutex return_address_to_functions_mutex{};
std::unordered_map<uintptr_t, uintptr_t> return_address_to_functions{};
std::unordered_map<uintptr_t, FunctionInfo> functions{};
std::atomic<uint32_t> total_times_funcs_called{0};
}

IXRTrackingSystemHook::IXRTrackingSystemHook(FFakeStereoRenderingHook* stereo_hook, size_t offset_in_engine) 
    : m_stereo_hook{stereo_hook},
    m_offset_in_engine{offset_in_engine}
{
    SPDLOG_INFO("IXRTrackingSystemHook::IXRTrackingSystemHook");

    g_hook = this;

    for (auto i = 0; i < m_vtable.size(); ++i) {
        m_vtable[i] = (uintptr_t)+[](void*) {
            return nullptr;
        };
    }

    for (auto i = 0; i < m_camera_vtable.size(); ++i) {
        m_camera_vtable[i] = (uintptr_t)+[](void*) {
            return nullptr;
        };
    }

    struct FName {
        int32_t a1{};
        int32_t a2{0};
    };

    // GetSystemName
    m_vtable[0] = (uintptr_t)+[](void* this_ptr, FName* out) -> FName* {
        static FName fake_name{};
        return &fake_name;
    };

    const auto version = sdk::get_file_version_info();

    if (version.dwFileVersionMS >= 0x40000 && version.dwFileVersionMS <= 0x40019) {
        SPDLOG_INFO("IXRTrackingSystemHook::IXRTrackingSystemHook: version <= 4.25");
        m_is_leq_4_25 = true;
    }
}

void IXRTrackingSystemHook::on_pre_engine_tick(sdk::UGameEngine* engine, float delta) {
    if (m_initialized) {
        return;
    }

    if (VR::get()->is_headlocked_aim_enabled()) {
        initialize();
    }
}

void IXRTrackingSystemHook::initialize() {
    m_initialized = true;

    SPDLOG_INFO("IXRTrackingSystemHook::initialize");

    if (sdk::UGameEngine::get() == nullptr) {
        SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: UGameEngine not found");
        return;
    }

    if (m_offset_in_engine == 0) {
        SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: m_offset_in_engine not set");
        return;
    }

    const auto& camera_vt = get_camera_vtable();
    const auto& vt = get_tracking_system_vtable();

    if (vt.implemented()) {
        if (vt.GetXRCamera_index().has_value()) {
            m_vtable[vt.GetXRCamera_index().value()] = (uintptr_t)&get_xr_camera;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_xr_camera_index not implemented");
        }

        if (vt.IsHeadTrackingAllowed_index().has_value()) {
            m_vtable[vt.IsHeadTrackingAllowed_index().value()] = (uintptr_t)&is_head_tracking_allowed;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: is_head_tracking_allowed_index not implemented");
        }

        if (vt.IsHeadTrackingAllowedForWorld_index().has_value()) {
            m_vtable[vt.IsHeadTrackingAllowedForWorld_index().value()] = (uintptr_t)&is_head_tracking_allowed_for_world;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: is_head_tracking_allowed_for_world_index not implemented");
        }
    } else {
        SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: IXRTrackingSystemVT not implemented");
    }

    if (camera_vt.implemented()) {
        if (camera_vt.ApplyHMDRotation_index().has_value()) {
            m_camera_vtable[camera_vt.ApplyHMDRotation_index().value()] = (uintptr_t)&apply_hmd_rotation;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: apply_hmd_rotation_index not implemented");
        }

        if (camera_vt.UpdatePlayerCamera_index().has_value()) {
            m_camera_vtable[camera_vt.UpdatePlayerCamera_index().value()] = (uintptr_t)&update_player_camera;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: update_player_camera_index not implemented");
        }
    } else {
        SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: IXRCameraVT not implemented");
    }

    if (camera_vt.implemented()) {
        m_xr_camera.vtable = m_camera_vtable.data();
        m_xr_camera_shared.obj = &m_xr_camera;
    }

    if (vt.implemented()) {
        auto tracking_system = (SharedPtr*)((uintptr_t)sdk::UGameEngine::get() + m_offset_in_engine);

        m_xr_tracking_system.vtable = m_vtable.data();
        tracking_system->obj = &m_xr_tracking_system;
        tracking_system->ref_controller = nullptr;
        //tracking_system->ref_controller = m_ref_controller.get();
    }

    SPDLOG_INFO("IXRTrackingSystemHook::IXRTrackingSystemHook done");
}

bool IXRTrackingSystemHook::analyze_head_tracking_allowed(uintptr_t return_address) {
    ++detail::total_times_funcs_called;

    std::scoped_lock _{detail::return_address_to_functions_mutex};

    auto it = detail::return_address_to_functions.find(return_address);

    if (it == detail::return_address_to_functions.end()) {
        const auto vfunc = utility::find_virtual_function_start(return_address);

        if (vfunc) {
            detail::return_address_to_functions[return_address] = *vfunc;
        } else {
            detail::return_address_to_functions[return_address] = 0;
        }

        it = detail::return_address_to_functions.find(return_address);
    }

    if (it->second == 0) {
        return false;
    }

    auto& func = detail::functions[it->second];

    if (!g_hook->m_process_view_rotation_hook && detail::total_times_funcs_called >= 100 && 
        !func.calls_xr_camera && !func.calls_update_player_camera && !func.calls_apply_hmd_rotation && !func.process_view_rotation_analysis_failed)
    {
        g_hook->m_attempted_hook_view_rotation = true;

        SPDLOG_INFO("Possibly found ProcessViewRotation: 0x{:x}", it->second);

        const auto module = utility::get_module_within(it->second);

        if (!module) {
            SPDLOG_ERROR("Failed to get module for ProcessViewRotation");
            return false;
        }

        const auto func_ptr = utility::scan_ptr(*module, it->second);

        if (!func_ptr) {
            SPDLOG_ERROR("Failed to find ProcessViewRotation");
            return false;
        }

        // We do not definitively hook ProcessViewRotation here, we point it towards a function analyzer
        // that checks whether the arguments match up with what we expect. If they do, we hook it.
        m_addr_of_process_view_rotation_ptr = *func_ptr;
        //g_hook->m_process_view_rotation_hook = std::make_unique<PointerHook>((void**)*func_ptr, (void*)&process_view_rotation_analyzer);
        g_hook->m_process_view_rotation_hook = safetyhook::create_inline((void*)it->second, (void*)&process_view_rotation_analyzer);

        if (!g_hook->m_process_view_rotation_hook) {
            SPDLOG_ERROR("Failed to hook ProcessViewRotation");
            func.process_view_rotation_analysis_failed = true;
            g_hook->m_attempted_hook_view_rotation = false;
            return true;
        }

        SPDLOG_INFO("Hooked ProcessViewRotation");
    }

    return true;
}

bool IXRTrackingSystemHook::is_head_tracking_allowed(sdk::IXRTrackingSystem*) {
    SPDLOG_INFO_ONCE("is_head_tracking_allowed");

    auto& vr = VR::get();

    if (!vr->is_hmd_active() || !vr->is_headlocked_aim_enabled()) {
        return false;
    }

    if (!g_hook->m_process_view_rotation_hook && !g_hook->m_attempted_hook_view_rotation) {
        const auto return_address = (uintptr_t)_ReturnAddress();

        // <= 4.25 doesn't have IsHeadTrackingAllowedForWorld
        if (g_hook->m_is_leq_4_25) {
            g_hook->analyze_head_tracking_allowed(return_address);
        }
    }

    return !g_hook->m_process_view_rotation_hook;
}

bool IXRTrackingSystemHook::is_head_tracking_allowed_for_world(sdk::IXRTrackingSystem*, void*) {
    SPDLOG_INFO_ONCE("is_head_tracking_allowed_for_world");

    auto& vr = VR::get();

    if (!vr->is_hmd_active() || !vr->is_headlocked_aim_enabled()) {
        return false;
    }

    if (!g_hook->m_process_view_rotation_hook && !g_hook->m_attempted_hook_view_rotation) {
        const auto return_address = (uintptr_t)_ReturnAddress();
        g_hook->analyze_head_tracking_allowed(return_address);

        return true;
    }

    /*(if (!VR::get()->is_hmd_active()) {
        return false;
    }

    return true;*/

    return false;
}

IXRTrackingSystemHook::SharedPtr* IXRTrackingSystemHook::get_xr_camera(sdk::IXRTrackingSystem*, IXRTrackingSystemHook::SharedPtr* out, size_t device_id) {
    SPDLOG_INFO_ONCE("get_xr_camera");

    if (out != nullptr) {
        *out = g_hook->m_xr_camera_shared;
    }

    if (VR::get()->is_hmd_active() && !g_hook->m_process_view_rotation_hook && !g_hook->m_attempted_hook_view_rotation) {
        const auto return_address = (uintptr_t)_ReturnAddress();
        ++detail::total_times_funcs_called;

        std::scoped_lock _{detail::return_address_to_functions_mutex};

        auto it = detail::return_address_to_functions.find(return_address);

        if (it == detail::return_address_to_functions.end()) {
            const auto vfunc = utility::find_virtual_function_start(return_address);

            if (vfunc) {
                detail::return_address_to_functions[return_address] = *vfunc;
            } else {
                detail::return_address_to_functions[return_address] = 0;
            }

            it = detail::return_address_to_functions.find(return_address);
        }

        if (it->second != 0) {
            auto& func = detail::functions[it->second];
            func.calls_xr_camera = true;
        }
    }

    return out;
}

void IXRTrackingSystemHook::apply_hmd_rotation(sdk::IXRCamera*, void* player_controller, Rotator<float>* rot) {
    SPDLOG_INFO_ONCE("apply_hmd_rotation");

    if (VR::get()->is_hmd_active() && !g_hook->m_process_view_rotation_hook && !g_hook->m_attempted_hook_view_rotation) {
        const auto return_address = (uintptr_t)_ReturnAddress();
        ++detail::total_times_funcs_called;

        std::scoped_lock _{detail::return_address_to_functions_mutex};

        auto it = detail::return_address_to_functions.find(return_address);

        if (it == detail::return_address_to_functions.end()) {
            const auto vfunc = utility::find_virtual_function_start(return_address);

            if (vfunc) {
                detail::return_address_to_functions[return_address] = *vfunc;
            } else {
                detail::return_address_to_functions[return_address] = 0;
            }

            it = detail::return_address_to_functions.find(return_address);
        }

        if (it->second != 0) {
            auto& func = detail::functions[it->second];
            func.calls_apply_hmd_rotation = true;
        }
    }

    /*if (g_hook->m_stereo_hook == nullptr) {
        return;
    }

    if (!VR::get()->is_hmd_active()) {
        return;
    }

    if (g_hook->m_stereo_hook->has_double_precision()) {
        *(Rotator<double>*)rot = g_hook->m_stereo_hook->m_last_rotation_double;
    } else {
        *rot = g_hook->m_stereo_hook->m_last_rotation;
    }

    VR::get()->recenter_view();*/
}

bool IXRTrackingSystemHook::update_player_camera(sdk::IXRCamera*, glm::quat* rel_rot, glm::vec3* rel_pos) {
    SPDLOG_INFO_ONCE("update_player_camera");

    if (VR::get()->is_hmd_active() && !g_hook->m_process_view_rotation_hook && !g_hook->m_attempted_hook_view_rotation) {
        ++detail::total_times_funcs_called;

        std::scoped_lock _{detail::return_address_to_functions_mutex};

        const auto return_address = (uintptr_t)_ReturnAddress();
        auto it = detail::return_address_to_functions.find(return_address);

        if (it == detail::return_address_to_functions.end()) {
            const auto vfunc = utility::find_virtual_function_start(return_address);

            if (vfunc) {
                detail::return_address_to_functions[return_address] = *vfunc;
            } else {
                detail::return_address_to_functions[return_address] = 0;
            }

            it = detail::return_address_to_functions.find(return_address);
        }

        if (it->second != 0) {
            auto& func = detail::functions[it->second];
            func.calls_update_player_camera = true;
        }
    }

    //*rel_pos = glm::vec3{0.0f, 0.0f, 0.0f};
    //*rel_rot = glm::identity<glm::quat>();

    return false;
}

void* IXRTrackingSystemHook::process_view_rotation_analyzer(void* a1, size_t a2, size_t a3, size_t a4, size_t a5, size_t a6) {
    SPDLOG_INFO_ONCE("process_view_rotation_analyzer");

    std::scoped_lock _{detail::return_address_to_functions_mutex};

    auto call_orig = [&]() {
        return g_hook->m_process_view_rotation_hook.call<void*>(a1, a2, a3, a4, a5, a6);
    };

    const auto result = call_orig();

    // Not the exact stack pointer but at least some memory pointing to the stack
    // We need this to check whether the arguments reside on the stack.
    // If they don't, then this is not the correct function and we need to analyze the next one.
    const auto stack_pointer = (intptr_t)_AddressOfReturnAddress();

    // Check if a3 and a4 are on the stack, and are not equal to eachother
    if (std::abs((intptr_t)a3 - stack_pointer) > 0x1000 || std::abs((intptr_t)a4 - stack_pointer) > 0x1000 || a3 == a4) {
        SPDLOG_ERROR("Function we hooked for ProcessViewRotation is not the correct one. Analyzing next function.");

        const auto function_addr = g_hook->m_process_view_rotation_hook.target();
        detail::functions[function_addr].process_view_rotation_analysis_failed = true;

        g_hook->m_process_view_rotation_hook.reset();
        g_hook->m_attempted_hook_view_rotation = false;
    } else {
        SPDLOG_INFO("Found correct function for ProcessViewRotation. Hooking it.");
        const auto target = g_hook->m_process_view_rotation_hook.target();
        g_hook->m_process_view_rotation_hook.reset();
        //g_hook->m_process_view_rotation_hook = std::make_unique<PointerHook>((void**)g_hook->m_addr_of_process_view_rotation_ptr, &IXRTrackingSystemHook::process_view_rotation);
        g_hook->m_process_view_rotation_hook = safetyhook::create_inline((void*)target, &IXRTrackingSystemHook::process_view_rotation);
    }

    return result;
}

void IXRTrackingSystemHook::process_view_rotation(
    void* player_controller, float delta_time, Rotator<float>* rot, Rotator<float>* delta_rot) {
    SPDLOG_INFO_ONCE("process_view_rotation");

    auto call_orig = [&]() {
        g_hook->m_process_view_rotation_hook.call<void>(player_controller, delta_time, rot, delta_rot);
    };

    auto& vr = VR::get();

    if (!vr->is_hmd_active() || !vr->is_headlocked_aim_enabled()) {
        call_orig();
        return;
    }

    if (g_hook->m_stereo_hook->has_double_precision()) {
        //*(Rotator<double>*)rot = g_hook->m_stereo_hook->m_last_rotation_double;
        *(Rotator<double>*)rot = g_hook->m_last_view_rotation_double;
    } else {
        //*rot = g_hook->m_stereo_hook->m_last_rotation;
        *rot = g_hook->m_last_view_rotation;
    }

    vr->set_decoupled_pitch(true);
    vr->recenter_view();

    call_orig();

    if (g_hook->m_stereo_hook->has_double_precision()) {
        auto rot_d = (Rotator<double>*)rot;

        g_hook->m_last_view_rotation_double = *rot_d;

        const auto view_mat_inverse =
        glm::yawPitchRoll(
            glm::radians(-rot_d->yaw),
            glm::radians(rot_d->pitch),
            glm::radians(-rot_d->roll));

        const auto view_quat_inverse = glm::quat {
            view_mat_inverse
        };

        auto vqi_norm = glm::normalize(view_quat_inverse);

        // Decoupled Pitch
        if (vr->is_decoupled_pitch_enabled()) {
            //vr->set_pre_flattened_rotation(vqi_norm);
            vqi_norm = utility::math::flatten(vqi_norm);
        }

        const auto current_hmd_rotation = glm::normalize(glm::quat{vr->get_rotation(0)});
        const auto new_rotation = glm::normalize(vqi_norm * current_hmd_rotation);
        const auto euler = glm::degrees(utility::math::euler_angles_from_steamvr(new_rotation));

        rot_d->pitch = euler.x;
        rot_d->yaw = euler.y;
        rot_d->roll = euler.z;
    } else {
        g_hook->m_last_view_rotation = *rot;

        const auto view_mat_inverse =
        glm::yawPitchRoll(
            glm::radians(-rot->yaw),
            glm::radians(rot->pitch),
            glm::radians(-rot->roll));

        const auto view_quat_inverse = glm::quat {
            view_mat_inverse
        };

        auto vqi_norm = glm::normalize(view_quat_inverse);

        // Decoupled Pitch
        if (vr->is_decoupled_pitch_enabled()) {
            //vr->set_pre_flattened_rotation(vqi_norm);
            vqi_norm = utility::math::flatten(vqi_norm);
        }

        const auto current_hmd_rotation = glm::normalize(glm::quat{vr->get_rotation(0)});
        const auto new_rotation = glm::normalize(vqi_norm * current_hmd_rotation);
        const auto euler = glm::degrees(utility::math::euler_angles_from_steamvr(new_rotation));

        rot->pitch = euler.x;
        rot->yaw = euler.y;
        rot->roll = euler.z;
    }
}