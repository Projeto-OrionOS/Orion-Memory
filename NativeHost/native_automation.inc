/* A one-second, allocation-free automatic cleanup policy. All decisions use a
   monotonic clock; only the native action dispatcher can start actual work. */
#define NA_PRESSURE_MS 20000ull
#define NA_HYSTERESIS 3.0
#define NA_MINUTE_MS 60000ull
typedef struct {
    int threshold_enabled,threshold_percent,interval_enabled,interval_minutes;
    int allow_aggressive,cooldown_minutes;
    DWORD selected_mask;
} NAConfig;
typedef struct {
    NAConfig config;
    ULONGLONG interval_anchor,last_completion,pressure_start,last_attempt;
    int initialized,has_completion,has_pressure,armed;
} NAState;
enum { NA_NONE,NA_THRESHOLD,NA_INTERVAL };
static NAState na_state;

static DWORD na_effective_mask(DWORD selected,int allow_aggressive){
    /* A conservative run only discards the low-priority standby cache. It
       never trims processes, flushes volumes or empties the full standby list. */
    return allow_aggressive?(selected&511u):(1u<<4);
}
static NAConfig na_current_config(void){
    NAConfig config;memset(&config,0,sizeof(config));
    config.threshold_enabled=g_settings.auto_threshold_enabled;
    config.threshold_percent=clampi(g_settings.auto_threshold_percent,1,99);
    config.interval_enabled=g_settings.auto_interval_enabled;
    config.interval_minutes=clampi(g_settings.auto_interval_minutes,1,240);
    config.allow_aggressive=g_settings.allow_aggressive_auto;
    config.cooldown_minutes=clampi(g_settings.minimum_cooldown_minutes,1,120);
    for(int i=0;i<9;++i)if(g_settings.regions[i])config.selected_mask|=1u<<i;
    return config;
}
static void na_reconfigure(NAState *state,NAConfig next,ULONGLONG now){
    if(!state->initialized){
        memset(state,0,sizeof(*state));state->initialized=1;
        state->interval_anchor=now;state->armed=1;
    }else{
        if(state->config.interval_enabled!=next.interval_enabled||state->config.interval_minutes!=next.interval_minutes)
            state->interval_anchor=now;
        if(state->config.threshold_enabled!=next.threshold_enabled||state->config.threshold_percent!=next.threshold_percent){
            state->has_pressure=0;state->armed=1;
        }
        if(state->config.allow_aggressive!=next.allow_aggressive||
           na_effective_mask(state->config.selected_mask,state->config.allow_aggressive)!=na_effective_mask(next.selected_mask,next.allow_aggressive)){
            state->interval_anchor=now;state->has_pressure=0;state->armed=1;
        }
    }
    /* Do not erase the last completion: changing theme, language or thresholds
       must never bypass a cooldown or postpone an unchanged interval. */
    state->config=next;
}
static int na_due(NAState *state,double percent,ULONGLONG now,int paused){
    if(!state->initialized)return NA_NONE;
    NAConfig *config=&state->config;
    if(paused||percent<0.0||percent>100.0||percent!=percent){state->has_pressure=0;return NA_NONE;}
    ULONGLONG cooldown=(ULONGLONG)clampi(config->cooldown_minutes,1,120)*NA_MINUTE_MS;
    int cooldown_complete=!state->has_completion||now-state->last_completion>=cooldown;
    if(percent<=(double)config->threshold_percent-NA_HYSTERESIS)state->armed=1;
    /* If cleanup cannot lower sustained pressure, allow another attempt only
       after the full cooldown instead of leaving automation latched forever. */
    if(!state->armed&&state->has_completion&&cooldown_complete)state->armed=1;
    int threshold_due=0;
    if(config->threshold_enabled&&state->armed&&cooldown_complete&&percent>=config->threshold_percent){
        if(!state->has_pressure){state->pressure_start=now;state->has_pressure=1;}
        threshold_due=now-state->pressure_start>=NA_PRESSURE_MS;
    }else state->has_pressure=0;
    int interval_due=config->interval_enabled&&now-state->interval_anchor>=(ULONGLONG)clampi(config->interval_minutes,1,240)*NA_MINUTE_MS;
    return threshold_due?NA_THRESHOLD:interval_due?NA_INTERVAL:NA_NONE;
}
static void na_started(NAState *state,ULONGLONG now){
    state->last_attempt=now;state->interval_anchor=now;
    state->last_completion=now;state->has_completion=1;
    state->has_pressure=0;state->armed=0;
}
static void na_completed(NAState *state,ULONGLONG now){
    state->interval_anchor=now;state->last_completion=now;
    state->has_completion=1;state->has_pressure=0;
}
static void native_automation_preferences_changed(void){
    na_reconfigure(&na_state,na_current_config(),GetTickCount64());
}
static void native_automation_cleanup_completed(int succeeded){
    (void)succeeded;
    if(!na_state.initialized)native_automation_preferences_changed();
    /* Failed attempts also cool down, avoiding one error balloon per second. */
    na_completed(&na_state,GetTickCount64());
}
static void native_automation_tick(void){
    if(!na_state.initialized)native_automation_preferences_changed();
    if(!na_state.config.threshold_enabled&&!na_state.config.interval_enabled)return;
    ULONGLONG now=GetTickCount64();
    if(!native_actions_can_automate()||!a_elevated()){
        na_due(&na_state,0.0,now,1);return;
    }
    MEMORYSTATUSEX memory;memset(&memory,0,sizeof(memory));memory.dwLength=sizeof(memory);
    if(!GlobalMemoryStatusEx(&memory)||!memory.ullTotalPhys){na_due(&na_state,0.0,now,1);return;}
    double percent=100.0*(double)(memory.ullTotalPhys-memory.ullAvailPhys)/(double)memory.ullTotalPhys;
    int reason=na_due(&na_state,percent,now,0);if(reason==NA_NONE)return;
    DWORD mask=na_effective_mask(na_state.config.selected_mask,na_state.config.allow_aggressive);
    if(!mask)return;
    wchar_t trigger[64];translate(reason==NA_THRESHOLD?"Automática: limite de memória":"Automática: intervalo",trigger,64);
    /* Mark before dispatch because a failed CreateThread can complete inline. */
    na_started(&na_state,now);
    native_request_automatic_cleanup(mask,trigger);
}

