/*
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║     SERC Mini-OS  —  CS 225: Introduction to Operating Systems  ║
 * ║     Copperbelt University  |  Dr Derrick Ntalasha               ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  OS Components:                                                  ║
 * ║   1. Process Management  (PCB, 5-state machine)                  ║
 * ║   2. CPU Scheduling      (FCFS, SJF, Priority, Round-Robin)      ║
 * ║   3. Memory Management   (First-Fit + fragmentation tracking)    ║
 * ║   4. IPC                 (message queue / pipe simulation)       ║
 * ║   5. Deadlock Handling   (Banker's Algorithm)                    ║
 * ║   6. File Management     (real-time log to serc_log.txt)         ║
 * ╚══════════════════════════════════════════════════════════════════╝
 *
 *  Build:  make
 *  Run:    ./serc_mini_os
 *
 *  All state is in-memory only — restarting resets everything.
 */

#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ─── CONSTANTS ───────────────────────────────────────────────────── */
#define WIN_W        1200
#define WIN_H        720
#define SB_W         210       /* sidebar width */
#define STB_H        28        /* status bar height */
#define MAX_PROC     12
#define MAX_MEM      256
#define MAX_RES      4
#define MAX_LOG      300
#define LOG_LINE     160
#define RR_QUANTUM   3
#define IPC_Q_SZ     16

/* ─── ENUMS ───────────────────────────────────────────────────────── */
typedef enum { NEW=0, READY, RUNNING, WAITING, TERMINATED } PState;
typedef enum { FCFS=0, SJF, PRIO, RR }                     SchedAlgo;
typedef enum { TAB_DASH=0,TAB_PROC,TAB_MEM,TAB_IPC,TAB_DEAD,TAB_LOG,TAB_N } Tab;

/* ─── STRUCTURES ──────────────────────────────────────────────────── */
typedef struct {
    int    pid;
    char   name[36];
    PState state;
    int    priority;          /* 1=CRITICAL 2=HIGH 3=NORMAL */
    int    burst, remaining;
    int    arrival, finish_tick;
    int    waiting_time, turnaround;
    int    mem_needed, mem_start, mem_ok;
    int    res[MAX_RES], res_max[MAX_RES];
    float  anim_y, bar_anim;  /* slide-in and progress animation */
} PCB;

typedef struct { int start,size,free,owner; } MemBlock;

typedef struct {
    int  from_pid, to_pid, tick_sent;
    char msg[64];
} IPCMsg;

/* ─── GLOBALS ─────────────────────────────────────────────────────── */
PCB       procs[MAX_PROC];
int       proc_cnt=0, next_pid=1, running_idx=-1;
int       time_tick=0, rr_ctr=0;
SchedAlgo cur_algo=FCFS;

#define   MAX_MBLK 32
MemBlock  mblk[MAX_MBLK];
int       mblk_cnt=0;

int  res_total[MAX_RES]={3,4,3,2};
int  res_avail[MAX_RES]={3,4,3,2};
const char *res_names[MAX_RES]={"Comms","Vehicles","Radios","Officers"};

IPCMsg ipc_q[IPC_Q_SZ];
int    ipc_head=0,ipc_tail=0,ipc_cnt=0;

char log_buf[MAX_LOG][LOG_LINE];
int  log_cnt=0;
float log_scroll=0;

Tab   cur_tab=TAB_DASH;
int   show_add=0;
float add_anim=0;

char  dlg_name[36]="";
int   dlg_prio=1,dlg_burst=6,dlg_mem=16,dlg_field=0;

Font  fnt, fmono;
float tick_flash=0;

/* ─── AUTO-RUN ────────────────────────────────────────────────────── */
int   auto_run=0;          /* 0=off 1=on */
float auto_speed=1.0f;     /* ticks per second: 0.5, 1, 2, 4 */
float auto_accum=0.0f;     /* accumulated time since last auto-tick */

/* ─── COLORS ──────────────────────────────────────────────────────── */
#define CBG     (Color){13,15,23,255}
#define CSB     (Color){18,21,33,255}
#define CPANEL  (Color){22,26,40,255}
#define CCARD   (Color){28,33,50,255}
#define CCARD2  (Color){32,38,58,255}
#define CBORD   (Color){45,52,75,255}
#define CACC    (Color){82,130,255,255}
#define CTEXT   (Color){210,218,240,255}
#define CDIM    (Color){100,112,148,255}
#define CGREEN  (Color){60,210,120,255}
#define CRED    (Color){255,75,75,255}
#define CORANG  (Color){255,165,50,255}
#define CYEL    (Color){255,220,60,255}
#define CPINK   (Color){255,100,180,255}

static Color state_col(PState s){
    switch(s){case NEW:return CACC;case READY:return CYEL;case RUNNING:return CGREEN;
               case WAITING:return CORANG;case TERMINATED:return CDIM;}return WHITE;}
static Color prio_col(int p){if(p==1)return CRED;if(p==2)return CORANG;return CGREEN;}
static const char* sstr(PState s){
    switch(s){case NEW:return"NEW";case READY:return"READY";case RUNNING:return"RUNNING";
               case WAITING:return"WAITING";case TERMINATED:return"DONE";}return"?";}
static const char* pstr(int p){if(p==1)return"CRITICAL";if(p==2)return"HIGH";return"NORMAL";}
static const char* astr(SchedAlgo a){
    switch(a){case FCFS:return"FCFS";case SJF:return"SJF";
               case PRIO:return"Priority";case RR:return"Round Robin";}return"?";}

/* ─── DRAW HELPERS ────────────────────────────────────────────────── */
static void rr(float x,float y,float w,float h,float r,Color c){
    DrawRectangleRounded((Rectangle){x,y,w,h},r,8,c);}
static void rrl(float x,float y,float w,float h,float r,Color c){
    DrawRectangleRoundedLines((Rectangle){x,y,w,h},r,8,c);}
static void dt(const char*t,float x,float y,float sz,Color c){
    DrawTextEx(fnt,t,(Vector2){x,y},sz,0.3f,c);}
static void dm(const char*t,float x,float y,float sz,Color c){
    DrawTextEx(fmono,t,(Vector2){x,y},sz,0,c);}
static float tw(const char*t,float sz){return MeasureTextEx(fnt,t,sz,0.3f).x;}
static float mw(const char*t,float sz){return MeasureTextEx(fmono,t,sz,0).x;}
static void badge(float x,float y,const char*t,Color bg){
    float w2=mw(t,11);
    rr(x,y,w2+12,18,0.6f,bg);
    DrawTextEx(fmono,t,(Vector2){x+6,y+3},11,0,WHITE);}

