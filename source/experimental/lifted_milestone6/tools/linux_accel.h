/* Linux IIO accelerometer -> original Android JNI acceleration events.
 * Included by host.c; no synthetic acceleration or gameplay-state changes.
 */
#pragma once
#include <stdatomic.h>
static atomic_int ds_accel_enabled;
static atomic_int ds_accel_delay_ms = 20;
static void ds_accel_set_enabled(int enabled) {
    atomic_store(&ds_accel_enabled, !!enabled);
    if (getenv("DS_ACCEL_DIAG"))
        fprintf(stderr,"[ACCEL_CONTROL] enabled=%d\n",!!enabled);
}
static void ds_accel_set_delay(int category) {
    /* Original Android delegate receives SENSOR_DELAY_* (0..3). */
    int ms = category==0 ? 10 : category==1 ? 20 : category==2 ? 60 : 200;
    atomic_store(&ds_accel_delay_ms, ms);
}
#if defined(__linux__) && defined(DS_SDL)
#ifndef DS_IIO_ROOT
#define DS_IIO_ROOT "/sys/bus/iio/devices"
#endif
static char ds_accel_path[4096];
static float ds_accel_scale[3], ds_accel_offset[3];
static float ds_accel_matrix[9] = {1,0,0,0,1,0,0,0,1};
static int ds_accel_found = -1;
static uint64_t ds_accel_last_ms;
static unsigned ds_accel_samples, ds_accel_errors;
static int ds_accel_text(const char* dir,const char* name,char* out,size_t size) {
    char path[4096];
    if(snprintf(path,sizeof path,"%s/%s",dir,name)>=(int)sizeof path)return 0;
    FILE* f=fopen(path,"r");if(!f)return 0;
    int ok=fgets(out,(int)size,f)!=NULL;fclose(f);return ok;
}
static int ds_accel_number(const char* dir,const char* name,float* out) {
    char s[128],*end;
    if(!ds_accel_text(dir,name,s,sizeof s))return 0;
    float v=strtof(s,&end);
    if(end==s || !isfinite(v))return 0;
    *out=v;return 1;
}
static int ds_accel_parse_matrix(const char* s,float m[9]) {
    float v[9];int n=0;
    while(*s && n<9) {
        if(*s==',' || *s==';' || *s==' ' || *s=='\t' || *s=='\n'){s++;continue;}
        char* end;v[n]=strtof(s,&end);
        if(end==s || !isfinite(v[n]))return 0;
        n++;s=end;
    }
    if(n!=9)return 0;
    while(*s==' ' || *s=='\t' || *s=='\n')s++;
    if(*s)return 0;
    for(int row=0;row<3;row++) {
        float norm=0;
        for(int col=0;col<3;col++)norm+=v[3*row+col]*v[3*row+col];
        if(fabsf(norm-1.0f)>0.01f)return 0;
        for(int other=0;other<row;other++) {
            float dot=0;for(int col=0;col<3;col++)dot+=v[3*row+col]*v[3*other+col];
            if(fabsf(dot)>0.01f)return 0;
        }
    }
    memcpy(m,v,sizeof v);return 1;
}
static void ds_accel_mount_matrix(void) {
    char s[1024];const char* override=getenv("DS_ACCEL_MATRIX");
    if(override) {
        if(!ds_accel_parse_matrix(override,ds_accel_matrix))
            fprintf(stderr,"[ACCEL_WARNING] invalid DS_ACCEL_MATRIX; using identity\n");
        return;
    }
    if((ds_accel_text(ds_accel_path,"in_accel_mount_matrix",s,sizeof s) ||
        ds_accel_text(ds_accel_path,"mount_matrix",s,sizeof s)) &&
        ds_accel_parse_matrix(s,ds_accel_matrix))return;
    /* udev's hardware database supplies mounting corrections on Chromebooks. */
    unsigned major,minor;
    if(ds_accel_text(ds_accel_path,"dev",s,sizeof s) && sscanf(s,"%u:%u",&major,&minor)==2) {
        char path[128];snprintf(path,sizeof path,"/run/udev/data/c%u:%u",major,minor);
        FILE* f=fopen(path,"r");if(!f)return;
        while(fgets(s,sizeof s,f))if(!strncmp(s,"E:ACCEL_MOUNT_MATRIX=",21)) {
            ds_accel_parse_matrix(s+21,ds_accel_matrix);break;
        }
        fclose(f);
    }
}
static int ds_accel_probe(const char* path,float scale[3],float offset[3]) {
    for(int i=0;i<3;i++) {
        char name[64];float raw;
        snprintf(name,sizeof name,"in_accel_%c_raw",'x'+i);
        if(!ds_accel_number(path,name,&raw))return 0;
        snprintf(name,sizeof name,"in_accel_%c_scale",'x'+i);
        if(!ds_accel_number(path,name,&scale[i]) &&
           !ds_accel_number(path,"in_accel_scale",&scale[i]) &&
           !ds_accel_number(path,"scale",&scale[i]))return 0;
        if(scale[i]<=0)return 0;
        offset[i]=0;snprintf(name,sizeof name,"in_accel_%c_offset",'x'+i);
        if(!ds_accel_number(path,name,&offset[i]))
            ds_accel_number(path,"in_accel_offset",&offset[i]);
    }
    return 1;
}
static int ds_accel_init(void) {
    if(ds_accel_found>=0)return ds_accel_found;
    ds_accel_found=0;
    const char* chosen=getenv("DS_ACCEL_DEVICE");
    if(chosen) {
        if(strlen(chosen)<sizeof ds_accel_path &&
           ds_accel_probe(chosen,ds_accel_scale,ds_accel_offset)) {
            strcpy(ds_accel_path,chosen);ds_accel_found=1;
        }
    } else {
        DIR* d=opendir(DS_IIO_ROOT);struct dirent* ent;int best=-1;
        if(d) {
            while((ent=readdir(d)))if(!strncmp(ent->d_name,"iio:device",10)) {
                char path[4096],label[128]="";float scale[3],offset[3];
                snprintf(path,sizeof path,"%s/%s",DS_IIO_ROOT,ent->d_name);
                if(!ds_accel_probe(path,scale,offset))continue;
                ds_accel_text(path,"label",label,sizeof label);
                int score=strstr(label,"display") || strstr(label,"lid") ? 100 : strstr(label,"base") ? 0 : 50;
                if(score<=best)continue;
                best=score;strcpy(ds_accel_path,path);
                memcpy(ds_accel_scale,scale,sizeof scale);memcpy(ds_accel_offset,offset,sizeof offset);
                ds_accel_found=1;
            }
            closedir(d);
        }
    }
    if(ds_accel_found) {
        ds_accel_mount_matrix();
        fprintf(stderr,"[LINUX_ACCEL] device=%s scale=(%g,%g,%g) matrix=(%g,%g,%g;%g,%g,%g;%g,%g,%g) units=m/s2\n",
                ds_accel_path,ds_accel_scale[0],ds_accel_scale[1],ds_accel_scale[2],
                ds_accel_matrix[0],ds_accel_matrix[1],ds_accel_matrix[2],
                ds_accel_matrix[3],ds_accel_matrix[4],ds_accel_matrix[5],
                ds_accel_matrix[6],ds_accel_matrix[7],ds_accel_matrix[8]);
    } else fprintf(stderr,"[LINUX_ACCEL] no readable IIO accelerometer; motion input unavailable\n");
    return ds_accel_found;
}
static int ds_accel_poll(uint64_t now,float xyz[3]) {
    if(!atomic_load(&ds_accel_enabled) || !ds_accel_init())return 0;
    if(ds_accel_samples && now-ds_accel_last_ms<(unsigned)atomic_load(&ds_accel_delay_ms))return 0;
    ds_accel_last_ms=now;
    float v[3];
    for(int i=0;i<3;i++) {
        char name[64];float raw;snprintf(name,sizeof name,"in_accel_%c_raw",'x'+i);
        if(!ds_accel_number(ds_accel_path,name,&raw)) {
            if(ds_accel_errors++%100==0)fprintf(stderr,"[ACCEL_WARNING] sensor read failed: %s\n",name);
            return 0;
        }
        v[i]=(raw+ds_accel_offset[i])*ds_accel_scale[i];
    }
    for(int i=0;i<3;i++)xyz[i]=ds_accel_matrix[3*i]*v[0]+ds_accel_matrix[3*i+1]*v[1]+ds_accel_matrix[3*i+2]*v[2];
    ds_accel_samples++;
    if(getenv("DS_ACCEL_DIAG") && (ds_accel_samples<=8 || ds_accel_samples%100==0))
        fprintf(stderr,"[ACCEL_SAMPLE] n=%u xyz=(%.5f,%.5f,%.5f) m/s2\n",ds_accel_samples,xyz[0],xyz[1],xyz[2]);
    return 1;
}
#else
static int ds_accel_init(void) { return 0; }
#endif