static int native_automation_self_test(void){
    int failures=0;NAConfig config;memset(&config,0,sizeof(config));
    config.threshold_percent=88;config.interval_minutes=2;config.cooldown_minutes=15;config.selected_mask=511;
    NAState state;memset(&state,0,sizeof(state));na_reconfigure(&state,config,0);
    failures+=check(na_due(&state,99,3600000,0)==NA_NONE,"automatic cleanup stays disabled until an interval or threshold is selected");
    config.threshold_enabled=1;na_reconfigure(&state,config,100);
    int before=na_due(&state,90,100,0),unstable=na_due(&state,87,20000,0),restart=na_due(&state,90,21000,0);
    failures+=check(before==NA_NONE&&unstable==NA_NONE&&restart==NA_NONE&&na_due(&state,90,40999,0)==NA_NONE&&na_due(&state,90,41000,0)==NA_THRESHOLD,"threshold requires 20 seconds of continuous pressure and resets after a transient spike");
    na_started(&state,41000);na_completed(&state,42000);
    failures+=check(na_due(&state,84,43000,0)==NA_NONE&&state.armed&&na_due(&state,95,42000+14*NA_MINUTE_MS,0)==NA_NONE,"threshold hysteresis rearms below three points but preserves its independent cooldown");
    config.interval_enabled=1;na_reconfigure(&state,config,50000);
    failures+=check(na_due(&state,95,50000+2*NA_MINUTE_MS-1,0)==NA_NONE&&na_due(&state,95,50000+2*NA_MINUTE_MS,0)==NA_INTERVAL,"a two-minute interval is honored even when the saved threshold cooldown is fifteen minutes");
    na_started(&state,170000);na_completed(&state,171000);na_reconfigure(&state,config,200000);
    failures+=check(state.interval_anchor==171000&&state.last_completion==171000&&na_due(&state,50,290999,0)==NA_NONE&&na_due(&state,50,291000,0)==NA_INTERVAL,"unchanged preferences do not postpone intervals or erase completion cooldowns");
    config.interval_minutes=1;na_reconfigure(&state,config,300000);
    failures+=check(state.interval_anchor==300000&&state.last_completion==171000&&na_due(&state,95,359999,0)==NA_NONE&&na_due(&state,95,360000,0)==NA_INTERVAL,"editing the interval restarts its own clock without bypassing threshold cooldown");
    memset(&state,0,sizeof(state));na_reconfigure(&state,config,0);
    na_due(&state,95,40000,0);
    failures+=check(na_due(&state,95,60000,0)==NA_THRESHOLD,"simultaneous threshold and interval deadlines yield exactly one cleanup decision");
    na_started(&state,60000);
    failures+=check(na_due(&state,95,60001,0)==NA_NONE&&na_due(&state,95,120000,1)==NA_NONE&&!state.has_pressure,"busy, unauthorized and diagnostic states cannot accumulate pressure or dispatch cleanup");
    failures+=check(na_effective_mask(511,0)==(1u<<4)&&na_effective_mask(0,0)==(1u<<4)&&na_effective_mask(511,1)==511&&na_effective_mask(0,1)==0,"conservative automatic cleanup uses only low-priority standby; explicit advanced automation uses the selected regions");
    int was_busy=g_busy;g_busy=0;
    failures+=check(g_test_mode&&!native_request_automatic_cleanup(1u<<4,L"Teste")&&!a_job,"diagnostic mode refuses automatic dispatch before allocating a job, launching a child or requesting elevation");
    g_busy=was_busy;
    return failures;
}