/* ─── LOG ─────────────────────────────────────────────────────────── */
static void wlog(const char*msg){
    if(log_cnt<MAX_LOG) strncpy(log_buf[log_cnt++],msg,LOG_LINE-1);
    else{ memmove(log_buf[0],log_buf[1],(MAX_LOG-1)*LOG_LINE);
          strncpy(log_buf[MAX_LOG-1],msg,LOG_LINE-1); }
    /* File Management: append to disk */
    FILE*f=fopen("serc_log.txt","a");
    if(f){fprintf(f,"[T%04d] %s\n",time_tick,msg);fclose(f);}
}

/* ─── MEMORY ──────────────────────────────────────────────────────── */
static void mem_init(void){
    mblk_cnt=1; mblk[0]=(MemBlock){0,MAX_MEM,1,-1};}
static int mem_alloc(int pid,int sz){
    for(int i=0;i<mblk_cnt;i++){
        if(mblk[i].free&&mblk[i].size>=sz){
            int s=mblk[i].start;
            if(mblk[i].size>sz+4&&mblk_cnt<MAX_MBLK){
                memmove(&mblk[i+1],&mblk[i],(mblk_cnt-i)*sizeof(MemBlock));
                mblk_cnt++;
                mblk[i+1]=(MemBlock){s+sz,mblk[i].size-sz,1,-1};
                mblk[i].size=sz;
            }
            mblk[i].free=0; mblk[i].owner=pid; return s;
        }
    } return -1;
}
static void mem_free(int pid){
    for(int i=0;i<mblk_cnt;i++)
        if(!mblk[i].free&&mblk[i].owner==pid){mblk[i].free=1;mblk[i].owner=-1;}
    for(int i=0;i<mblk_cnt-1;i++)
        if(mblk[i].free&&mblk[i+1].free){
            mblk[i].size+=mblk[i+1].size;
            memmove(&mblk[i+1],&mblk[i+2],(mblk_cnt-i-2)*sizeof(MemBlock));
            mblk_cnt--;i--;
        }
}
static int mem_used(void){int u=0;for(int i=0;i<mblk_cnt;i++)if(!mblk[i].free)u+=mblk[i].size;return u;}
static int mem_frags(void){int f=0;for(int i=0;i<mblk_cnt;i++)if(mblk[i].free)f++;return f>1?f-1:0;}

/* ─── IPC ─────────────────────────────────────────────────────────── */
static void ipc_send(int from,int to,const char*msg){
    if(ipc_cnt>=IPC_Q_SZ)return;
    ipc_q[ipc_tail]=(IPCMsg){from,to,time_tick,""}; 
    strncpy(ipc_q[ipc_tail].msg,msg,63);
    ipc_tail=(ipc_tail+1)%IPC_Q_SZ; ipc_cnt++;
    char buf[LOG_LINE]; snprintf(buf,LOG_LINE,"[IPC] PID %d → PID %d: \"%s\"",from,to,msg);
    wlog(buf);
}

/* ─── DEADLOCK (Banker's) ─────────────────────────────────────────── */
static int banker_safe(void){
    int work[MAX_RES]; memcpy(work,res_avail,sizeof(work));
    int finish[MAX_PROC]={0}; int found=1;
    while(found){found=0;
        for(int i=0;i<proc_cnt;i++){
            if(finish[i]||procs[i].state==TERMINATED){finish[i]=1;continue;}
            int ok=1;
            for(int r=0;r<MAX_RES;r++)if(procs[i].res_max[r]-procs[i].res[r]>work[r]){ok=0;break;}
            if(ok){for(int r=0;r<MAX_RES;r++)work[r]+=procs[i].res[r];finish[i]=1;found=1;}
        }
    }
    for(int i=0;i<proc_cnt;i++)if(!finish[i]&&procs[i].state!=TERMINATED)return 0;
    return 1;
}

/* ─── PROCESS MANAGEMENT ──────────────────────────────────────────── */
static void proc_terminate(int idx){
    PCB*p=&procs[idx]; if(p->state==TERMINATED)return;
    for(int r=0;r<MAX_RES;r++){res_avail[r]+=p->res[r];p->res[r]=0;}
    if(p->mem_ok){mem_free(p->pid);p->mem_ok=0;}
    p->state=TERMINATED; p->finish_tick=time_tick;
    p->turnaround=time_tick-p->arrival;
    char buf[LOG_LINE];
    snprintf(buf,LOG_LINE,"[-] TERMINATED: %s (PID %d) | WT:%d | TAT:%d",
             p->name,p->pid,p->waiting_time,p->turnaround);
    wlog(buf);
}

static void proc_create(const char*name,int prio,int burst,int mem){
    if(proc_cnt>=MAX_PROC){wlog("[!] Process table full");return;}
    PCB*p=&procs[proc_cnt]; memset(p,0,sizeof(PCB));
    p->pid=next_pid++; strncpy(p->name,name,sizeof(p->name)-1); p->name[sizeof(p->name)-1]=0;
    p->state=NEW; p->priority=prio; p->burst=burst;
    p->remaining=burst; p->arrival=time_tick;
    p->mem_needed=mem; p->mem_start=-1;
    p->anim_y=WIN_H; p->bar_anim=0;
    for(int r=0;r<MAX_RES;r++) p->res_max[r]=rand()%2;
    int s=mem_alloc(p->pid,mem);
    if(s>=0){p->mem_start=s;p->mem_ok=1;p->state=READY;
        char buf[LOG_LINE];
        snprintf(buf,LOG_LINE,"[+] Created: %s (PID %d) | %s | Burst:%d | Mem:%dMB @%d",
                 name,p->pid,pstr(prio),burst,mem,s); wlog(buf);
    } else {
        p->state=WAITING;
        char buf[LOG_LINE];
        snprintf(buf,LOG_LINE,"[W] %s (PID %d) WAITING — no memory (%dMB)",name,p->pid,mem);
        wlog(buf);
    }
    if(proc_cnt>0&&procs[0].state!=TERMINATED)
        ipc_send(procs[0].pid,p->pid,"TASK_READY");
    proc_cnt++;
}

