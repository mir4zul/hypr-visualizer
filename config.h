/* Runtime settings are read only by the rendering thread. Audio capture keeps
 * its fixed analysis bands; movement shaping happens after the spectrum copy. */
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define MAX_COLORS 32
struct settings {
    double bar_width, gap, height, opacity, glow, attack, release;
    int style, natural, wallpaper_mode, lockscreen, color_count;
    unsigned colors[MAX_COLORS];
};
static struct settings settings;
static char config_path[4096];
static time_t wallpaper_mtime;
static void default_settings(struct settings *s) {
    memset(s,0,sizeof *s);
    s->bar_width=BAR_SPACING*BAR_WIDTH_RATIO;s->gap=BAR_SPACING-s->bar_width;
    s->height=BAR_HEIGHT_RATIO;s->opacity=OPACITY;
    s->lockscreen=1;
    s->attack=ATTACK_SECONDS;s->release=RELEASE_SECONDS;
    s->color_count=sizeof palette/sizeof *palette;
    memcpy(s->colors,palette,sizeof palette);
}
static char *trim(char *s) {
    while(isspace((unsigned char)*s))s++;
    char *end=s+strlen(s);
    while(end>s && isspace((unsigned char)end[-1]))*--end=0;
    return s;
}
static int setting_number(const char *text,double lo,double hi,double *out) {
    char *end;errno=0;double n=strtod(text,&end);
    if(end==text || *end || errno || !isfinite(n) || n<lo || n>hi)return 0;
    *out=n;return 1;
}
static int setting_colors(struct settings *s,char *text) {
    unsigned colors[MAX_COLORS];int n=0;char *save;
    for(char *p=strtok_r(text,",",&save);p;p=strtok_r(NULL,",",&save)) {
        p=trim(p);if(*p=='#')p++;
        if(strlen(p)!=6 || n==MAX_COLORS)return 0;
        for(int i=0;i<6;i++)if(!isxdigit((unsigned char)p[i]))return 0;
        colors[n++]=strtoul(p,NULL,16);
    }
    if(n<2)return 0;
    memcpy(s->colors,colors,n*sizeof *colors);s->color_count=n;return 1;
}
static int setting_apply(struct settings *s,char *key,char *value) {
    if(!strcmp(key,"theme")) {
        if(!strcmp(value,"classic")) {
            s->wallpaper_mode=0;
            s->color_count=sizeof palette/sizeof *palette;
            memcpy(s->colors,palette,sizeof palette);return 1;
        }
        if(!strcmp(value,"custom")) { s->wallpaper_mode=0; return 1; }
        if(!strcmp(value,"wallpaper")) { s->wallpaper_mode=1; return 1; }
        const char *colors=NULL;
        if(!strcmp(value,"aurora"))colors="#63e6be,#74c0fc,#b197fc,#f783ac";
        if(!strcmp(value,"sunset"))colors="#ffcf70,#ff9068,#ed648e,#8d78dc";
        if(!strcmp(value,"ocean"))colors="#91f2d0,#37c6d0,#4288e8,#a49bff";
        if(!colors)return 0;
        s->wallpaper_mode=0;
        char copy[128];snprintf(copy,sizeof copy,"%s",colors);return setting_colors(s,copy);
    }
    if(!strcmp(key,"colors"))return setting_colors(s,value);
    if(!strcmp(key,"style")) {
        const char *names[]={"bars","wave","lines","pill"};
        for(int i=0;i<4;i++)if(!strcmp(value,names[i])){s->style=i;return 1;}
        return 0;
    }
    if(!strcmp(key,"motion")) {
        if(strcmp(value,"classic") && strcmp(value,"natural"))return 0;
        s->natural=!strcmp(value,"natural");return 1;
    }
    if(!strcmp(key,"lockscreen")) {
        if(!strcmp(value,"on") || !strcmp(value,"yes") || !strcmp(value,"true")) { s->lockscreen=1; return 1; }
        if(!strcmp(value,"off") || !strcmp(value,"no") || !strcmp(value,"false")) { s->lockscreen=0; return 1; }
        return 0;
    }
    if(!strcmp(key,"bar_width"))return setting_number(value,1,200,&s->bar_width);
    if(!strcmp(key,"gap"))return setting_number(value,0,200,&s->gap);
    if(!strcmp(key,"height"))return setting_number(value,0.02,1,&s->height);
    if(!strcmp(key,"opacity"))return setting_number(value,0,1,&s->opacity);
    if(!strcmp(key,"glow"))return setting_number(value,0,1,&s->glow);
    if(!strcmp(key,"attack"))return setting_number(value,0.02,1,&s->attack);
    if(!strcmp(key,"release"))return setting_number(value,0.05,2,&s->release);
    return 0;
}
static int read_settings(const char *path,struct settings *out) {
    FILE *file=fopen(path,"r");if(!file)return errno==ENOENT?0:-1;
    struct settings next;default_settings(&next);
    char line[1024];int valid=1;
    while(fgets(line,sizeof line,file)) {
        if(!strchr(line,'\n') && !feof(file)){valid=0;break;}
        char *key=trim(line);if(!*key || *key=='#')continue;
        char *value=strchr(key,'=');if(!value){valid=0;break;}
        *value++=0;
        if(!setting_apply(&next,trim(key),trim(value))){valid=0;break;}
    }
    if(ferror(file))valid=0;
    fclose(file);if(!valid)return -1;*out=next;return 1;
}
/* DMS persists the active wallpaper in session.json. Refresh its palette when
 * that file changes, so wallpaper changes outside our settings window appear. */
