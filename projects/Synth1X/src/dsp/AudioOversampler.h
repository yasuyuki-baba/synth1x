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

using StereoFrame=std::array<float,2>;

class StereoHalfBandStage
{
public:
    StereoHalfBandStage() noexcept { (void)OversamplingFilter::coefficients(); }

    void interpolate(StereoFrame input,StereoFrame& even,StereoFrame& odd) noexcept
    {
        push({input[0]*2,input[1]*2});
        even=centerTap(16);
        for(float& sample:even)sample+=0.0f;
        odd=oddTaps({},0,1);
    }

    StereoFrame decimate(StereoFrame even,StereoFrame odd) noexcept
    {
        push(even);
        const auto output=oddTaps(centerTap(32),1,2);
        push(odd);
        return output;
    }

private:
    void push(StereoFrame frame) noexcept
    {
        head=(head+63)&63;
        history[head]=history[head+64]=frame;
    }

    StereoFrame centerTap(int offset) const noexcept
    {
        const float center=OversamplingFilter::coefficients()[32];
        const auto& frame=history[head+offset];
        return {center*frame[0],center*frame[1]};
    }

    StereoFrame oddTaps(StereoFrame sum,int offset,int stride) const noexcept
    {
        const auto& weights=OversamplingFilter::coefficients();
        for(int tap=0;tap<32;++tap)
        {
            const auto& frame=history[head+offset+tap*stride];
            const float weight=weights[2*tap+1];
            sum[0]+=weight*frame[0];
            sum[1]+=weight*frame[1];
        }
        return sum;
    }

    // A mirrored ring keeps both channels contiguous across the wrap.
    std::array<StereoFrame,128> history{};
    int head=0;
};

class StereoOversampler
{
public:
    void setFactor(int next) noexcept
    {
        if(!OversamplingSettings::valid(next) || next==factor)return;
        factor=next;
        reset();
    }
    int getFactor() const noexcept { return factor; }
    void reset() noexcept
    {
        silentLinearFrames=0;
        up={};
        down={};
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
        std::array<StereoFrame,16> frames{};
        frames[0]={left,right};
        for(int count=1,stage=0;count<factor;count*=2,++stage)
        {
            std::array<StereoFrame,16> next{};
            for(int i=0;i<count;++i)
                up[stage].interpolate(frames[i],next[2*i],next[2*i+1]);
            frames=next;
        }

        std::array<float,16> leftSamples{},rightSamples{};
        for(int i=0;i<factor;++i)
        {
            leftSamples[i]=frames[i][0];
            rightSamples[i]=frames[i][1];
        }
        kernel(leftSamples.data(),rightSamples.data(),factor);
        for(int i=0;i<factor;++i)frames[i]={leftSamples[i],rightSamples[i]};

        for(int count=factor,stage=0;count>1;count/=2,++stage)
            for(int i=0;i<count/2;++i)
                frames[i]=down[stage].decimate(frames[2*i],frames[2*i+1]);
        return frames[0];
    }
    int factor=1;
    int silentLinearFrames=0;
    std::array<StereoHalfBandStage,4> up;
    std::array<StereoHalfBandStage,4> down;
};
}