/* ─── SCHEDULER ───────────────────────────────────────────────────── */
static int sched_fcfs(void){int b=-1,bv=999999;for(int i=0;i<proc_cnt;i++)if(procs[i].state==READY&&procs[i].mem_ok&&procs[i].arrival<bv){bv=procs[i].arrival;b=i;}return b;}
static int sched_sjf(void){int b=-1,bv=999999;for(int i=0;i<proc_cnt;i++)if(procs[i].state==READY&&procs[i].mem_ok&&procs[i].remaining<bv){bv=procs[i].remaining;b=i;}return b;}
static int sched_prio(void){int b=-1,bv=999;for(int i=0;i<proc_cnt;i++)if(procs[i].state==READY&&procs[i].mem_ok&&procs[i].priority<bv){bv=procs[i].priority;b=i;}return b;}
static int sched_rr(void){
    int s=(running_idx>=0)?(running_idx+1)%proc_cnt:0;
    for(int o=0;o<proc_cnt;o++){int i=(s+o)%proc_cnt;if(procs[i].state==READY&&procs[i].mem_ok)return i;}return -1;}

static void do_tick(void){
    time_tick++; tick_flash=1.0f;
    for(int i=0;i<proc_cnt;i++)if(procs[i].state==READY)procs[i].waiting_time++;
    for(int i=0;i<proc_cnt;i++)if(procs[i].state==NEW&&procs[i].mem_ok)procs[i].state=READY;
    if(running_idx==-1||procs[running_idx].state!=RUNNING){
        int next=-1;
        switch(cur_algo){case FCFS:next=sched_fcfs();break;case SJF:next=sched_sjf();break;
                          case PRIO:next=sched_prio();break;case RR:next=sched_rr();break;}
        if(next>=0){
            procs[next].state=RUNNING; running_idx=next; rr_ctr=0;
            char buf[LOG_LINE];
            snprintf(buf,LOG_LINE,"[>] Tick %d: Dispatched PID %d (%s) via %s",
                     time_tick,procs[next].pid,procs[next].name,astr(cur_algo));
            wlog(buf);
        }
    }
    if(running_idx>=0&&procs[running_idx].state==RUNNING){
        procs[running_idx].remaining--; rr_ctr++;
        if(cur_algo==RR&&rr_ctr>=RR_QUANTUM&&procs[running_idx].remaining>0){
            procs[running_idx].state=READY;
            char buf[LOG_LINE];
            snprintf(buf,LOG_LINE,"[~] PID %d preempted (RR quantum=%d)",procs[running_idx].pid,RR_QUANTUM);
            wlog(buf); running_idx=-1; rr_ctr=0;
        }
        if(running_idx>=0&&procs[running_idx].remaining<=0){proc_terminate(running_idx);running_idx=-1;}
    }
    for(int i=0;i<proc_cnt;i++)
        if(procs[i].state==WAITING&&!procs[i].mem_ok){
            int s=mem_alloc(procs[i].pid,procs[i].mem_needed);
            if(s>=0){procs[i].mem_start=s;procs[i].mem_ok=1;procs[i].state=READY;
                wlog("[M] Deferred alloc OK — process promoted to READY");}
        }
}

/* ═══════════════ SIDEBAR ═════════════════════════════════════════ */
static void draw_sidebar(void){
    rr(0,0,SB_W,WIN_H,0,CSB);
    DrawLine(SB_W,0,SB_W,WIN_H,CBORD);
    dt("SERC",14,14,26,CACC);
    dt("Mini-OS  v2.0",14,42,12,CDIM);
    DrawLine(10,62,SB_W-10,62,CBORD);

    const char*labs[TAB_N]={"Dashboard","Processes","Memory","IPC","Deadlock","Log"};
    for(int i=0;i<TAB_N;i++){
        float ty=72+i*50; Vector2 mp=GetMousePosition();
        int hov=(mp.x>=8&&mp.x<=SB_W-8&&mp.y>=ty&&mp.y<=ty+38);
        if(hov&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON))cur_tab=(Tab)i;
        if(cur_tab==i){rr(8,ty,SB_W-16,38,0.25f,CACC);dt(labs[i],20,ty+12,14,WHITE);}
        else{if(hov)rr(8,ty,SB_W-16,38,0.25f,(Color){35,42,62,255});dt(labs[i],20,ty+12,14,hov?CTEXT:CDIM);}
    }
    DrawLine(10,380,SB_W-10,380,CBORD);
    dt("SCHEDULER",14,388,11,CDIM);
    const char*an[]={"FCFS","SJF","Priority","Round Robin"};
    SchedAlgo av[]={FCFS,SJF,PRIO,RR};
    for(int i=0;i<4;i++){
        float by=404+i*30; Vector2 mp=GetMousePosition();
        int hov=(mp.x>=10&&mp.x<=SB_W-10&&mp.y>=by&&mp.y<=by+24);
        if(hov&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON))cur_algo=av[i];
        Color bg=cur_algo==av[i]?(Color){50,80,180,255}:hov?(Color){30,36,56,255}:(Color){0,0,0,0};
        if(bg.a>0)rr(10,by,SB_W-20,24,0.3f,bg);
        dt(an[i],20,by+5,13,cur_algo==av[i]?WHITE:CDIM);
    }
    DrawLine(10,530,SB_W-10,530,CBORD);

    /* ── AUTO RUN ── */
    dt("EXECUTION",14,538,11,CDIM);

    /* Auto toggle button */
    float aby2=556;
    Color abc2=auto_run?(Color){200,60,60,255}:(Color){40,160,100,255};
    Vector2 mp=GetMousePosition();
    int hov_auto=(mp.x>=10&&mp.x<=SB_W-10&&mp.y>=aby2&&mp.y<=aby2+36);
    if(hov_auto) abc2.r+=20,abc2.g+=20,abc2.b+=20;
    rr(10,aby2,SB_W-20,36,0.3f,abc2);
    const char*atxt=auto_run?"⏸  AUTO: ON":"▶▶ AUTO: OFF";
    float atw=tw(atxt,13); dt(atxt,10+(SB_W-20-atw)/2,aby2+11,13,WHITE);
    if(hov_auto&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
        auto_run=!auto_run; auto_accum=0;
        wlog(auto_run?"[*] Auto-run STARTED":"[*] Auto-run PAUSED");
    }

    /* Speed selector */
    dt("Speed (ticks/sec)",14,600,11,CDIM);
    float speeds[]={0.5f,1.0f,2.0f,4.0f};
    const char*slabs[]={"0.5x","1x","2x","4x"};
    float sbw=(SB_W-24)/4.0f;
    for(int i=0;i<4;i++){
        float bx=10+i*sbw,by=614;
        int sel=(auto_speed==speeds[i]);
        int shov=(mp.x>=bx&&mp.x<=bx+sbw-2&&mp.y>=by&&mp.y<=by+22);
        Color bc=sel?(Color){60,100,220,255}:shov?(Color){35,42,62,255}:(Color){25,30,48,255};
        rr(bx,by,sbw-2,22,0.3f,bc);
        float tw2=tw(slabs[i],12);
        dt(slabs[i],bx+(sbw-2-tw2)/2,by+4,12,sel?WHITE:CDIM);
        if(shov&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON))auto_speed=speeds[i];
    }

    /* Progress ring when auto-running */
    if(auto_run){
        float cx2=SB_W/2.0f, cy2=648;
        float pct=auto_accum*auto_speed;
        DrawCircleLines((int)cx2,(int)cy2,8,CBORD);
        /* arc approximation with segments */
        for(int s=0;s<(int)(pct*20);s++){
            float a=-3.14159f/2+s*(2*3.14159f/20);
            DrawLine(cx2,cy2,cx2+8*cosf(a),cy2+8*sinf(a),CGREEN);
        }
    }

    DrawLine(10,660,SB_W-10,660,CBORD);

    /* Manual tick button */
    float tby=WIN_H-STB_H-58;
    Color tc=tick_flash>0?(Color){80,230,140,255}:(Color){30,38,58,255};
    int hov_tick=(mp.x>=10&&mp.x<=SB_W-10&&mp.y>=tby&&mp.y<=tby+42);
    if(hov_tick) tc=(Color){45,55,82,255};
    rr(10,tby,SB_W-20,42,0.3f,tc);
    float tw3=tw("▶  TICK  (+1)",15);
    dt("▶  TICK  (+1)",10+(SB_W-20-tw3)/2,tby+14,15,hov_tick?WHITE:CDIM);
    if(hov_tick&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON))do_tick();
    char tc3[24];snprintf(tc3,24,"Tick: %d",time_tick);
    float tw4=tw(tc3,12); dt(tc3,10+(SB_W-20-tw4)/2,tby+46,12,CDIM);
}

