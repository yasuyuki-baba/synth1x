#pragma once

#include "../RenderControls.h"
#include "../render/RenderHistory.h"
#include <algorithm>
#include <memory>
#include <atomic>
#include <cmath>

namespace synth1x
{
    /**
        Interface for mapping source parameter values to RenderControls using
        the source patch version and loading context.
    */
    class PatchMapper
    {
    public:
        virtual ~PatchMapper() = default;
        void setRandomHistoryValid(bool valid) noexcept { loadHistory.nativeRandomLfoHistoryValid=valid; }
        const render::RenderHistory& getRenderHistory() const noexcept { return loadHistory; }
        virtual int getSourceVersion() const noexcept = 0;
        virtual const std::array<float,128>& getSourceValues() const noexcept = 0;
        void setNativeSourceContext(bool enabled, std::array<uint64_t,2> presence,
                                    const std::array<int,99>& initial, std::array<uint64_t,2> edits) noexcept
        { nativeSourceLoading=enabled; nativeSourcePresence=presence;nativeInitialControls=initial;nativeRuntimeEdits=edits; }
        virtual bool readNativeControls(std::array<int,99>&) const noexcept { return false; }
        // Original source presence is separate from the host automation values.
        void setMissingEnvelopeFields (unsigned fields) noexcept { missingEnvelopeFields = fields; }


        // The processor supplies pointers to its source parameter atomics.
        // The mapper reads their values without retaining the pointers.
        virtual void mapParameterValues (const std::array<std::atomic<float>*,126>& values,
                                         RenderControls& target)
        {
            std::array<float,128> raw{};
            for(size_t i=0;i<values.size();++i)if(values[i]!=nullptr)raw[i]=values[i]->load()/127.0f;
            mapFromRawBuffer(raw.data(),target);
        }

        /**
            Maps a buffer of source parameter values normalized by 127 to RenderControls.
        */
        virtual void mapFromRawBuffer (const float* p, RenderControls& target) = 0;

        /**
            Creates the shared mapper for the given source patch version.
        */
        static std::unique_ptr<PatchMapper> create (int version);

    protected:
        render::RenderHistory loadHistory;
        unsigned missingEnvelopeFields = 0;
        bool nativeSourceLoading = false;
        std::array<uint64_t,2> nativeSourcePresence {};
        std::array<int,99> nativeInitialControls {};
        std::array<uint64_t,2> nativeRuntimeEdits {};
        // Exponential mapping for time parameters
        float mapTimeSecExp (float norm, float minSec, float maxSec) const
        {
            float n = std::clamp (norm, 0.0f, 1.0f);
            // A zero attack is valid, but cannot be the base of a ratio.
            if (minSec <= 0.0f)
                return n == 0.0f ? 0.0f : 0.001f * std::pow (maxSec / 0.001f, n);
            return minSec * std::pow (maxSec / minSec, n);
        }
    };
}
