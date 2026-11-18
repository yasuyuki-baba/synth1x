#include "PatchMapper.h"
#include "Synth1PatchMapper.h"

namespace synth1x
{
    std::unique_ptr<PatchMapper> PatchMapper::create (int version)
    {
        return std::make_unique<Synth1PatchMapper> (version >= 1 && version <= 113 ? version : 105);
    }
}