/* ═══════════════ DASHBOARD ═══════════════════════════════════════ */
static void draw_dashboard(void){
    float x0=SB_W+16,y0=16,pw=WIN_W-SB_W-32;
    dt("Dashboard",x0,y0,22,CTEXT);
    dt("Smart Emergency Response Center — System Overview",x0,y0+26,13,CDIM);
    int active=0,rdy=0,wait=0,done=0;
    for(int i=0;i<proc_cnt;i++){
        if(procs[i].state==RUNNING||procs[i].state==NEW)active++;
        else if(procs[i].state==READY)rdy++;
        else if(procs[i].state==WAITING)wait++;
        else if(procs[i].state==TERMINATED)done++;
    }
    struct{const char*l;int v;Color c;const char*s;}cards[]={
        {"Active",active,CGREEN,"running now"},{"Ready",rdy,CYEL,"in queue"},
        {"Waiting",wait,CORANG,"for memory"},{"Completed",done,CACC,"total done"}};
    float cw=194,ch=86;
    for(int i=0;i<4;i++){
        float cx=x0+i*(cw+10),cy=y0+56;
        DrawRectangleRounded((Rectangle){cx+3,cy+3,cw,ch},0.1f,8,(Color){0,0,0,60});
        rr(cx,cy,cw,ch,0.1f,CCARD);
        DrawRectangleRounded((Rectangle){cx,cy,4,ch},0.5f,4,cards[i].c);
        dt(cards[i].l,cx+16,cy+10,13,CDIM);
        char v[8];snprintf(v,8,"%d",cards[i].v);
        dt(v,cx+16,cy+28,34,cards[i].c);
        dt(cards[i].s,cx+16,cy+68,11,CDIM);
    }
    /* Memory bar */
    float mx=x0,my=y0+56+ch+14,mw2=pw;
    rr(mx,my,mw2,44,0.1f,CCARD);
    dt("Memory Usage",mx+12,my+6,12,CDIM);
    float upct=(float)mem_used()/MAX_MEM;
    Color mc=upct>0.85f?CRED:upct>0.6f?CORANG:CGREEN;
    rr(mx+12,my+22,upct*(mw2-24),16,0.4f,mc);
    rrl(mx+12,my+22,mw2-24,16,0.4f,CBORD);
    char mb[80];snprintf(mb,80,"%dMB / %dMB (%d%% | %d fragment%s)",
        mem_used(),MAX_MEM,(int)(upct*100),mem_frags(),mem_frags()==1?"":"s");
    dm(mb,mx+12+((mw2-24)-mw(mb,11))/2,my+25,11,WHITE);
    /* Table */
    float ty=my+58; dt("Process Table",x0,ty,16,CTEXT); ty+=22;
    rr(x0,ty,mw2,22,0.1f,(Color){20,24,38,255});
    const char*hh[]={"PID","Name","State","Priority","Burst","Rem","Wait","TAT","Mem"};
    float hx[]={6,40,150,244,328,386,440,494,548};
    for(int i=0;i<9;i++)dm(hh[i],x0+hx[i],ty+5,11,CDIM);
    ty+=24;
    for(int i=0;i<proc_cnt;i++){
        PCB*p=&procs[i];
        Color rb=p->state==RUNNING?(Color){18,42,28,255}:i%2==0?CCARD:CCARD2;
        rr(x0,ty+i*24,mw2,23,0.05f,rb);
        char pid[8];snprintf(pid,8,"%d",p->pid);
        dm(pid,x0+hx[0],ty+i*24+5,12,CTEXT);
        dm(p->name,x0+hx[1],ty+i*24+5,12,CTEXT);
        badge(x0+hx[2],ty+i*24+3,sstr(p->state),state_col(p->state));
        badge(x0+hx[3],ty+i*24+3,pstr(p->priority),prio_col(p->priority));
        char n[80];snprintf(n,80,"%-6d %-10d %-8d %-8d %dMB",
            p->burst,p->remaining,p->waiting_time,p->turnaround,p->mem_needed);
        dm(n,x0+hx[4],ty+i*24+5,12,CTEXT);
    }
    /* Metrics */
    int twt=0,ttat=0,cnt=0;
    for(int i=0;i<proc_cnt;i++)if(procs[i].state==TERMINATED){twt+=procs[i].waiting_time;ttat+=procs[i].turnaround;cnt++;}
    float sy=ty+proc_cnt*24+10; rr(x0,sy,mw2,36,0.1f,CCARD);
    char ms[200];
    snprintf(ms,200,"Scheduler: %s  |  Avg Wait: %.1f  |  Avg TAT: %.1f  |  CPU: %s  |  RR Quantum: %d  |  Done: %d",
        astr(cur_algo),cnt?(float)twt/cnt:0,cnt?(float)ttat/cnt:0,
        running_idx>=0?"BUSY":"IDLE",RR_QUANTUM,cnt);
    dt(ms,x0+12,sy+10,12,CDIM);
}

