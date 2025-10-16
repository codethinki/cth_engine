#pragma once
#include "jolly/res/model/primitive/pipeline_flags.hpp"

#include <jvk/utility/vk_exceptions.hpp>

#include <cth/macro.hpp>

#include <array>
#include <string_view>



namespace jly {

namespace dev {
    cxpr std::array<std::pair<std::string_view, PipelineFlags>, 7> ATTRIBUTE_MAPPINGS = {
        {
            {"NORMAL", PipelineFlags::NORMALS},
            {"TANGENT", PipelineFlags::TANGENTS},
            {"TEXCOORD_0", PipelineFlags::TEXCOORD_0},
            {"TEXCOORD_1", PipelineFlags::TEXCOORD_1},
            {"COLOR_0", PipelineFlags::COLOR_0},
            {"JOINTS_0", PipelineFlags::JOINTS_0},
            {"WEIGHTS_0", PipelineFlags::WEIGHTS_0},
        }
    };
}

cxpr PipelineFlags to_pipeline_flag(std::string_view const input) {
    for(auto const& [sv, flag] : dev::ATTRIBUTE_MAPPINGS)
        if(sv == input) return flag;

    JVK_STABLE_THROW(true, "input does not match any pipeline flag")
        details->add("input: {}", input);

    return PipelineFlags::NONE;
}
}