static void sync_wallpaper(const struct settings *next) {
    if (!next->wallpaper_mode) return;
    const char *state=getenv("XDG_STATE_HOME"),*home=getenv("HOME");
    char session[4096];
    if(state && *state)snprintf(session,sizeof session,"%s/DankMaterialShell/session.json",state);
    else if(home)snprintf(session,sizeof session,"%s/.local/state/DankMaterialShell/session.json",home);
    else return;
    struct stat st;if(stat(session,&st)<0 || st.st_mtime==wallpaper_mtime)return;
    wallpaper_mtime=st.st_mtime;
    FILE *file=fopen(session,"r");if(!file)return;
    char line[8192],path[4096]={0};
    while(fgets(line,sizeof line,file)) {
        char *key=strstr(line,"\"wallpaperPath\"");if(!key)continue;
        char *quote=strchr(key+15,'\"');if(!quote)continue;quote++;
        char *end=strchr(quote,'\"');if(!end)continue;
        size_t length=(size_t)(end-quote);if(length>=sizeof path)continue;
        memcpy(path,quote,length);path[length]=0;break;
    }
    fclose(file);if(!path[0] || path[0]=='#' || access(path,R_OK))return;
    pid_t child=fork();
    if(child==0) {
        char helper[4096];if(!home)_exit(0);
        snprintf(helper,sizeof helper,"%s/.local/bin/hypr-visualizer-settings",home);
        execl(helper,helper,"--wallpaper",path,(char *)NULL);_exit(127);
    }
}
static void init_settings(void) {
    default_settings(&settings);
    const char *config=getenv("XDG_CONFIG_HOME"),*home=getenv("HOME");
    if(config && *config)snprintf(config_path,sizeof config_path,"%s/hypr-visualizer/config",config);
    else if(home)snprintf(config_path,sizeof config_path,"%s/.config/hypr-visualizer/config",home);
}
static int reload_settings(void) {
    static struct timespec last={0};struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    if((now.tv_sec-last.tv_sec)*1000000000LL+now.tv_nsec-last.tv_nsec<100000000LL)return 0;
    last=now;
    struct settings next;default_settings(&next);
    int result=read_settings(config_path,&next);
    if(result<0)return 0; /* Keep last valid settings during an incomplete edit. */
    sync_wallpaper(&next);
    if(!memcmp(&next,&settings,sizeof next))return 0;
    settings=next;return 1;
}
static unsigned color_at(double position) {
    double p=fmax(0,fmin(1,position))*(settings.color_count-1);
    int a=(int)p,b=a+1<settings.color_count?a+1:a;double t=p-a;
    unsigned c=settings.colors[a],d=settings.colors[b],out=0;
    for(int shift=0;shift<=16;shift+=8)
        out|=(unsigned)((((c>>shift)&255)*(1-t)+((d>>shift)&255)*t))<<shift;
    return out;
}