/* ═══════════════ PROCESSES ═══════════════════════════════════════ */
static void draw_processes(void){
    float x0=SB_W+16,y0=16,pw=WIN_W-SB_W-32;
    dt("Process Management",x0,y0,22,CTEXT);
    dt("Create emergency tasks and watch them execute",x0,y0+26,13,CDIM);
    /* Add button */
    float abx=WIN_W-186,aby=y0; Vector2 mp=GetMousePosition();
    Color abc=mp.x>=abx&&mp.x<=abx+166&&mp.y>=aby&&mp.y<=aby+34?(Color){80,220,130,255}:(Color){60,200,110,255};
    if(abc.r==80&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){show_add=1;add_anim=0;memset(dlg_name,0,36);dlg_prio=1;dlg_burst=6;dlg_mem=16;dlg_field=0;}
    rr(abx,aby,166,34,0.3f,abc);
    dt("+ Add Task",abx+34,aby+9,15,WHITE);
    /* Cards */
    float cw=(pw-20)/3.0f,ch=140,cy0=y0+58;
    for(int i=0;i<proc_cnt;i++){
        PCB*p=&procs[i];
        float ty=cy0+(i/3)*(ch+10);
        p->anim_y+=(ty-p->anim_y)*0.18f;
        float cx=x0+(i%3)*(cw+10),cy=p->anim_y;
        DrawRectangleRounded((Rectangle){cx+3,cy+3,cw,ch},0.1f,8,(Color){0,0,0,70});
        rr(cx,cy,cw,ch,0.1f,CCARD);
        DrawRectangleRounded((Rectangle){cx,cy,5,ch},0.2f,4,prio_col(p->priority));
        if(p->state==RUNNING){
            float pulse=0.5f+0.5f*sinf((float)GetTime()*4);
            DrawRectangleRounded((Rectangle){cx,cy,cw,ch},0.1f,8,(Color){60,200,120,(unsigned char)(pulse*30)});
            dm("● EXECUTING",cx+cw-100,cy+10,11,CGREEN);
        }
        char ttl[40];snprintf(ttl,40,"%.20s",p->name);
        dt(ttl,cx+14,cy+10,14,CTEXT);
        char pid[16];snprintf(pid,16,"PID %d",p->pid);
        dm(pid,cx+14,cy+28,11,CDIM);
        badge(cx+14,cy+46,sstr(p->state),state_col(p->state));
        badge(cx+90,cy+46,pstr(p->priority),prio_col(p->priority));
        float done_pct=p->burst>0?1.0f-(float)p->remaining/p->burst:1.0f;
        p->bar_anim+=(done_pct-p->bar_anim)*0.1f;
        Color bc=p->state==TERMINATED?CDIM:p->state==RUNNING?CGREEN:p->state==WAITING?CORANG:CACC;
        float bw=cw-28;
        rr(cx+14,cy+72,bw,10,0.5f,(Color){25,30,48,255});
        if(p->bar_anim>0.01f)rr(cx+14,cy+72,bw*p->bar_anim,10,0.5f,bc);
        char info[64];snprintf(info,64,"CPU: %d/%d ticks  |  Mem: %dMB",p->burst-p->remaining,p->burst,p->mem_needed);
        dm(info,cx+14,cy+90,11,CDIM);
        char info2[48];snprintf(info2,48,"Wait: %d ticks  |  TAT: %d",p->waiting_time,p->turnaround);
        dm(info2,cx+14,cy+104,11,CDIM);
        if(p->state!=TERMINATED){
            float tbx=cx+cw-82,tby=cy+ch-30;
            Color tbc=mp.x>=tbx&&mp.x<=tbx+72&&mp.y>=tby&&mp.y<=tby+22?CRED:(Color){180,50,50,255};
            rr(tbx,tby,72,22,0.4f,tbc);
            dt("Terminate",tbx+5,tby+5,11,WHITE);
            if(tbc.r==255&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON))proc_terminate(i);
        } else { dm("COMPLETED",cx+cw-90,cy+ch-24,11,CDIM); }
    }
    if(proc_cnt==0){
        dt("No processes yet. Click '+ Add Task' to create an emergency task.",x0+20,y0+100,14,CDIM);}
}

/* ═══════════════ MEMORY ══════════════════════════════════════════ */
static void draw_memory(void){
    float x0=SB_W+16,y0=16,pw=WIN_W-SB_W-32;
    dt("Memory Management",x0,y0,22,CTEXT);
    dt("First-Fit allocation — real-time fragmentation tracking",x0,y0+26,13,CDIM);
    rr(x0,y0+48,pw,36,0.1f,CCARD);
    char ms[200];snprintf(ms,200,"Total: %dMB   Used: %dMB   Free: %dMB   Fragmentation: %d holes   Blocks: %d   Utilization: %d%%",
        MAX_MEM,mem_used(),MAX_MEM-mem_used(),mem_frags(),mblk_cnt,(int)((float)mem_used()/MAX_MEM*100));
    dt(ms,x0+12,y0+60,12,CDIM);
    /* Visual map */
    float mx=x0,my=y0+94;
    rr(mx,my,pw,72,0.08f,(Color){16,20,32,255});
    dt("Memory Address Space  [0 MB ──────────────────────────── 256 MB]",mx+8,my+4,11,CDIM);
    Color blk_cols[]={(Color){82,130,255,255},(Color){60,210,120,255},(Color){255,165,50,255},
                      (Color){255,75,75,255},(Color){180,100,240,255},(Color){255,220,60,255},
                      (Color){100,200,200,255},(Color){255,150,180,255}};
    for(int i=0;i<mblk_cnt;i++){
        float bx=mx+(float)mblk[i].start/MAX_MEM*pw;
        float bw2=(float)mblk[i].size/MAX_MEM*pw;
        if(bw2<1)continue;
        Color bc=mblk[i].free?(Color){35,42,60,255}:blk_cols[mblk[i].owner%8];
        DrawRectangleRounded((Rectangle){bx+1,my+22,bw2-2,44},0.05f,4,bc);
        if(bw2>24){char lbl[12];if(mblk[i].free)snprintf(lbl,12,"FREE");else snprintf(lbl,12,"P%d",mblk[i].owner);dm(lbl,(int)(bx+4),my+38,10,WHITE);}
    }
    float ty=my+80;
    rr(x0,ty,pw,20,0.1f,(Color){20,24,38,255});
    const char*hh[]={"#","Start","Size","Status","Owner PID + Name","% RAM"};
    float hx[]={6,55,145,230,325,510};
    for(int i=0;i<6;i++)dm(hh[i],x0+hx[i],ty+4,11,CDIM);
    ty+=22;
    for(int i=0;i<mblk_cnt;i++){
        Color rb=i%2==0?CCARD:CCARD2;
        rr(x0,ty+i*22,pw,21,0.05f,rb);
        char row[64];snprintf(row,64,"%-4d %-12d %dMB",i,mblk[i].start,mblk[i].size);
        dm(row,x0+hx[0],ty+i*22+4,12,CTEXT);
        if(mblk[i].free){badge(x0+hx[3],ty+i*22+2,"FREE",(Color){25,65,40,255});dm("—",x0+hx[4],ty+i*22+4,12,CDIM);}
        else{badge(x0+hx[3],ty+i*22+2,"USED",(Color){70,30,30,255});
            /* find name */
            const char*nn="?";
            for(int j=0;j<proc_cnt;j++)if(procs[j].pid==mblk[i].owner){nn=procs[j].name;break;}
            char ow[48];snprintf(ow,48,"PID %d — %s",mblk[i].owner,nn);
            dm(ow,x0+hx[4],ty+i*22+4,12,CACC);}
        char pcts[12];snprintf(pcts,12,"%.1f%%",(float)mblk[i].size/MAX_MEM*100);
        dm(pcts,x0+hx[5],ty+i*22+4,12,CDIM);
    }
}

