#define main visualizer_main
#include "../main.c"
#undef main
#include <assert.h>
static void write_config(const char *path,const char *text) {
    FILE *f=fopen(path,"w");assert(f);fputs(text,f);fclose(f);
}
static unsigned long long pixels(cairo_surface_t *s) {
    cairo_surface_flush(s);unsigned char *p=cairo_image_surface_get_data(s);
    unsigned long long sum=0;
    for(int i=0;i<cairo_image_surface_get_stride(s)*cairo_image_surface_get_height(s);i++)sum+=p[i];
    return sum;
}
int main(void) {
    char path[]="/tmp/hypr-visualizer-config-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
    struct settings next;
    default_settings(&settings);
    write_config(path,"theme=classic\n");assert(read_settings(path,&next)==1);
    assert(!memcmp(&next,&settings,sizeof next));
    write_config(path,"theme=aurora\nstyle=wave\nmotion=natural\nlockscreen=off\nglow=0.7\nbar_width=12\ngap=4\n");
    assert(read_settings(path,&next)==1 && next.style==1 && next.natural && !next.lockscreen && next.glow==0.7);
    assert(next.colors[0]==0x63e6be);
    const char *bad[]={"glow=nan\n","height=inf\n","bar_width=0\n","gap=-1\n","style=bad\n","unknown=1\n","opacity=2\n","colors=#ffffff\n","colors=#zzzzzz,#ffffff\n"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++) {
        struct settings before=next;write_config(path,bad[i]);
        assert(read_settings(path,&next)==-1 && !memcmp(&before,&next,sizeof next));
    }
    unlink(path);
    float classic=animate_level(0,1);settings.natural=1;
    assert(animate_level(0,1)<classic && animate_level(0,0)==0);
    float v=0;
    for(int i=0;i<300;i++)v=animate_level(v,0.01f);
    assert(v>0.01f && v<0.05f);
    for(int i=0;i<300;i++)v=animate_level(v,0);
    assert(v==0);
    default_settings(&settings);
    cairo_surface_t *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,960,540);
    cairo_t *cr=cairo_create(surface);float shown[MAX_VISIBLE_BARS]={0};int count=visible_bar_count(960);
    for(int style=0;style<4;style++) {
        settings.style=style;settings.glow=1;
        render_bars(cr,960,540,shown,count);assert(pixels(surface)==0);
    }
    for(int i=0;i<count;i++)shown[i]=0.2+0.7*(1-fabsf((i-(count-1)/2.0f)/(count/2.0f)));
    for(int style=0;style<4;style++) {
        settings.style=style;settings.glow=0;
        render_bars(cr,960,540,shown,count);unsigned long long plain=pixels(surface);assert(plain>0);
        settings.glow=1;render_bars(cr,960,540,shown,count);assert(pixels(surface)>plain);
        char output[128];snprintf(output,sizeof output,"build/preview-style-%d.png",style);
        assert(cairo_surface_write_to_png(surface,output)==CAIRO_STATUS_SUCCESS);
        settings.opacity=0;render_bars(cr,960,540,shown,count);assert(pixels(surface)==0);settings.opacity=OPACITY;
    }
    cairo_destroy(cr);cairo_surface_destroy(surface);
    puts("PASS: Classic defaults, strict config validation, natural motion, silence, four render styles and bass glow");
}
