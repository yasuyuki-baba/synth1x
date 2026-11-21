#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace synth1x::dsp
{
    // CSynth output stage, Win64 RVAs 0x28150 / 0x28480. The recurrence
    // advances once per interleaved sample, so right follows left by one step.
    class OutputGain
    {
    public:
        static constexpr float defaultTarget=0x1.a696eep-1f;
        static float midiVolumeTarget(int dial) noexcept
        {
            // Synth1 v1.13 beta 3 output-stage targets observed after CC7.
            // The unmodified output target is the target for dial 64.
            static constexpr std::array<float,128> targets {
                0x0p+0f,0x1.137a66p-11f,0x1.f6c2ecp-9f,0x1.6cb6cep-7f,
                0x1.639deep-6f,0x1.19d6bep-5f,0x1.8b882ap-5f,0x1.00d3fep-4f,
                0x1.3c7af4p-4f,0x1.77c4b8p-4f,0x1.b22ae2p-4f,0x1.eb6b34p-4f,
                0x1.11b67cp-3f,0x1.2d182ap-3f,0x1.47e1e4p-3f,0x1.621ee6p-3f,
                0x1.7bdc62p-3f,0x1.952852p-3f,0x1.ae10bap-3f,0x1.c6a33ep-3f,
                0x1.deecdcp-3f,0x1.f6f9ep-3f,0x1.076ae8p-2f,0x1.1345b2p-2f,
                0x1.1f1254p-2f,0x1.2ad576p-2f,0x1.36936ep-2f,0x1.425048p-2f,
                0x1.4e0fcep-2f,0x1.59d584p-2f,0x1.65a4cp-2f,0x1.71809cp-2f,
                0x1.7d6c0cp-2f,0x1.8969d8p-2f,0x1.957ca2p-2f,0x1.a1a6fp-2f,
                0x1.adeb26p-2f,0x1.ba4b94p-2f,0x1.c6ca68p-2f,0x1.d369c6p-2f,
                0x1.e02bbcp-2f,0x1.ed124ap-2f,0x1.fa1f5ep-2f,0x1.03aa6cp-1f,
                0x1.0a5a4ep-1f,0x1.112034p-1f,0x1.17fd08p-1f,0x1.1ef1a4p-1f,
                0x1.25feeap-1f,0x1.2d25bp-1f,0x1.3466dp-1f,0x1.3bc32p-1f,
                0x1.433b76p-1f,0x1.4ad0a6p-1f,0x1.528384p-1f,0x1.5a54ep-1f,
                0x1.624592p-1f,0x1.6a566cp-1f,0x1.728842p-1f,0x1.7adbeap-1f,
                0x1.83523cp-1f,0x1.8bec0ap-1f,0x1.94aa3p-1f,0x1.9d8d88p-1f,
                0x1.a696eep-1f,0x1.afc742p-1f,0x1.b91f6p-1f,0x1.c2a02ep-1f,
                0x1.cc4a9p-1f,0x1.d61f6ep-1f,0x1.e01fb2p-1f,0x1.ea4c48p-1f,
                0x1.f4a62p-1f,0x1.ff2e36p-1f,0x1.04f2b8p+0f,0x1.0a666ap+0f,
                0x1.0ff2aep+0f,0x1.159804p+0f,0x1.1b56eep+0f,0x1.212ff2p+0f,
                0x1.272396p+0f,0x1.2d3262p+0f,0x1.335cep+0f,0x1.39a39ep+0f,
                0x1.40072ap+0f,0x1.468814p+0f,0x1.4d26f2p+0f,0x1.53e45ap+0f,
                0x1.5ac0e6p+0f,0x1.61bd2cp+0f,0x1.68d9cap+0f,0x1.701764p+0f,
                0x1.77769cp+0f,0x1.7ef816p+0f,0x1.869c7cp+0f,0x1.8e647ap+0f,
                0x1.9650bep+0f,0x1.9e61f8p+0f,0x1.a698dep+0f,0x1.aef626p+0f,
                0x1.b77a8ep+0f,0x1.c026d2p+0f,0x1.c8fbb4p+0f,0x1.d1f9f8p+0f,
                0x1.db226ap+0f,0x1.e475d6p+0f,0x1.edf504p+0f,0x1.f7a0cep+0f,
                0x1.00bd04p+1f,0x1.05c0c8p+1f,0x1.0adc22p+1f,0x1.100f84p+1f,
                0x1.155b5ep+1f,0x1.1ac02ap+1f,0x1.203e5ep+1f,0x1.25d674p+1f,
                0x1.2b88e6p+1f,0x1.315636p+1f,0x1.373ee2p+1f,0x1.3d436cp+1f,
                0x1.43646p+1f,0x1.49a23cp+1f,0x1.4ffd9p+1f,0x1.5676e6p+1f,
                0x1.5d0edp+1f,0x1.63c5dep+1f,0x1.6a9ca4p+1f,0x1.7193bap+1f
            };
            return targets[static_cast<unsigned>(std::clamp(dial, 0, 127))];
        }
        void prepare (float sampleRate, bool preserveCurrent = false) noexcept
        {
            coefficient = static_cast<float> (1.0 - std::pow (0.5,
                1.0 / static_cast<float> (static_cast<float> (sampleRate) * 0.002f)));
            if (!preserveCurrent) current = 0.0f;
        }
        float next (float target) noexcept
        {
            current += (target - current) * coefficient;
            return current;
        }
        void finish (float target) noexcept
        {
            if (std::abs (target - current) < 1.0f / 65536.0f) current = target;
        }
        void silence (float target) noexcept { current = target; }
        float value() const noexcept { return current; }
    private:
        float current { 0.0f }, coefficient { 0.0f };
    };
}