/* ═══════════════ IPC ═════════════════════════════════════════════ */
static void draw_ipc(void){
    float x0=SB_W+16,y0=16,pw=WIN_W-SB_W-32;
    dt("Inter-Process Communication (IPC)",x0,y0,22,CTEXT);
    dt("Pipe-style message queue between processes",x0,y0+26,13,CDIM);
    rr(x0,y0+50,pw,52,0.1f,CCARD);
    DrawRectangleRounded((Rectangle){x0,y0+50,5,52},0.5f,4,CACC);
    dt("IPC Mechanism: Shared Message Queue (Pipe Simulation)",x0+14,y0+58,13,CACC);
    dt("When a task is created, it sends a TASK_READY signal to the first active process.",x0+14,y0+74,12,CDIM);
    dt("This simulates pipe-based inter-process signalling between OS tasks.",x0+14,y0+88,12,CDIM);
    char qs[64];snprintf(qs,64,"Queue depth: %d / %d messages",ipc_cnt,IPC_Q_SZ);
    dt(qs,x0,y0+118,13,CTEXT);
    float ty=y0+140;
    rr(x0,ty,pw,20,0.1f,(Color){20,24,38,255});
    dm("Tick",x0+8,ty+4,11,CDIM); dm("From PID",x0+70,ty+4,11,CDIM);
    dm("To PID",x0+150,ty+4,11,CDIM); dm("Message",x0+230,ty+4,11,CDIM);
    ty+=22;
    if(ipc_cnt==0){dt("No IPC messages yet. Add tasks to generate traffic.",x0+16,ty+14,13,CDIM);return;}
    for(int i=0;i<ipc_cnt&&i<20;i++){
        int idx=(ipc_head+i)%IPC_Q_SZ;
        Color rb=i%2==0?CCARD:CCARD2;
        rr(x0,ty+i*24,pw,23,0.05f,rb);
        char tck[10];snprintf(tck,10,"T%d",ipc_q[idx].tick_sent);
        dm(tck,x0+8,ty+i*24+5,12,CDIM);
        char fr[12];snprintf(fr,12,"PID %d",ipc_q[idx].from_pid);
        dm(fr,x0+70,ty+i*24+5,12,CORANG);
        char to[12];snprintf(to,12,"PID %d",ipc_q[idx].to_pid);
        dm(to,x0+150,ty+i*24+5,12,CACC);
        dm(ipc_q[idx].msg,x0+230,ty+i*24+5,12,CTEXT);
    }
}

/* ═══════════════ DEADLOCK ════════════════════════════════════════ */
static void draw_deadlock(void){
    float x0=SB_W+16,y0=16,pw=WIN_W-SB_W-32;
    dt("Deadlock Detection — Banker's Algorithm",x0,y0,22,CTEXT);
    dt("Resource allocation state and safety sequence analysis",x0,y0+26,13,CDIM);
    int safe=banker_safe();
    Color sc=safe?CGREEN:CRED;
    rr(x0,y0+50,pw,44,0.1f,CCARD);
    DrawRectangleRounded((Rectangle){x0,y0+50,5,44},0.5f,4,sc);
    dt(safe?"✓  SAFE STATE — No deadlock risk detected":"✗  UNSAFE STATE — Potential deadlock!",x0+18,y0+58,15,sc);
    dt(safe?"A safe execution sequence exists for all current processes.":"No safe sequence found. Consider releasing resources.",x0+18,y0+76,12,CDIM);
    float ry=y0+106; dt("System Resources",x0,ry,15,CTEXT); ry+=20;
    rr(x0,ry,pw,20,0.1f,(Color){20,24,38,255});
    dm("Resource",x0+8,ry+4,11,CDIM); dm("Total",x0+180,ry+4,11,CDIM);
    dm("Available",x0+260,ry+4,11,CDIM); dm("In Use",x0+360,ry+4,11,CDIM);
    dm("Usage",x0+450,ry+4,11,CDIM); ry+=22;
    for(int r=0;r<MAX_RES;r++){
        int used2=res_total[r]-res_avail[r];
        rr(x0,ry+r*26,pw,25,0.05f,r%2==0?CCARD:CCARD2);
        dm(res_names[r],x0+8,ry+r*26+6,12,CTEXT);
        dm(TextFormat("%d",res_total[r]),x0+180,ry+r*26+6,12,CTEXT);
        dm(TextFormat("%d",res_avail[r]),x0+260,ry+r*26+6,12,CGREEN);
        dm(TextFormat("%d",used2),x0+360,ry+r*26+6,12,used2>0?CORANG:CDIM);
        rr(x0+450,ry+r*26+6,200,13,0.5f,(Color){25,30,48,255});
        float upct=res_total[r]>0?(float)used2/res_total[r]:0;
        if(upct>0)rr(x0+450,ry+r*26+6,200*upct,13,0.5f,upct>0.8f?CRED:CACC);
    }
    float py=ry+MAX_RES*26+18; dt("Allocation Matrix  (Held / Maximum Need)",x0,py,14,CTEXT); py+=20;
    rr(x0,py,pw,20,0.1f,(Color){20,24,38,255});
    dm("PID  Name",x0+8,py+4,11,CDIM);
    for(int r=0;r<MAX_RES;r++) dm(res_names[r],x0+220+r*150,py+4,11,CDIM);
    py+=22;
    for(int i=0;i<proc_cnt;i++){
        if(procs[i].state==TERMINATED)continue;
        rr(x0,py,pw,23,0.05f,i%2==0?CCARD:CCARD2);
        char line[32];snprintf(line,32,"%-4d %-18s",procs[i].pid,procs[i].name);
        dm(line,x0+8,py+5,12,CTEXT);
        for(int r=0;r<MAX_RES;r++){
            int need=procs[i].res_max[r]-procs[i].res[r];
            dm(TextFormat("%d/%d (need %d)",procs[i].res[r],procs[i].res_max[r],need),
               x0+220+r*150,py+5,11,CACC);
        }
        py+=24;
    }
    if(proc_cnt==0)dt("No active processes. Add tasks first.",x0+16,py+10,13,CDIM);
}

