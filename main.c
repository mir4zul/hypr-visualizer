#define _GNU_SOURCE
#include <wayland-client.h>
#include <cairo/cairo.h>
#include <pulse/pulseaudio.h>
#include <pulse/error.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "layer-shell.h"
#include "theme.h"

static struct wl_display *display;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct zwlr_layer_shell_v1 *shell;
static volatile sig_atomic_t running = 1;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static float levels[BAR_COUNT];
struct buffer { struct wl_buffer *wl; void *data; size_t size; int busy; };
struct output {
    uint32_t name;
    struct wl_output *wl;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer;
    int width, height, configured, redraw, closed;
    float shown[BAR_COUNT];
    struct buffer buffers[2];
    struct output *next;
};
static struct output *outputs;
static void stop(int sig) { (void)sig; running = 0; }
static void released(void *data, struct wl_buffer *wl) { (void)wl; ((struct buffer *)data)->busy = 0; }
static const struct wl_buffer_listener buffer_listener = { released };
static void discard_buffers(struct output *o) {
    struct buffer *buffers=o->buffers;
    for (int i=0;i<2;i++) if (buffers[i].wl) {
        wl_buffer_destroy(buffers[i].wl); munmap(buffers[i].data,buffers[i].size);
        memset(&buffers[i],0,sizeof buffers[i]);
    }
}
static int make_buffer(struct output *o, struct buffer *b) {
    int width=o->width,height=o->height;
    b->size = (size_t)width*height*4;
    int fd=memfd_create("hypr-visualizer",MFD_CLOEXEC);
    if(fd<0) return 0;
    if(ftruncate(fd,b->size)<0) {close(fd);return 0;}
    b->data=mmap(NULL,b->size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(b->data==MAP_FAILED) {close(fd);return 0;}
    struct wl_shm_pool *pool=wl_shm_create_pool(shm,fd,b->size);
    b->wl=wl_shm_pool_create_buffer(pool,0,width,height,width*4,WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);close(fd);
    wl_buffer_add_listener(b->wl,&buffer_listener,b);return 1;
}
static void configure(void *data,struct zwlr_layer_surface_v1 *layer,uint32_t serial,uint32_t w,uint32_t h) {
    struct output *o=data;
    zwlr_layer_surface_v1_ack_configure(layer,serial);
    if (w && h && (o->width!=(int)w || o->height!=(int)h)) {
        discard_buffers(o);o->width=w;o->height=h;o->redraw=1;
    }
    o->configured=o->width>0 && o->height>0;
}
static void closed(void *data,struct zwlr_layer_surface_v1 *layer) {
    (void)layer;struct output *o=data;o->closed=1;o->configured=0;
}
static const struct zwlr_layer_surface_v1_listener layer_listener={configure,closed};
static void destroy_surface(struct output *o) {
    discard_buffers(o);
    if(o->layer)zwlr_layer_surface_v1_destroy(o->layer);
    if(o->surface)wl_surface_destroy(o->surface);
    o->layer=NULL;o->surface=NULL;o->configured=0;
}
static void create_surfaces(void) {
    if(!compositor || !shm || !shell)return;
    for(struct output *o=outputs;o;o=o->next) {
        if(o->closed){destroy_surface(o);continue;}
        if(o->surface)continue;
        o->surface=wl_compositor_create_surface(compositor);
        struct wl_region *region=wl_compositor_create_region(compositor);
        wl_surface_set_input_region(o->surface,region);wl_region_destroy(region);
        o->layer=zwlr_layer_shell_v1_get_layer_surface(shell,o->surface,o->wl,
            ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM,"hypr-visualizer");
        zwlr_layer_surface_v1_set_anchor(o->layer,ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP|
            ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM|ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT|
            ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
        zwlr_layer_surface_v1_set_size(o->layer,0,0);
        zwlr_layer_surface_v1_set_exclusive_zone(o->layer,-1);
        zwlr_layer_surface_v1_set_keyboard_interactivity(o->layer,0);
        zwlr_layer_surface_v1_add_listener(o->layer,&layer_listener,o);
        o->redraw=1;wl_surface_commit(o->surface);
    }
}
static void global(void *data,struct wl_registry *reg,uint32_t name,const char *interface,uint32_t version) {
    (void)data;(void)version;
    if(!strcmp(interface,"wl_compositor")) compositor=wl_registry_bind(reg,name,&wl_compositor_interface,1);
    else if(!strcmp(interface,"wl_shm")) shm=wl_registry_bind(reg,name,&wl_shm_interface,1);
    else if(!strcmp(interface,"zwlr_layer_shell_v1")) shell=wl_registry_bind(reg,name,&zwlr_layer_shell_v1_interface,1);
    else if(!strcmp(interface,"wl_output")) {
        struct output *o=calloc(1,sizeof *o);
        if(!o){running=0;return;}
        o->name=name;o->wl=wl_registry_bind(reg,name,&wl_output_interface,1);
        o->next=outputs;outputs=o;
    }
}
static void removed(void *data,struct wl_registry *reg,uint32_t name) {
    (void)data;(void)reg;
    struct output **link=&outputs;
    while(*link) {
        struct output *o=*link;
        if(o->name==name) {
            *link=o->next;destroy_surface(o);wl_output_destroy(o->wl);free(o);return;
        }
        link=&o->next;
    }
}
static const struct wl_registry_listener registry_listener={global,removed};
struct audio_state {
    pa_mainloop *loop;
    pa_stream *stream;
    float samples[1024], window[1024], coefficients[BAR_COUNT];
    size_t used;
};
static void operation_done(pa_operation *op) { if(op) pa_operation_unref(op); }
static void analyze(struct audio_state *a) {
    float next[BAR_COUNT], raw[BAR_COUNT];
    for(int i=0;i<BAR_COUNT;i++) {
        float q1=0,q2=0;
        for(int j=0;j<1024;j++) {float q=a->window[j]*a->samples[j]+a->coefficients[i]*q1-q2;q2=q1;q1=q;}
        float magnitude=sqrtf(fmaxf(0,q1*q1+q2*q2-a->coefficients[i]*q1*q2))/256;
        float db=20*log10f(magnitude+1e-9f);
        float gain=BASS_GAIN_DB+(TREBLE_GAIN_DB-BASS_GAIN_DB)*i/(BAR_COUNT-1);
        raw[i]=db<=-65?0:fminf(1,fmaxf(0,(db+gain+65)/65));
    }
    for(int i=0;i<BAR_COUNT;i++) {
        float left=raw[i?i-1:i],right=raw[i+1<BAR_COUNT?i+1:i];
        next[i]=0.70f*raw[i]+0.15f*(left+right);
    }
    pthread_mutex_lock(&mutex);memcpy(levels,next,sizeof levels);pthread_mutex_unlock(&mutex);
}
static void audio_read(pa_stream *stream,size_t bytes,void *data) {
    (void)bytes;struct audio_state *a=data;
    const void *chunk;size_t size;
    if(pa_stream_peek(stream,&chunk,&size)<0)return;
    if(!size)return;
    const float *samples=chunk;
    for(size_t i=0;i<size/sizeof(float);i++) {
        a->samples[a->used++]=samples?samples[i]:0;
        if(a->used==1024){analyze(a);a->used=0;}
    }
    pa_stream_drop(stream);
}
static void stream_state(pa_stream *stream,void *data) {
    struct audio_state *a=data;
    if(pa_stream_get_state(stream)==PA_STREAM_FAILED || pa_stream_get_state(stream)==PA_STREAM_TERMINATED)
        pa_mainloop_quit(a->loop,1);
}
static void sink_info(pa_context *context,const pa_sink_info *info,int end,void *data) {
    struct audio_state *a=data;
    if(end || !info || !info->monitor_source_name)return;
    if(a->stream) {
        if(pa_stream_get_state(a->stream)==PA_STREAM_READY)
            operation_done(pa_context_move_source_output_by_name(context,pa_stream_get_index(a->stream),info->monitor_source_name,NULL,NULL));
        return;
    }
    const pa_sample_spec spec={PA_SAMPLE_FLOAT32NE,32000,1};
    const pa_buffer_attr attr={.maxlength=(uint32_t)-1,.tlength=(uint32_t)-1,
        .prebuf=(uint32_t)-1,.minreq=(uint32_t)-1,.fragsize=1024*sizeof(float)};
    a->stream=pa_stream_new(context,"Desktop spectrum",&spec,NULL);
    if(!a->stream){pa_mainloop_quit(a->loop,1);return;}
    pa_stream_set_read_callback(a->stream,audio_read,a);
    pa_stream_set_state_callback(a->stream,stream_state,a);
    if(pa_stream_connect_record(a->stream,info->monitor_source_name,&attr,PA_STREAM_ADJUST_LATENCY)<0)
        pa_mainloop_quit(a->loop,1);
}
static void server_info(pa_context *context,const pa_server_info *info,void *data) {
    if(info && info->default_sink_name)
        operation_done(pa_context_get_sink_info_by_name(context,info->default_sink_name,sink_info,data));
}
static void subscription(pa_context *context,pa_subscription_event_type_t type,uint32_t index,void *data) {
    (void)type;(void)index;
    operation_done(pa_context_get_server_info(context,server_info,data));
}
static void context_state(pa_context *context,void *data) {
    struct audio_state *a=data;
    switch(pa_context_get_state(context)) {
    case PA_CONTEXT_READY:
        pa_context_set_subscribe_callback(context,subscription,a);
        operation_done(pa_context_subscribe(context,PA_SUBSCRIPTION_MASK_SERVER|PA_SUBSCRIPTION_MASK_SINK,NULL,NULL));
        operation_done(pa_context_get_server_info(context,server_info,a));break;
    case PA_CONTEXT_FAILED:case PA_CONTEXT_TERMINATED:pa_mainloop_quit(a->loop,1);break;
    default:break;
    }
}
static void *audio_thread(void *unused) {
    (void)unused;
    struct audio_state a={0};
    for(int j=0;j<1024;j++)a.window[j]=0.5f-0.5f*cosf(2*M_PI*j/1023);
    for(int i=0;i<BAR_COUNT;i++)a.coefficients[i]=2*cosf(2*M_PI*(55*powf(14000.0f/55,i/(float)(BAR_COUNT-1)))/32000);
    while(running) {
        a.loop=pa_mainloop_new();a.stream=NULL;a.used=0;
        if(!a.loop)return NULL;
        pa_context *context=pa_context_new(pa_mainloop_get_api(a.loop),"hypr-visualizer");
        if(!context){pa_mainloop_free(a.loop);return NULL;}
        pa_context_set_state_callback(context,context_state,&a);
        if(pa_context_connect(context,NULL,PA_CONTEXT_NOAUTOSPAWN,NULL)>=0)pa_mainloop_run(a.loop,NULL);
        if(a.stream){pa_stream_set_state_callback(a.stream,NULL,NULL);pa_stream_disconnect(a.stream);pa_stream_unref(a.stream);}
        pa_context_disconnect(context);pa_context_unref(context);pa_mainloop_free(a.loop);
        pthread_mutex_lock(&mutex);memset(levels,0,sizeof levels);pthread_mutex_unlock(&mutex);
        if(running)sleep(2);
    }
    return NULL;
}
static void mirror_spectrum(const float *spectrum,float *target) {
    const int half=(BAR_COUNT+1)/2;
    for(int i=0;i<BAR_COUNT;i++) {
        int band=i<half?half-1-i:i-BAR_COUNT/2;
        int start=band*BAR_COUNT/half,end=(band+1)*BAR_COUNT/half;
        float sum=0;
        for(int j=start;j<end;j++)sum+=spectrum[j];
        target[i]=sum/(end-start);
    }
}
static void display_levels(float *target) {
    float spectrum[BAR_COUNT];
    pthread_mutex_lock(&mutex);memcpy(spectrum,levels,sizeof spectrum);pthread_mutex_unlock(&mutex);
    mirror_spectrum(spectrum,target);
}
static float animate_level(float current,float target) {
    float tau=target>current?ATTACK_SECONDS:RELEASE_SECONDS;
    float value=current+(target-current)*(1-expf(-1.0f/(FPS*tau)));
    return value<0.001f?0:value;
}
static int draw(struct output *o) {
    int width=o->width,height=o->height,force=o->redraw;
    float *shown=o->shown;
    struct buffer *buffers=o->buffers;
    struct wl_surface *surface=o->surface;
    float target[BAR_COUNT];int changed=force;
    display_levels(target);
    for(int i=0;i<BAR_COUNT;i++) {
        float value=animate_level(shown[i],target[i]);
        if(fabsf(value-shown[i])*height>0.1f)changed=1;
        shown[i]=value;
    }
    if(!changed)return 1;
    struct buffer *b=NULL;
    for(int i=0;i<2;i++)if(!buffers[i].busy){b=&buffers[i];break;}
    if(!b)return 0;
    if(!b->wl && !make_buffer(o,b)){running=0;return 0;}
    cairo_surface_t *cs=cairo_image_surface_create_for_data(b->data,CAIRO_FORMAT_ARGB32,width,height,width*4);
    cairo_t *cr=cairo_create(cs);
    cairo_set_operator(cr,CAIRO_OPERATOR_CLEAR);cairo_paint(cr);cairo_set_operator(cr,CAIRO_OPERATOR_OVER);
    double step=(double)width/BAR_COUNT, thickness=step*BAR_WIDTH_RATIO;
    for(int i=0;i<BAR_COUNT;i++) {
        double bar=shown[i]*fmax(0,height*BAR_HEIGHT_RATIO-thickness);
        if(bar<1)continue;
        double p=(double)i/(BAR_COUNT-1)*(sizeof palette/sizeof *palette-1);
        int a=(int)p, z=a+1<(int)(sizeof palette/sizeof *palette)?a+1:a;
        double t=p-a;unsigned c=palette[a],d=palette[z];
        double r=(((c>>16)&255)*(1-t)+((d>>16)&255)*t)/255;
        double g=(((c>>8)&255)*(1-t)+((d>>8)&255)*t)/255;
        double blue=((c&255)*(1-t)+(d&255)*t)/255;
        cairo_pattern_t *gradient=cairo_pattern_create_linear(0,height-bar-thickness/2,0,height);
        cairo_pattern_add_color_stop_rgba(gradient,0,r,g,blue,OPACITY);
        cairo_pattern_add_color_stop_rgba(gradient,0.65,r,g,blue,OPACITY);
        cairo_pattern_add_color_stop_rgba(gradient,1,r,g,blue,OPACITY*BASE_ALPHA);
        cairo_set_source(cr,gradient);cairo_pattern_destroy(gradient);
        cairo_set_line_width(cr,thickness);cairo_set_line_cap(cr,CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr,(i+0.5)*step,height+thickness);cairo_line_to(cr,(i+0.5)*step,height-bar);cairo_stroke(cr);
    }
    cairo_destroy(cr);cairo_surface_destroy(cs);
    b->busy=1;wl_surface_attach(surface,b->wl,0,0);wl_surface_damage(surface,0,0,width,height);wl_surface_commit(surface);return 1;
}
static int stream_levels(void) {
    signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGPIPE,SIG_IGN);
    pthread_t thread;
    if(pthread_create(&thread,NULL,audio_thread,NULL))return 1;
    float values[BAR_COUNT]={0};
    while(running) {
        float target[BAR_COUNT];
        display_levels(target);
        printf("{\"height\":%.3f,\"width\":%.3f,\"opacity\":%.3f,\"baseAlpha\":%.3f,\"levels\":[",BAR_HEIGHT_RATIO,BAR_WIDTH_RATIO,OPACITY,BASE_ALPHA);
        for(int i=0;i<BAR_COUNT;i++) {
            values[i]=animate_level(values[i],target[i]);
            printf("%s%.4f",i?",":"",values[i]);
        }
        printf("],\"colors\":[");
        for(int i=0;i<BAR_COUNT;i++) {
            double position=(double)i/(BAR_COUNT-1)*(sizeof palette/sizeof *palette-1);
            int a=(int)position,z=a+1<(int)(sizeof palette/sizeof *palette)?a+1:a;
            double t=position-a;unsigned c=palette[a],d=palette[z];
            unsigned r=((c>>16)&255)*(1-t)+((d>>16)&255)*t;
            unsigned g=((c>>8)&255)*(1-t)+((d>>8)&255)*t;
            unsigned b=(c&255)*(1-t)+(d&255)*t;
            printf("%s\"#%02x%02x%02x\"",i?",":"",r,g,b);
        }
        puts("]}");
        if(fflush(stdout)==EOF)break;
        struct timespec delay={0,1000000000/FPS};nanosleep(&delay,NULL);
    }
    return 0;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--levels"))return stream_levels();
    if(argc>1) {if(!strcmp(argv[1],"--version")){puts("hypr-visualizer 0.4.0");return 0;}
        fprintf(stderr,"Usage: hypr-visualizer [--version]\n");return 2;}
    if(!getenv("HYPRLAND_INSTANCE_SIGNATURE")){fprintf(stderr,"Run inside a Hyprland session.\n");return 1;}
    const char *runtime=getenv("XDG_RUNTIME_DIR");char lockpath[4096];
    if(!runtime){fprintf(stderr,"Run inside a Wayland desktop session.\n");return 1;}
    snprintf(lockpath,sizeof lockpath,"%s/hypr-visualizer.lock",runtime);
    int lock=open(lockpath,O_CREAT|O_RDWR|O_CLOEXEC,0600);
    if(lock<0 || flock(lock,LOCK_EX|LOCK_NB)<0){fprintf(stderr,"Already running or cannot lock runtime directory.\n");return 1;}
    signal(SIGINT,stop);signal(SIGTERM,stop);
    display=wl_display_connect(NULL);
    if(!display){fprintf(stderr,"Cannot connect to Wayland.\n");return 1;}
    struct wl_registry *reg=wl_display_get_registry(display);
    wl_registry_add_listener(reg,&registry_listener,NULL);wl_display_roundtrip(display);
    if(!compositor||!shm||!shell){fprintf(stderr,"A layer-shell compositor such as Hyprland is required.\n");return 1;}
    create_surfaces();
    pthread_t thread;
    if(pthread_create(&thread,NULL,audio_thread,NULL)){fprintf(stderr,"Cannot start audio thread.\n");return 1;}
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    long long next=now.tv_sec*1000LL+now.tv_nsec/1000000;
    while(running) {
        while(wl_display_prepare_read(display)!=0)if(wl_display_dispatch_pending(display)<0)goto done;
        wl_display_flush(display);
        struct pollfd fd={wl_display_get_fd(display),POLLIN,0};
        clock_gettime(CLOCK_MONOTONIC,&now);
        long long current=now.tv_sec*1000LL+now.tv_nsec/1000000;
        int result=poll(&fd,1,current<next?(int)(next-current):0);
        if(result>0 && (fd.revents&POLLIN)){if(wl_display_read_events(display)<0)break;}
        else wl_display_cancel_read(display);
        if(result>0 && (fd.revents&(POLLERR|POLLHUP)))break;
        if(wl_display_dispatch_pending(display)<0)break;
        clock_gettime(CLOCK_MONOTONIC,&now);
        current=now.tv_sec*1000LL+now.tv_nsec/1000000;
        if(current>=next) {
            create_surfaces();
            for(struct output *o=outputs;o;o=o->next)
                if(o->configured && draw(o))o->redraw=0;
            next=current+1000/FPS;
        }
    }
 done:
    /* Process exit also closes the blocking audio stream and its thread. */
    while(outputs){struct output *o=outputs;outputs=o->next;destroy_surface(o);wl_output_destroy(o->wl);free(o);}
    wl_registry_destroy(reg);wl_display_disconnect(display);close(lock);return 0;
}
