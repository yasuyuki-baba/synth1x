#pragma once
#include "OversamplingSettings.h"
#include <array>
#include <cmath>

namespace synth1x::dsp
{
class OversamplingFilter
{
public:
    OversamplingFilter() noexcept { (void)coefficients(); }
    void reset() noexcept { history={};head=64; }
    void push(float value) noexcept { if(++head==65)head=0;history[head]=value; }
    float value() const noexcept
    {
        const auto& h=coefficients();
        int center=head+33;
        if(center>=65)center-=65;
        float result=h[32]*history[center];
        int index=head-1;
        if(index<0)index+=65;
        // Visit odd taps in coefficient order, wrapping within one ring.
        for(int i=1;i<65;i+=2)
        {
            result+=h[i]*history[index];
            index-=2;
            if(index<0)index+=65;
        }
        return result;
    }
    static const std::array<float,65>& coefficients() noexcept
    {
        static const auto values=[] {
            std::array<float,65> h{};float sum=0;
            constexpr float pi=3.14159265358979323846f;
            for(int i=0;i<65;++i)
            {
                const int n=i-32;
                const float window=.42f-.5f*std::cos(2*pi*i/64)+.08f*std::cos(4*pi*i/64);
                const float tap=n==0 ? .5f : n%2==0 ? 0 : std::sin(.5f*pi*n)/(pi*n);
                h[i]=static_cast<float>(tap*window);sum+=h[i];
            }
            for(auto& tap:h)tap=static_cast<float>(tap/sum);
            return h;
        }();
        return values;
    }
private:
    std::array<float,65> history{};
    int head=64;
};

class AudioDownsampler
{
public:
    void setFactor(int next) noexcept
    { if(OversamplingSettings::valid(next) && next!=factor){factor=next;reset();} }
    int getFactor() const noexcept { return factor; }
    void reset() noexcept { for(auto& stage:stages)stage.reset(); }
    float process(const std::array<float,16>& input) noexcept
    {
        if(factor==1)return input[0];
        auto values=input;
        int count=factor;
        for(int stage=0;count>1;++stage,count/=2)
            for(int i=0;i<count/2;++i)
            {
                // Phase zero keeps the total delay an integral host-frame count.
                stages[stage].push(values[i*2]);
                const float output=stages[stage].value();
                stages[stage].push(values[i*2+1]);values[i]=output;
            }
        return values[0];
    }
private:
    int factor=1;
    std::array<OversamplingFilter,4> stages;
};

class AudioUpsamplingStage
{
public:
    AudioUpsamplingStage() noexcept { (void)OversamplingFilter::coefficients(); }
    void reset() noexcept { history={};head=0; }

    std::array<float,2> process(float input) noexcept
    {
        head=(head+31)&31;
        history[head]=history[head+32]=input*2;
        const auto& weights=OversamplingFilter::coefficients();

        // In the zero-stuffed FIR, even output positions use only the center
        // tap. The other phase uses the odd taps in the original summation order.
        const float even=weights[32]*history[head+16]+0.0f;
        float odd=0.0f;
        for(int i=0;i<32;++i)
            odd+=weights[2*i+1]*history[head+i];
        return {even,odd};
    }
private:
    // Mirror the ring so the 32-tap phase reads one contiguous span.
    std::array<float,64> history{};
    int head=0;
};

class StereoOversampler
{
public:
    void setFactor(int next) noexcept
    {
        if(!OversamplingSettings::valid(next) || next==factor)return;
        factor=next;
        silentLinearFrames=0;
        for(auto& channel:up)for(auto& stage:channel)stage.reset();
        for(auto& channel:down)channel.setFactor(next);
    }
    int getFactor() const noexcept { return factor; }
    void reset() noexcept
    {
        silentLinearFrames=0;
        for(auto& channel:up)for(auto& stage:channel)stage.reset();
        for(auto& channel:down)channel.reset();
    }

    std::array<float,2> processLinear(float left,float right) noexcept
    {
        // Each linear-phase FIR has a finite span of twice its group delay.
        // Drain both FIR chains before skipping all-zero input frames.
        const int drainFrames=4*OversamplingSettings::decimationLatency(factor)+1;
        if(left==0.0f && right==0.0f)
        {
            if(silentLinearFrames==drainFrames)return {};
            ++silentLinearFrames;
        }
        else silentLinearFrames=0;
        return processFrame(left,right,[](float*,float*,int){});
    }

    template<class Kernel> std::array<float,2> process(float left,float right,Kernel&& kernel) noexcept
    {
        silentLinearFrames=0;
        return processFrame(left,right,kernel);
    }

private:
    template<class Kernel> std::array<float,2> processFrame(float left,float right,Kernel&& kernel) noexcept
    {
        std::array<std::array<float,16>,2> values{};
        values[0][0]=left;values[1][0]=right;
        for(int channel=0;channel<2;++channel)
            for(int count=1,stage=0;count<factor;count*=2,++stage)
            {
                std::array<float,16> next{};
                for(int i=0;i<count;++i)
                {
                    const auto pair=up[channel][stage].process(values[channel][i]);
                    next[i*2]=pair[0];
                    next[i*2+1]=pair[1];
                }
                values[channel]=next;
            }
        kernel(values[0].data(),values[1].data(),factor);
        return {down[0].process(values[0]),down[1].process(values[1])};
    }
    int factor=1;
    int silentLinearFrames=0;
    std::array<std::array<AudioUpsamplingStage,4>,2> up;
    std::array<AudioDownsampler,2> down;
};
}