/* ═══════════════ LOG ═════════════════════════════════════════════ */
static void draw_log(void){
    float x0=SB_W+16,y0=16,pw=WIN_W-SB_W-32;
    dt("System Log",x0,y0,22,CTEXT);
    char sub[80];snprintf(sub,80,"%d events — live-written to serc_log.txt",log_cnt);
    dt(sub,x0,y0+26,13,CDIM);
    Vector2 mp=GetMousePosition();
    rr(WIN_W-152,y0,132,28,0.3f,mp.x>=WIN_W-152&&mp.y>=y0&&mp.y<=y0+28?(Color){70,80,110,255}:(Color){45,52,75,255});
    dt("Clear Log",WIN_W-130,y0+7,13,CDIM);
    if(mp.x>=WIN_W-152&&mp.y>=y0&&mp.y<=y0+28&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){log_cnt=0;log_scroll=0;}
    log_scroll-=GetMouseWheelMove()*2;
    if(log_scroll<0)log_scroll=0;
    if(log_scroll>log_cnt)log_scroll=(float)log_cnt;
    float ty=y0+52,lh=20;
    int vis=(int)((WIN_H-STB_H-ty)/lh);
    for(int i=(int)log_scroll;i<log_cnt&&i<(int)log_scroll+vis;i++){
        float ry=ty+(i-(int)log_scroll)*lh;
        Color lc=CTEXT,rb=(Color){0,0,0,0};
        if(strncmp(log_buf[i],"[!]",3)==0){lc=CRED;rb=(Color){40,12,12,255};}
        else if(strncmp(log_buf[i],"[+]",3)==0){lc=CGREEN;rb=(Color){12,32,18,255};}
        else if(strncmp(log_buf[i],"[>]",3)==0){lc=CACC;rb=(Color){12,18,45,255};}
        else if(strncmp(log_buf[i],"[-]",3)==0)lc=CDIM;
        else if(strncmp(log_buf[i],"[~]",3)==0)lc=CYEL;
        else if(strncmp(log_buf[i],"[W]",3)==0){lc=CORANG;rb=(Color){38,25,10,255};}
        else if(strncmp(log_buf[i],"[M]",3)==0)lc=CYEL;
        else if(strncmp(log_buf[i],"[IPC]",5)==0){lc=CPINK;rb=(Color){35,12,35,255};}
        if(rb.a>0)rr(x0,ry,pw,lh-1,0.04f,rb);
        dm(log_buf[i],x0+8,ry+3,12,lc);
    }
}

/* ═══════════════ ADD DIALOG ══════════════════════════════════════ */
static void draw_add_dialog(void){
    add_anim+=(1.0f-add_anim)*0.16f;
    DrawRectangle(0,0,WIN_W,WIN_H,(Color){0,0,0,(unsigned char)(190*add_anim)});
    float dw=490,dh=408,dx=WIN_W/2-dw/2,dy=WIN_H/2-dh/2*(0.7f+0.3f*add_anim);
    DrawRectangleRounded((Rectangle){dx+6,dy+6,dw,dh},0.08f,8,(Color){0,0,0,100});
    rr(dx,dy,dw,dh,0.08f,CPANEL);
    DrawRectangleRounded((Rectangle){dx,dy,dw,4},0.5f,4,CACC);
    DrawLine(dx,dy+58,dx+dw,dy+58,CBORD);
    dt("Create Emergency Task",dx+20,dy+16,18,CTEXT);
    dt("Fill in task details — tab between fields — enter to confirm",dx+20,dy+38,12,CDIM);
    /* Name */
    dt("Task Name",dx+20,dy+68,13,CDIM);
    rr(dx+20,dy+84,dw-40,36,0.15f,(Color){14,18,30,255});
    rrl(dx+20,dy+84,dw-40,36,0.15f,dlg_field==0?CACC:CBORD);
    dt(strlen(dlg_name)>0?dlg_name:"e.g. Ambulance-01, Fire-Alert, Police-K9 …",dx+32,dy+95,14,strlen(dlg_name)>0?CTEXT:CDIM);
    if(dlg_field==0&&(int)(GetTime()*2)%2==0)
        DrawRectangle(dx+32+(int)tw(dlg_name,14),dy+95,2,16,CACC);
    /* Priority */
    dt("Priority",dx+20,dy+136,13,CDIM);
    const char*pn[]={"CRITICAL","HIGH","NORMAL"};
    Color pc[]={CRED,CORANG,CGREEN};
    Vector2 mp=GetMousePosition();
    for(int i=0;i<3;i++){
        float bx=dx+20+i*152,by=dy+152;
        rr(bx,by,142,32,0.3f,dlg_prio==i+1?pc[i]:(Color){28,34,52,255});
        float w2=tw(pn[i],13); dt(pn[i],bx+(142-w2)/2,by+9,13,dlg_prio==i+1?WHITE:CDIM);
        if(mp.x>=bx&&mp.x<=bx+142&&mp.y>=by&&mp.y<=by+32&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON))dlg_prio=i+1;
    }
    /* Burst */
    dt("Burst Time (ticks)",dx+20,dy+202,13,CDIM);
    rr(dx+20,dy+218,210,36,0.15f,(Color){14,18,30,255});
    rrl(dx+20,dy+218,210,36,0.15f,dlg_field==1?CACC:CBORD);
    char bt[20];snprintf(bt,20,"%d ticks",dlg_burst);
    dt(bt,dx+32,dy+229,14,CTEXT);
    /* Mem */
    dt("Memory (MB)",dx+260,dy+202,13,CDIM);
    rr(dx+260,dy+218,210,36,0.15f,(Color){14,18,30,255});
    rrl(dx+260,dy+218,210,36,0.15f,dlg_field==2?CACC:CBORD);
    char mem[16];snprintf(mem,16,"%d MB",dlg_mem);
    dt(mem,dx+272,dy+229,14,CTEXT);
    /* Field focus via click */
    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
        if(mp.x>=dx+20&&mp.x<=dx+dw-20&&mp.y>=dy+84&&mp.y<=dy+120)dlg_field=0;
        if(mp.x>=dx+20&&mp.x<=dx+230&&mp.y>=dy+218&&mp.y<=dy+254)dlg_field=1;
        if(mp.x>=dx+260&&mp.x<=dx+470&&mp.y>=dy+218&&mp.y<=dy+254)dlg_field=2;
    }
    /* Keyboard */
    int key=GetCharPressed();
    while(key>0){
        if(dlg_field==0){int l=strlen(dlg_name);if(key>=32&&key<=126&&l<34){dlg_name[l]=(char)key;dlg_name[l+1]=0;}}
        else if(dlg_field==1){if(key>='0'&&key<='9'){dlg_burst=dlg_burst*10+(key-'0');if(dlg_burst>99)dlg_burst=99;}}
        else{if(key>='0'&&key<='9'){dlg_mem=dlg_mem*10+(key-'0');if(dlg_mem>MAX_MEM)dlg_mem=MAX_MEM;}}
        key=GetCharPressed();
    }
    if(IsKeyPressed(KEY_BACKSPACE)){
        if(dlg_field==0){int l=strlen(dlg_name);if(l>0)dlg_name[l-1]=0;}
        else if(dlg_field==1)dlg_burst/=10;
        else dlg_mem/=10;
    }
    if(IsKeyPressed(KEY_TAB))dlg_field=(dlg_field+1)%3;
    /* Buttons */
    float cbx=dx+20,cby=dy+dh-56;
    Color okc=mp.x>=cbx&&mp.x<=cbx+210&&mp.y>=cby&&mp.y<=cby+38?(Color){110,155,255,255}:CACC;
    rr(cbx,cby,210,38,0.3f,okc);
    dt("Create Task",cbx+52,cby+12,15,WHITE);
    rr(cbx+230,cby,210,38,0.3f,(Color){32,38,58,255});
    dt("Cancel",cbx+290,cby+12,15,CDIM);
    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
        if(mp.x>=cbx&&mp.x<=cbx+210&&mp.y>=cby&&mp.y<=cby+38){
            if(!strlen(dlg_name))strcpy(dlg_name,"Emergency");
            if(dlg_burst<1){dlg_burst=1;} if(dlg_mem<4){dlg_mem=4;}
            proc_create(dlg_name,dlg_prio,dlg_burst,dlg_mem);
            cur_tab=TAB_PROC; show_add=0;
        }
        if(mp.x>=cbx+230&&mp.x<=cbx+440&&mp.y>=cby&&mp.y<=cby+38)show_add=0;
    }
    if(IsKeyPressed(KEY_ESCAPE))show_add=0;
    if(IsKeyPressed(KEY_ENTER)&&dlg_field!=0){
        if(!strlen(dlg_name))strcpy(dlg_name,"Emergency");
        if(dlg_burst<1){dlg_burst=1;} if(dlg_mem<4){dlg_mem=4;}
        proc_create(dlg_name,dlg_prio,dlg_burst,dlg_mem);
        cur_tab=TAB_PROC; show_add=0;
    }
}

