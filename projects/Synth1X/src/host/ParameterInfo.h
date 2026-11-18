#pragma once

#include <string>

namespace synth1x::host
{
inline constexpr int processorParameterCount=129;

struct ParameterInfo
{
    std::string id,name;
    double minimum,maximum,defaultNormalized;
};
}
