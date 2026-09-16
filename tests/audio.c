#define main visualizer_main
#include "../main.c"
#undef main
#include <assert.h>
int main(void) {
    default_settings(&settings);
    struct audio_state a={0};
    for(int j=0;j<1024;j++)a.window[j]=0.5f-0.5f*cosf(2*M_PI*j/1023);
    for(int i=0;i<BAR_COUNT;i++)a.coefficients[i]=2*cosf(2*M_PI*(55*powf(14000.0f/55,i/(float)(BAR_COUNT-1)))/32000);
    analyze(&a);
    for(int i=0;i<BAR_COUNT;i++)assert(levels[i]==0);
    int band=24;
    float frequency=55*powf(14000.0f/55,band/(float)(BAR_COUNT-1));
    for(int j=0;j<1024;j++)a.samples[j]=0.05f*sinf(2*M_PI*frequency*j/32000);
    analyze(&a);
    int peak=0;
    for(int i=0;i<BAR_COUNT;i++){assert(isfinite(levels[i]) && levels[i]>=0 && levels[i]<=1);if(levels[i]>levels[peak])peak=i;}
    assert(peak==band);assert(levels[band]>0.4f && levels[band]<0.7f);
    assert(levels[band-1]>0 && levels[band+1]>0);
    memset(a.samples,0,sizeof a.samples);analyze(&a);
    for(int i=0;i<BAR_COUNT;i++)assert(levels[i]==0);
    float rising=0,falling=1;
    for(int i=0;i<6;i++) {
        float nextRise=animate_level(rising,1),nextFall=animate_level(falling,0);
        assert(nextRise>rising && nextRise<=1);
        assert(nextFall<falling && nextFall>=0);
        rising=nextRise;falling=nextFall;
    }
    assert(rising>0.95f && falling>0.3f);
    for(int i=0;i<FPS*3;i++)falling=animate_level(falling,0);
    assert(falling==0);
    float spectrum[BAR_COUNT]={0},mirrored[BAR_COUNT];
    spectrum[0]=1;
    mirror_spectrum(spectrum,mirrored);
    assert(mirrored[BAR_COUNT/2]>0 && mirrored[0]==0);
    for(int i=0;i<BAR_COUNT;i++)assert(mirrored[i]==mirrored[BAR_COUNT-1-i]);
    memset(spectrum,0,sizeof spectrum);spectrum[BAR_COUNT-1]=1;
    mirror_spectrum(spectrum,mirrored);
    assert(mirrored[0]>0 && mirrored[BAR_COUNT/2]==0);
    for(int i=0;i<BAR_COUNT;i++)assert(mirrored[i]==mirrored[BAR_COUNT-1-i]);
    assert(visible_bar_count(1280)==42);
    assert(visible_bar_count(1920)==64);
    assert(visible_bar_count(2560)==84);
    assert(visible_bar_count(1)==2);
    assert(visible_bar_count(100000)==MAX_VISIBLE_BARS);
    for(int width=1;width<=16000;width+=37) {
        int count=visible_bar_count(width);
        assert(count>=2 && count<=MAX_VISIBLE_BARS && count%2==0);
        for(int i=0;i<count;i++) {
            float v=resample_level(mirrored,count,i);
            assert(isfinite(v) && v>=0 && v<=1);
            assert(fabsf(v-resample_level(mirrored,count,count-1-i))<0.0001f);
        }
    }
    puts("PASS: adaptive density and mirrored resampling");
    puts("PASS: bass centered, treble at edges, mirror symmetry; smooth attack, slow release, settles to silence; silence, tone frequency, bounded levels, return to silence");
}