/* ═══════════════ STATUS BAR ══════════════════════════════════════ */
static void draw_status(void){
    DrawRectangle(SB_W,WIN_H-STB_H,WIN_W-SB_W,STB_H,(Color){14,18,30,255});
    DrawLine(SB_W,WIN_H-STB_H,WIN_W,WIN_H-STB_H,CBORD);
    char s[256];
    snprintf(s,256,"Tick: %d  |  Algo: %s  |  Procs: %d  |  Mem: %dMB/%dMB  |  Deadlock: %s  |  IPC: %d msgs  |  Log: %d events  |  CS225 — CBU",
        time_tick,astr(cur_algo),proc_cnt,mem_used(),MAX_MEM,banker_safe()?"SAFE":"UNSAFE!",ipc_cnt,log_cnt);
    dm(s,SB_W+12,WIN_H-STB_H+7,11,CDIM);
}

/* ═══════════════ MAIN ════════════════════════════════════════════ */
int main(void){
    srand((unsigned)time(NULL));
    mem_init();
    /* Clear log file — fresh session */
    FILE*f=fopen("serc_log.txt","w");
    if(f){fprintf(f,"=== SERC Mini-OS — CS225 Copperbelt University ===\n");fclose(f);}

    SetConfigFlags(FLAG_MSAA_4X_HINT|FLAG_WINDOW_HIGHDPI);
    InitWindow(WIN_W,WIN_H,"SERC Mini-OS  |  CS 225  |  Copperbelt University  |  Dr Derrick Ntalasha");
    SetTargetFPS(60);

    /* Smooth font loading with antialiasing */
    fnt =LoadFontEx("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",32,NULL,256);
    fmono=LoadFontEx("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",24,NULL,256);
    if(fnt.texture.id==0) fnt=GetFontDefault();
    if(fmono.texture.id==0)fmono=GetFontDefault();
    SetTextureFilter(fnt.texture,TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(fmono.texture,TEXTURE_FILTER_BILINEAR);

    wlog("[*] SERC Mini-OS started — volatile session (restart = clean slate)");
    wlog("[*] Memory: 256MB | Resources: Comms(3) Vehicles(4) Radios(3) Officers(2)");
    wlog("[*] Components: Process Mgmt | CPU Scheduling | Memory | IPC | Deadlock | File Log");

    while(!WindowShouldClose()){
        float dt2=GetFrameTime();
        tick_flash-=dt2*3.0f;
        if(tick_flash<0)tick_flash=0;

        /* Auto-run: fire a tick every 1/speed seconds */
        if(auto_run){
            auto_accum+=dt2;
            if(auto_accum>=1.0f/auto_speed){
                auto_accum=0;
                do_tick();
            }
        }
        BeginDrawing();
        ClearBackground(CBG);
        draw_sidebar();
        BeginScissorMode(SB_W,0,WIN_W-SB_W,WIN_H-STB_H);
        switch(cur_tab){
            case TAB_DASH: draw_dashboard(); break;
            case TAB_PROC: draw_processes(); break;
            case TAB_MEM:  draw_memory();    break;
            case TAB_IPC:  draw_ipc();       break;
            case TAB_DEAD: draw_deadlock();  break;
            case TAB_LOG:  draw_log();       break;
            default: break;
        }
        EndScissorMode();
        if(show_add)draw_add_dialog();
        draw_status();
        EndDrawing();
    }
    CloseWindow();
    wlog("[*] Session ended cleanly");
    return 0;
}
