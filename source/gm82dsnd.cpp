//---------------------------------------------------------------------------//
/*

    Game Maker 8.2 DirectSound
    ==========================
    v0.1.0
    3 Sep 2026
  
  
  A modern audio engine for Game Maker 8.2.
  
  Written by renex. Requires Core and Buffer.
  
  Includes contributions from:
  
  - DFelipehDEV

*/
//---------------------------------------------------------------------------//
#pragma region header


#include <stdio.h>
#include <stdint.h>
#include <cmath>
#include <windows.h>
#include <dsound.h>
#include <initguid.h>
#include <Mmdeviceapi.h>

#include "../include/stb_vorbis.c"
//bruh
#undef L
#undef R
#undef C

#define MINIMP3_IMPLEMENTATION
#include "../include/minimp3.h"

#define MINIMP3_NO_STDIO
#include "../include/minimp3_ex.h"

#define LIBXMP_STATIC
#include "../include/xmp.h"

#pragma comment(lib,"dsound.lib")
#pragma comment(lib,"Dxguid.lib")
#pragma comment(lib,"Winmm.lib")
#pragma comment(lib,"../include/libxmp-lite-static.lib")

#define GMREAL extern "C" __declspec(dllexport) double __cdecl
#define GMSTR extern "C" __declspec(dllexport) char* __cdecl

#define ERROR_GENERIC     -1
#define ERROR_NON_EXIST   -2
#define ERROR_FAIL_LOAD   -3
#define ERROR_NO_SPACE    -4

#define EXPAND(x) x
#define GET_MACRO(_1, _2, NAME, ...) NAME

#define REPEAT2(x,n) for (int x = 0; x < (n); ++x)
#define REPEAT1(n) for (int _ = 0; _ < (n); ++_)
#define REPEAT(...) EXPAND(GET_MACRO(__VA_ARGS__, REPEAT2, REPEAT1)(__VA_ARGS__))

#define runner_function(type, name, addr, ...)\
    type (*name)(__VA_ARGS__) = (type(*)(__VA_ARGS__))addr;

#pragma endregion
//---------------------------------------------------------------------------//
#pragma region debug helpers

extern bool __vibe_check(const wchar_t* file, int line, HRESULT hr) {
    if (SUCCEEDED(hr)) return false;
    wchar_t buf[1024];
    _snwprintf_s(
        buf, 1024,
        L"DirectSound error in file %s at line %i:\nHRESULT = 0x%08X",
        file, line, hr
    );
    MessageBoxW(0, buf, L"Warning", 0);
    exit(1);
    return true;
}

#define WIDE2(x) L##x
#define WIDE1(x) WIDE2(x)
#define vibe_check(a) __vibe_check(WIDE1(__FILE__),__LINE__,a)

extern void debug_message(const wchar_t* msg) {    
    MessageBoxW(0, msg, L"Debug message", 0);
}

extern void debug_message(const wchar_t* msg, int value) {    
    wchar_t buf[1024];
    _snwprintf_s(buf, 1024, msg, value);
    MessageBoxW(0, buf, L"Debug message", 0);
}

#pragma endregion
//---------------------------------------------------------------------------//
#pragma region types and globals


//directsound
    LPDIRECTSOUND Device;
    LPDIRECTSOUNDBUFFER PrimaryBuffer;
    LPDIRECTSOUNDBUFFER SecondaryBuffer;
    DSBUFFERDESC BufferDescriptor;
    WAVEFORMATEX FormatDescriptor;
    

//wave format
    #pragma pack(push, 1)
    struct RiffWaveFmt {
        char RIFF[4];
        uint32_t size;
        char WaveFmt[8];
        uint32_t FormatLength;
        uint16_t Format;
        uint16_t Channels;
        uint32_t SampleRate;
        uint32_t BytesPerSec;
        uint16_t BlockAlign;
        uint16_t BitsPerSample;
    };
    #pragma pack(pop)


//runner hacking
    struct TMemoryStream {
        uint32_t vfp;
        uint8_t* memory;
        uint32_t size;
        uint32_t position;
        uint32_t capacity;
    };

    struct GMSound {
        uint32_t vfp;
        uint32_t kind;
        char* extension;
        char* origname;
        TMemoryStream* memstream;
        uint32_t preload;
        uint32_t effects;
        double volume;
        double pan;
        uint32_t index;
        wchar_t* fname;
    };

    static GMSound*** gm_sound_mem = (GMSound***)0x6840c0;
    static uint32_t* gm_sound_count = (uint32_t*)0x6840c8;
    
    runner_function(void,YY_sound_free,0x00514154,int);


//objects
    struct SoundResource {
        float* buffer;
        int index;
        int inst_count;
        int frequency;
        int kind;
        int posmul;
        int loop_a;
        int loop_b;
        int loop_c;
        int length;
        float volume;
        float pan;
        float pitch;
        bool exists = false;
        bool loaded;
        bool persistent;
        bool preload;
    };

    struct SoundInstance {
        SoundResource* sound;
        LPDIRECTSOUNDBUFFER buffer;
        int write_cursor;
        int play_cursor;
        int index;
        int age;
        int loop_a;
        int loop_b;
        int loop_c;
        int loop_count;
        float volume;
        float pan;
        float pitch;
        float fade_from;
        float fade_to;
        int fade_length;
        int fade_count;
        bool exists = false;
        bool playing;
        bool looping;        
        bool fading;        
        bool persistent;
        bool scheduled;
    };


//Libxmp
    xmp_context XMP_CONTEXT;


//constants
    #define THREAD_MS       15
    #define RESOURCE_COUNT  100000
    #define INSTANCE_COUNT  100
    #define NUM_KINDS       8


//mixer
    int16_t* TertiaryBuffer;
    int BUFFERLENGTH;
    int BUFFERSAFE;


//global variables
    SoundResource sound_resources[RESOURCE_COUNT];
    SoundInstance sound_instances[NUM_KINDS][INSTANCE_COUNT];
    
    double VOLUME = 0.7;
    
    int LAST_INST_ID = RESOURCE_COUNT;
    int LAST_SND_ID;
    int BGM_INST_ID = -4;
    int MM_INST_ID = -4;
    int BUILTIN_COUNT = 0;
    int SYSTEM_SAMPLE_RATE = 48000;
    int MAX_SAMPLE_RATE;
    
    bool SET_LIN_VOLUME = true;
    bool SET_SCHEDULER = true;
    bool SET_REUSE_SNDIDS = true;
    bool SET_PERSISTENCE = true;
    bool SET_MASTERING = true;
    bool SET_LAYERING = false;


#pragma endregion
//---------------------------------------------------------------------------//
#pragma region function prototypes

//internals
    void dsound_hook();
    void dsound_init();
    void dsound_thread_update();
    void dsound_frame_update(int);
    int dsound_get_free_resource();
    int dsound_get_free_instance();
    int dsound_sound_from_instance(int);
    bool dsound_instance_from_iid(int, int*, int*, SoundInstance**);
    LONG dsound_volume_formula(double);
    LONG dsound_pan_formula(double);
    void dsound_mixer_thread(SoundInstance*);

//adding sounds
    void dsound_load_builtin(int);
    int dsound_add_file(char*, int);
    int dsound_add_mem(uint8_t*, int, int);
    int dsound_add_mem_index(int, uint8_t*, int, int);

//instance control
    int dsound_play(int, bool, double, double, double);
    bool dsound_exists(int);
    void dsound_stop(int);
    void dsound_inst_start(SoundInstance*);
    void dsound_inst_pause(SoundInstance*);
    void dsound_inst_stop(int);
    void dsound_inst_free(SoundInstance*);
    void dsound_sound_stop(int);
    void dsound_stop_nonp();
    void dsound_kind_stop(int);
    void dsound_stop_all();

//setters getters
    void dsound_set_volume(double);
    void dsound_set_pause(int, bool);
    void dsound_setpause_all(bool);
    void dsound_setpause_kind(int, bool);
    int dsound_get_position(SoundInstance*);
    void dsound_set_position(SoundInstance*, int);


#pragma endregion
//---------------------------------------------------------------------------//
#pragma region system boilerplate


bool WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
    //hooks runner and disables preload for all sound resources
    
    if (fdwReason == DLL_PROCESS_ATTACH) dsound_hook();
    
    return true;
}

void CALLBACK timer_callback(UINT, UINT, DWORD, DWORD, DWORD) {
    //called in the multimedia timer thread
    
    dsound_thread_update();
}

DSBUFFERDESC* describe_buffer(DWORD flags, WAVEFORMATEX* format, DWORD size) {
    //fills and returns a directsound buffer descriptor structure
    
    memset(&BufferDescriptor,0,sizeof(BufferDescriptor));
    BufferDescriptor.dwFlags = flags;
    BufferDescriptor.dwBufferBytes = size;
    BufferDescriptor.lpwfxFormat = format;
    BufferDescriptor.dwReserved = 0;
    BufferDescriptor.dwSize = sizeof(DSBUFFERDESC);
    return &BufferDescriptor;
}

WAVEFORMATEX* describe_format(int samplerate, int channels, int bits) {
    //fills and returns a directsound format descriptor structure
    
    memset(&FormatDescriptor,0,sizeof(FormatDescriptor));
    FormatDescriptor.wFormatTag = WAVE_FORMAT_PCM;
    FormatDescriptor.nChannels = (WORD)channels;
    FormatDescriptor.nSamplesPerSec = (DWORD)samplerate;
    FormatDescriptor.wBitsPerSample = (WORD)bits;
    FormatDescriptor.nBlockAlign =
        (FormatDescriptor.wBitsPerSample / 8) * FormatDescriptor.nChannels;
    FormatDescriptor.nAvgBytesPerSec =
        FormatDescriptor.nSamplesPerSec * FormatDescriptor.nBlockAlign;
    FormatDescriptor.cbSize = 0;
    return &FormatDescriptor;
}

int system_get_primary_samplerate() {
    //returns the sample rate of the default audio device,
    //or a reasonable default if not possible
    
    HRESULT hr;
    IMMDevice * pDevice = NULL;
    IMMDeviceEnumerator * pEnumerator = NULL;
    IPropertyStore* store = NULL;
    PWAVEFORMATEX deviceFormatProperties;
    PROPVARIANT prop;

    CoInitialize(NULL);

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (LPVOID *)&pEnumerator
    );

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);

    hr = pDevice->OpenPropertyStore(STGM_READ, &store);
    if (FAILED(hr)) {
        return 48000;
    }

    hr = store->GetValue(PKEY_AudioEngine_DeviceFormat, &prop);
    if (FAILED(hr)) {
        return 48000;
    }

    deviceFormatProperties = (PWAVEFORMATEX)prop.blob.pBlobData;

    return deviceFormatProperties->nSamplesPerSec;
}

#pragma endregion
//---------------------------------------------------------------------------//
#pragma region Game Maker interface


GMREAL __dsound_init() {
    dsound_init();
    return 0;
}

GMREAL __dsound_update(double frame_ms) {    
    dsound_frame_update((int)frame_ms);    
    return 0;
}

GMREAL __dsound_add_file(char* fname, double kind) {
    return (double)dsound_add_file(fname, (int)kind);
}

GMREAL __dsound_add_mem(double buffer, double length, double kind) {
    return (double)dsound_add_mem((uint8_t*)(int)buffer, (int)length, (int) kind);
}

GMREAL __dsound_play(double index, double loop, double vol, double pan, double pitch) {
    return (double)dsound_play((int)index, (loop>0.5), vol, pan, pitch);
}

GMREAL __dsound_stop(double index) {
    dsound_stop((int)index);
    return 0;
}

GMREAL __dsound_glob_vol(double vol) {
    dsound_set_volume(vol);
    return 0;
}

GMREAL __dsound_exists(double index) {
    return dsound_exists((int)index)?1:0;
}

GMREAL __dsound_setter(double index, double op, double value) {
    if (index < 0) return 0;
    
    if (index >= RESOURCE_COUNT) {
        int kind, iid;
        SoundInstance* inst;
        if (dsound_instance_from_iid((int)index, &kind, &iid, &inst)) {
            switch ((int)op) {
                case 0: inst->volume = min(1.0,max(0.0,value)); break;
                case 1: inst->pan    = min(1.0,max(-1.0,value)); break;
                case 2: inst->pitch  = max(100.0/inst->sound->frequency,value); break;
                case 5: inst->loop_a = (int)value; break;
                case 6: inst->loop_b = (int)value; break;
                case 9: inst->loop_c = (int)value; break;
                case 11: dsound_set_position(inst,(int)value); break;
            }
        }
        
        return 0;
    }
    
    if (!sound_resources[(int)index].exists) return 0;
    
    switch ((int)op) {
        case 0: sound_resources[(int)index].volume = min(1.0,max(0.0,value)); break;
        case 1: sound_resources[(int)index].pan    = min(1.0,max(-1.0,value)); break;
        case 2: sound_resources[(int)index].pitch  = max(100.0/sound_resources[(int)index].frequency,value); break;
        case 5: sound_resources[(int)index].loop_a = (int)value; break;
        case 6: sound_resources[(int)index].loop_b = (int)value; break;
        case 9: sound_resources[(int)index].loop_c = (int)value; break;
    }
    
    return 0;
}

GMREAL __dsound_getter(double index, double op) {
    if (index < 0) return 0;
    
    if (index >= RESOURCE_COUNT) {
        int kind, iid;
        SoundInstance* inst;
        if (dsound_instance_from_iid((int)index, &kind, &iid, &inst)) {
            switch ((int)op) {
                case 0: return inst->volume;
                case 1: return inst->pan;
                case 2: return inst->pitch;
                case 3: return (double)inst->sound->preload;
                case 4: return (double)inst->sound->kind;
                case 5: return (double)inst->loop_a;
                case 6: return (double)inst->loop_b;
                case 7: return (double)inst->sound->frequency;
                case 8: return (double)inst->sound->length;
                case 9: return (double)inst->loop_c;
                case 11: return (double)dsound_get_position(inst);
            }
        }
        
        return ERROR_NON_EXIST;
    }
    
    if (!sound_resources[(int)index].exists) return ERROR_NON_EXIST;
    
    switch ((int)op) {
        case 0: return sound_resources[(int)index].volume;
        case 1: return sound_resources[(int)index].pan;
        case 2: return sound_resources[(int)index].pitch;
        case 3: return (double)sound_resources[(int)index].preload;
        case 4: return (double)sound_resources[(int)index].kind;
        case 5: return (double)sound_resources[(int)index].loop_a;
        case 6: return (double)sound_resources[(int)index].loop_b;
        case 7: return (double)sound_resources[(int)index].frequency;
        case 8: return (double)sound_resources[(int)index].length;
        case 9: return (double)sound_resources[(int)index].loop_c;
    }
    
    return ERROR_GENERIC;
}

GMREAL __dsound_insts(double index) {
    if (index < 0 || index >= RESOURCE_COUNT) return false;
    return (double)sound_resources[(int)index].inst_count;
}

GMREAL __dsound_get_builtin_count() {
    return (double)BUILTIN_COUNT;
}

GMREAL __dsound_stop_nonpersist() {
    dsound_stop_nonp();
    return 0;
}

GMREAL __dsound_stop_all() {
    dsound_stop_all();
    return 0;
}

GMREAL __dsound_setpause(double index, double pause) {
    dsound_set_pause((int)index, pause>0.5);
    return 0;
}

GMREAL __dsound_setpause_all(double pause) {
    dsound_setpause_all(pause>0.5);
    return 0;
}

GMREAL __dsound_setpause_kind(double kind, double pause) {
    dsound_setpause_kind((int)kind, pause>0.5);
    return 0;
}

GMREAL __dsound_getbgid() {
    int kind, index;
    SoundInstance* inst;
    if (dsound_instance_from_iid(BGM_INST_ID, &kind, &index, &inst)) {
        return (double)BGM_INST_ID;
    }
    return -4;
}

GMREAL __dsound_settings(double setting, double value) {
    switch ((int)setting) {
        case 0: SET_LIN_VOLUME = (value>0.5); break;
        case 1: SET_SCHEDULER = (value>0.5); break;
        case 2: SET_REUSE_SNDIDS = (value>0.5); break;
        case 3: SET_PERSISTENCE = (value>0.5); break;
        case 4: SET_MASTERING = (value>0.5); break;
        case 5: SET_LAYERING = (value>0.5); break;
    }    
    return 0;
}

GMREAL __dsound_loopsetter(double index, double loopa, double loopb, double loopc) {
    if (index < 0) return 0;
    
    if (index >= RESOURCE_COUNT) {
        int kind, iid;
        SoundInstance* inst;
        if (dsound_instance_from_iid((int)index, &kind, &iid, &inst)) {
            inst->loop_a = (int)loopa;
            inst->loop_b = (int)loopb;
            inst->loop_c = (int)loopc;
        }
        
        return 0;
    }
    
    if (sound_resources[(int)index].exists) {    
        sound_resources[(int)index].loop_a = (int)loopa;
        sound_resources[(int)index].loop_b = (int)loopb;
        sound_resources[(int)index].loop_c = (int)loopc;
    }
    
    return 0;
}

#pragma endregion
//---------------------------------------------------------------------------//
#pragma region internals

double floorpo2(double val) {
    int i=2;
    do {i*=2;} while (i<val);
    return i/2.0;
}

double pitch_shifter(double pitch) {
    return pitch / floorpo2(pitch);
}


void dsound_hook() {
    //hooks runner and disables 'preload' on all sound resources
    
    BUILTIN_COUNT = *gm_sound_count;
    GMSound* sound;
    
    REPEAT(i, BUILTIN_COUNT) {
        sound=(*gm_sound_mem)[i];
        if (sound) {
            sound_resources[i].preload = (sound->preload>0);
            sound->preload = 0;
        }
    }
}

void dsound_init() {
    //initializes all systems
    
    //directsound
        SYSTEM_SAMPLE_RATE = system_get_primary_samplerate();
        
        //debug_message(L"system sample %i",SYSTEM_SAMPLE_RATE);
        
        vibe_check(DirectSoundCreate(NULL, &Device, NULL));
        
        //you're supposed to use your application's window here but the
        //desktop window works and i haven't been able to find any problems.
        //using the desktop prevents the extension from having to wait for
        //the runner to create a window, which allows full extension usage
        //without compromises during the earliest GML events.
        vibe_check(Device->SetCooperativeLevel(
            GetDesktopWindow(),
            DSSCL_PRIORITY
        ));
    
        vibe_check(Device->CreateSoundBuffer(
            describe_buffer(
                DSBCAPS_PRIMARYBUFFER,
                NULL,
                0
            ),
            &PrimaryBuffer,
            NULL
        ));
        vibe_check(PrimaryBuffer->Play(0, 0, DSBPLAY_LOOPING));
        
        //get max samplerate for pitch shifting
        //todo: not needed after mixer rewrite
        DSCAPS dscaps;
        dscaps.dwSize = sizeof(DSCAPS);
        vibe_check(Device->GetCaps(&dscaps));
        MAX_SAMPLE_RATE = dscaps.dwMaxSecondarySampleRate;
        
        //create master mixer buffer and the tertiary copy buffer
        BUFFERSAFE = (int)(SYSTEM_SAMPLE_RATE / 10.0);
        BUFFERLENGTH = BUFFERSAFE * 4;
        
        /*vibe_check(Device->CreateSoundBuffer(
            describe_buffer(
                DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2,
                describe_format(SYSTEM_SAMPLE_RATE,2,16),
                BUFFERLENGTH * 4 //in bytes
            ),
            &SecondaryBuffer,
            NULL
        ));
        vibe_check(SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING));*/
        
        TertiaryBuffer = (int16_t*)malloc(BUFFERSAFE * sizeof(int16_t) * 2);
    
    
    //set up timer callback
        timeSetEvent(
            THREAD_MS,
            THREAD_MS,
            timer_callback,
            0,
            TIME_PERIODIC
        );
    
    
    //initialize Libxmp
        XMP_CONTEXT = xmp_create_context();
        xmp_set_player(XMP_CONTEXT, XMP_PLAYER_DEFPAN, 60);
        xmp_set_player(XMP_CONTEXT, XMP_PLAYER_INTERP, XMP_INTERP_SPLINE);
    
    
    //load builtin sounds
        REPEAT(i, BUILTIN_COUNT) if ((*gm_sound_mem)[i]) {
            dsound_load_builtin(i);
        }
        LAST_SND_ID = BUILTIN_COUNT;
        if (LAST_SND_ID == 0) LAST_SND_ID = 1;
}

void dsound_thread_update() {
    //threaded; uses THREAD_MS increments
    
    SoundInstance* inst;
    
    REPEAT(kind, NUM_KINDS) REPEAT(i, INSTANCE_COUNT) {
        inst = &sound_instances[kind][i];
        //TODO: this is a good place to perform dead instance buffer cleanup
        /*if (inst->dying) {
            //etc
            if (the play cursor is beyond the write cursor) {
                dsound_inst_free(inst);
            }
        }
        }*/
        if (inst->exists) {
            if (inst->fading) {
                //upkeep instance fading
            }   
            dsound_mixer_thread(inst);            
        }
    }
}

void dsound_frame_update(int frame_ms) {
    //gml step
    //update instance life and tick the scheduler
    
    SoundInstance* inst;
    
    REPEAT(kind, NUM_KINDS) REPEAT(i, INSTANCE_COUNT) {
        inst = &sound_instances[kind][i];
        if (inst->exists) {            
            inst->age++;
            if (SET_SCHEDULER) {
                //if the instance is scheduled, start it
                if (inst->scheduled) {
                    inst->scheduled = false;
                    dsound_inst_start(inst);
                }
                
                //update buffer properties
                double volume_final = inst->sound->volume * inst->volume * VOLUME;
                double pan_final = inst->sound->pan + inst->pan;
                double pitch_final = inst->sound->pitch * inst->pitch;
                
                inst->buffer->SetVolume(dsound_volume_formula(volume_final));
                inst->buffer->SetPan(dsound_pan_formula(pan_final));
                inst->buffer->SetFrequency((DWORD)(pitch_shifter(pitch_final) * inst->sound->frequency));
            }            
        }
    }
}

int dsound_get_free_resource() {
    //returns an available index to create a sound resource
    
    if (!SET_REUSE_SNDIDS) {
        //incrementing ids only; get the next one
        return LAST_SND_ID;
    }
    
    //holes allowed
    REPEAT(i, RESOURCE_COUNT) {
        //we do not use resource zero
        if (i>0)
            if (!sound_resources[i].exists) return i;
    }
    
    return ERROR_NO_SPACE;
}

int dsound_get_free_instance(int kind) {
    //finds and returns a free instance index
    //if one isn't available, the oldest instance is freed and reused
    
    SoundInstance* inst;
    SoundInstance* oldest;
    int oldest_id = 0;
    oldest = &sound_instances[kind][oldest_id];
    REPEAT(i, INSTANCE_COUNT) {
        inst = &sound_instances[kind][i];
        if (!inst->exists) return i;
        
        if (inst->age > oldest->age) {
            oldest = inst;
            oldest_id = i;
        }
    }
    dsound_inst_free(oldest);
    return oldest_id;
}

int dsound_sound_from_instance(int unknown_id) {
    //convenience function that converts unknown ids into a sound resource id
    
    if (unknown_id >= RESOURCE_COUNT) {
        //is instance; verify
        int kind, iid;
        SoundInstance* inst;
        if (dsound_instance_from_iid(unknown_id, &kind, &iid, &inst)) {
            return inst->sound->index;
        } else {
            return ERROR_NON_EXIST;
        }
    } else {
        //is sound
        if (unknown_id < 0) return ERROR_NON_EXIST;
        if (!sound_resources[(int)unknown_id].exists) return ERROR_NON_EXIST;
        return unknown_id;
    }
}

bool dsound_instance_from_iid(
    int iid, int* get_kind, int* get_index, SoundInstance** get_inst
) {
    //finds an instance given its unique instance id
    
    *get_kind = ERROR_NON_EXIST;
    *get_index = ERROR_NON_EXIST;
    
    SoundInstance* inst;
    
    REPEAT(kind, NUM_KINDS) REPEAT(i, INSTANCE_COUNT) {
        inst = &sound_instances[kind][i];
        if (inst->exists && inst->index == iid) {
            *get_kind = kind;
            *get_index = i;
            *get_inst = inst;
            return true;
        }
    }
    
    return false;
}

LONG dsound_volume_formula(double vol) {
    //decode log volume used by directsound
    
    if (!SET_LIN_VOLUME) return (LONG)(-10000.0 * (1.0-vol));
    
    return (LONG)(3333.3 * log10(max(0.001,min(1.0,vol))));
}

LONG dsound_pan_formula(double pan) {
    //decode log pan used by directsound
    
    if (pan>=0) return (LONG)(-3333.3 * log10(max(0.001,min(1.0,1.0-pan))));
    else return (LONG)(3333.3 * log10(max(0.001,min(1.0,1.0+pan))));
}

#pragma endregion
//---------------------------------------------------------------------------//
#pragma region adding sounds


void dsound_load_builtin(int index) {
    //loads a builtin sound resource
    //note: no checks because the parent function already checks everything
    
    //grab the gm sound struct's memory stream by traversing memory
    GMSound* sound = (*gm_sound_mem)[index];
    TMemoryStream* memstream = sound->memstream;
    
    if (memstream == NULL) {
        //if there is no memory stream in the sound struct, then that means
        //we are looking at an exported "external codec" sound file...
        
        //or an empty sound resource.
        if (sound->fname == NULL) {
            return;
        }
        
        //the path to the temp file is stored on this field
        FILE* file = _wfopen(sound->fname, L"rb");
        
        if (file == NULL) return;
        
        fseek(file, 0, SEEK_END);
        int size = ftell(file);
        fseek(file, 0, SEEK_SET);        
        uint8_t* buffer = (uint8_t*)malloc(size);
        fread(buffer, size, 1, file);
        fclose(file);
        
        dsound_add_mem_index(index, buffer, size, sound->kind);
        
        //apply volume and pan from the sound resource
        sound_resources[index].volume =
            pow(10.0, (sound->volume) * 3.0 - 1.0) / 100.0;        
        sound_resources[index].pan = sound->pan;
        
        free(buffer);
    } else {
        //it's a valid builtin sound type loaded in memory
        //we can just grab it...
        dsound_add_mem_index(
            index,
            sound->memstream->memory,
            sound->memstream->size,
            sound->kind
        );
        
        //apply volume and pan from the sound resource
        sound_resources[index].volume =
            pow(10.0, (sound->volume) * 3.0 - 1.0) / 100.0;        
        sound_resources[index].pan = sound->pan;
        
        //we should also destroy it to save memory
        //note: if preloading is implemented, remove this
        YY_sound_free(index);
    }
}

int dsound_add_file(char* fname, int kind) {
    //adds a new sound resource from disk
    
    FILE* file = fopen(fname, "rb");
    if (file == NULL) return ERROR_NON_EXIST;
    
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);        
    uint8_t* buffer = (uint8_t*)malloc(size);
    fread(buffer, size, 1, file);
    fclose(file);
    
    int id = dsound_add_mem(buffer, size, kind);
    
    free(buffer);
    
    return id;
}

int dsound_add_mem(uint8_t* buffer, int length, int kind) {
    //adds a new sound resource from a buffer
    
    int id = dsound_get_free_resource();
    
    //there is no space!
    if (id < 0) return id;
    
    //add the sound.
    id = dsound_add_mem_index(id, buffer, length, kind);
    
    //I am error.
    if (id<0) return id;
    
    //increment id if using incrementing ids
    if (!SET_REUSE_SNDIDS) LAST_SND_ID++;
    
    return id;
}

int dsound_add_mem_index(int id, uint8_t* buffer, int length, int kind) {
    //adds a new sound resource from a buffer, in a specific index slot
    
    LPDIRECTSOUNDBUFFER secbuffer;
    int samplerate, channels, bits, sample_count;
    uint32_t data_length;
    uint8_t* data;
    int16_t* data16;
    float* sample_buffer;
    int mode;
    
    //find file type from magic number
    if (memcmp("RIF",buffer,3) == 0) {
        //read wav
        mode = 0;
        
        RiffWaveFmt* format = (RiffWaveFmt*)buffer;
        
        data = (uint8_t*)(buffer + 16);
        data_length = format->FormatLength;
        
        //navigate wav blocks until we get to the real data block
            do {        
                data += data_length + 8;
                data_length = *(uint32_t*)(data);
            } while (
                memcmp("data", data - 4, 4) != 0
             && data - buffer < length - 16
            );
            data += 4;            
    
        samplerate = format->SampleRate;
        channels = format->Channels;
        bits = format->BitsPerSample;
        
        sample_count = (data_length / channels) / (bits / 8);
        
        data16 = (int16_t*)data;
        sample_buffer = (float*)malloc(sample_count * 2 * sizeof(float));
        
        if (bits == 8) {
            if (channels == 1) REPEAT(i, sample_count) {
                //mono 8 bit
                sample_buffer[i*2 + 0] = (data[i] / 128.0 - 1);
                sample_buffer[i*2 + 1] = sample_buffer[i*2 + 0];
            } else if (channels == 2) REPEAT(i, sample_count) {
                //stereo 8 bit
                sample_buffer[i*2 + 0]=(data[i*2 + 0]/128.0 - 1);
                sample_buffer[i*2 + 1]=(data[i*2 + 1]/128.0 - 1);
            } else {
                return ERROR_FAIL_LOAD;
            }
        } else 
        if (bits == 16) {
            if (channels == 1) REPEAT(i, sample_count) {
                //mono 16 bit
                sample_buffer[i*2 + 0] = data16[i] / 32768.0;
                sample_buffer[i*2 + 1] = sample_buffer[i*2 + 0];
            } else if (channels == 2) REPEAT(i, sample_count) {
                //stereo 16 bit
                sample_buffer[i*2 + 0] = data16[i*2 + 0] / 32768.0;
                sample_buffer[i*2 + 1] = data16[i*2 + 1] / 32768.0;
            } else {
                return ERROR_FAIL_LOAD;
            }
        } else {
            return ERROR_FAIL_LOAD;
        }        
    } else if (memcmp("Ogg",buffer,3)==0) {
        //read ogg
        mode = 1;

        int16_t* ogg_data = NULL;
        sample_count = stb_vorbis_decode_memory(
            (const uint8_t*)buffer, length,
            &channels, &samplerate, &ogg_data
        );
        
        if (sample_count <= 0) return ERROR_FAIL_LOAD;
        
        bits = 16;
        data_length = sample_count * channels * 2;
        
        data16 = ogg_data;
        sample_buffer = (float*)malloc(sample_count * 2 * sizeof(float));
        REPEAT(i, sample_count) {
            //stereo 16 bit
            sample_buffer[i*2 + 0] = data16[i*2 + 0] / 32768.0;
            sample_buffer[i*2 + 1] = data16[i*2 + 1] / 32768.0;
        }
        
        free(ogg_data);
    } else if (
        memcmp("ID3",buffer,3) == 0
    ||  (buffer[0] == 0xff && buffer[1] == 0xfb) //check for file w/o id3 tag
    ) {
        //read mp3
        mode = 2;
        
        mp3dec_t mp3d;
        mp3dec_file_info_t info;
        if (mp3dec_load_buf(
            &mp3d,
            (const uint8_t*)buffer, length,
            &info,
            NULL, NULL
        )) {
            return ERROR_FAIL_LOAD;
        }     
        
        samplerate = info.hz;
        channels = info.channels;
        bits = 16; 
        data_length = info.samples * 2;
        
        sample_count = info.samples / channels;
        
        data16 = (int16_t*)info.buffer;
        sample_buffer = (float*)malloc(sample_count * 2 * sizeof(float));
        REPEAT(i, sample_count) {
            //stereo 16 bit
            sample_buffer[i*2 + 0] = data16[i*2 + 0] / 32768.0;
            sample_buffer[i*2 + 1] = data16[i*2 + 1] / 32768.0;
        }
        
        free(info.buffer);
    } else {
        //unrecognized file type - let's try loading it as a module!
        mode = 3;
        
        struct xmp_frame_info frameinfo;
        
        //first we need to determine how long the module is,
        //so we load it without instruments for quicker rendering
        xmp_set_player(XMP_CONTEXT, XMP_PLAYER_SMPCTL, XMP_SMPCTL_SKIP);
        
        if (xmp_load_module_from_memory(    
            XMP_CONTEXT,
            (const void*)buffer, (LONG)length
        ) != 0) {
            //it probably isn't a module...
            return ERROR_FAIL_LOAD;
        }
        
        xmp_start_player(XMP_CONTEXT, SYSTEM_SAMPLE_RATE, 0);
        
        data_length = 0;
        while (xmp_play_frame(XMP_CONTEXT) == 0) {
            xmp_get_frame_info(XMP_CONTEXT, &frameinfo);
            data_length += frameinfo.buffer_size;
            if (frameinfo.loop_count > 0) break;
        }
        
        xmp_end_player(XMP_CONTEXT);
        xmp_release_module(XMP_CONTEXT);
        
        if (data_length == 0) {
            //sorrgy... accedent...
            return ERROR_FAIL_LOAD;
        }
        
        //reload module with instruments enabled, and render it proper
        xmp_set_player(XMP_CONTEXT, XMP_PLAYER_SMPCTL, 0);
        xmp_load_module_from_memory(
            XMP_CONTEXT,
            (const void*)buffer, (LONG)length
        );
        xmp_start_player(XMP_CONTEXT, SYSTEM_SAMPLE_RATE, 0);
        
        //allocate enough space for the final size of the module render
        data = (uint8_t*)malloc(data_length);
        uint8_t* position = data;
        
        while (xmp_play_frame(XMP_CONTEXT) == 0) {
            xmp_get_frame_info(XMP_CONTEXT, &frameinfo);
            if (frameinfo.loop_count > 0) break;
            memcpy(position, frameinfo.buffer, frameinfo.buffer_size);
            position += frameinfo.buffer_size;
        }
        
        xmp_end_player(XMP_CONTEXT);
        xmp_release_module(XMP_CONTEXT);
        
        samplerate = SYSTEM_SAMPLE_RATE;
        channels = 2;
        bits = 16;
        sample_count = data_length / 4;
        
        data16 = (int16_t*)data;
        sample_buffer = (float*)malloc(sample_count * 2 * sizeof(float));
        REPEAT(i, sample_count) {
            //stereo 16 bit
            sample_buffer[i*2 + 0] = data16[i*2 + 0] / 32768.0;
            sample_buffer[i*2 + 1] = data16[i*2 + 1] / 32768.0;
        }
        
        free(data);
    }
    
    /** /
        debug_message(L"sample rate %i",samplerate);
        debug_message(L"channels %i",channels);
        debug_message(L"bits %i",bits);        
        debug_message(L"data length %i",data_length);
    /**/
    
    //create the sound resource
    SoundResource* sound = &sound_resources[id];
    sound->index = id;
    sound->buffer = sample_buffer;
    sound->kind = kind;
    sound->posmul = channels * (bits / 8);
    sound->length = data_length / sound->posmul;
    sound->loop_a = 0;
    sound->loop_b = sound->length;
    sound->loop_c = 0;
    sound->exists = true;
    sound->loaded = true;
    sound->persistent = false;
    sound->frequency = samplerate;
    sound->volume = 1.0;
    sound->pan = 0.0;
    sound->pitch = 1.0;
    sound->inst_count = 0;
    
    return id;
}

#pragma endregion
//---------------------------------------------------------------------------//
#pragma region instance control


int dsound_play(int index, bool loop, double vol, double pan, double pitch) {
    SoundResource* sound = &sound_resources[index];
    if (!sound->exists) return ERROR_NON_EXIST;
    
    //LPDIRECTSOUNDBUFFER clone;
    //vibe_check(Device->DuplicateSoundBuffer(sound->buffer, &clone));
    
    int kind = sound->kind;
    
    if (kind == 1) {
        if (!SET_LAYERING) dsound_kind_stop(1);
        BGM_INST_ID = LAST_INST_ID;
    }
    if (kind == 3) {
        if (!SET_LAYERING) dsound_kind_stop(3);
        MM_INST_ID = LAST_INST_ID;
    }
    
    LPDIRECTSOUNDBUFFER secbuffer;
    
    vibe_check(Device->CreateSoundBuffer(
        describe_buffer(
            DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY,
            describe_format(sound->frequency, 2, 16),
            BUFFERLENGTH * 4
        ),
        &secbuffer,
        NULL
    ));
    
    double volume_final = sound->volume * min(1.0,max(0.0,vol)) * VOLUME;
    double pan_final = sound->pan + pan;
    double pitch_final = sound->pitch * pitch;
    
    secbuffer->SetVolume(dsound_volume_formula(volume_final));
    secbuffer->SetPan(dsound_pan_formula(pan_final));
    secbuffer->SetFrequency((DWORD)(pitch_shifter(pitch_final) * sound->frequency));
    
    SoundInstance* inst = &sound_instances[kind][dsound_get_free_instance(kind)];
    inst->sound = sound;
    inst->buffer = secbuffer;
    inst->write_cursor = -1;
    inst->play_cursor = 0;
    inst->index = LAST_INST_ID;
    inst->volume = volume_final;
    inst->pan = pan_final;
    inst->pitch = pitch_final;
    inst->fade_from = volume_final;
    inst->fade_to = volume_final;
    inst->loop_a = sound->loop_a;
    inst->loop_b = sound->loop_b;
    inst->loop_c = sound->loop_c;
    inst->loop_count = 0;
    inst->fade_length = 1;
    inst->fade_count = 1;
    inst->age = 0;
    inst->exists = true;
    inst->playing = false;
    inst->looping = loop;
    inst->persistent = sound->persistent;
    
    sound->inst_count++;
    
    LAST_INST_ID++;
    
    if (SET_SCHEDULER) {
        //schedule play
        inst->scheduled = true;
    } else {
        //scheduler disabled. play immediately
        dsound_inst_start(inst);
    }
    
    return inst->index;
}

bool dsound_exists(int index) {
    if (index < 0) return 0;
    if (index >= RESOURCE_COUNT) {
        int kind, iid;
        SoundInstance* inst;
        if (dsound_instance_from_iid((int)index, &kind, &iid, &inst)) {
            return inst->exists;
        }
        return false;
    }
    return sound_resources[(int)index].exists;
}

void dsound_stop(int index) {
    if (index < 0) return;
    if (index >= RESOURCE_COUNT) dsound_inst_stop(index);
    else dsound_sound_stop(index);
}

void dsound_inst_start(SoundInstance* inst) {
    //if (inst->looping) {
        vibe_check(
            inst->buffer->Play(0, 0, DSBPLAY_LOOPING)
        );
    //} else {
    //    vibe_check(
    //        inst->clone_buffer->Play(0, 0, 0)
    //    );
    //}
    inst->playing = true;
    //todo: mixer needs some data here
}

void dsound_inst_pause(SoundInstance* inst) {
    vibe_check(inst->buffer->Stop());
    inst->playing = false;
}

void dsound_inst_stop(int iid) {
    //stops an instance given its instance id
    
    int kind, index;
    SoundInstance* inst;
    
    if (dsound_instance_from_iid(iid, &kind, &index, &inst)) {
        dsound_inst_free(inst);
    }
}

void dsound_inst_free(SoundInstance* inst) {
    //frees an instance by reference
    
    if (inst->exists) {
        inst->sound->inst_count--;
        inst->buffer->Stop();
        inst->buffer->Release();
        inst->exists = false;
    }
}

void dsound_sound_stop(int index) {
    //stops all instances of a sound
    
    SoundResource* sound = &sound_resources[index];
    
    if (sound->exists) {
        SoundInstance* inst;
        
        int kind = sound->kind;
        
        REPEAT(i, INSTANCE_COUNT) {
            inst = &sound_instances[kind][i];
            if (inst->exists && inst->sound == sound) {
                dsound_inst_free(inst);
            }
        }
    }
}

void dsound_stop_nonp() {
    //stops all instances that are not persistent - called in room end
    
    if (SET_PERSISTENCE) {
        SoundInstance* inst;
        
        REPEAT(i, INSTANCE_COUNT) REPEAT(kind, 4) {
            inst = &sound_instances[kind][i];
            if (inst->exists && !inst->persistent) {
                dsound_inst_free(inst);
            }
        }
    }
}

void dsound_kind_stop(int kind) {
    //stops all instances of a kind
    
    REPEAT(i, INSTANCE_COUNT) {
        dsound_inst_free(&sound_instances[kind][i]);
    }
}

void dsound_stop_all() {
    //stops all sounds
    
    REPEAT(i, INSTANCE_COUNT) REPEAT(kind, NUM_KINDS) {
        dsound_inst_free(&sound_instances[kind][i]);
    }
}

#pragma endregion
//---------------------------------------------------------------------------//
#pragma region setters getters


void dsound_set_volume(double vol) {
    VOLUME = min(1.0,max(0.0,vol));
}

void dsound_set_pause(int index, bool paused) {
    int kind, iid;
    SoundInstance* inst;
    
    if (dsound_instance_from_iid(index, &kind, &iid, &inst)) {
        //pause instance
        if (paused) {
            if (inst->playing) {
                dsound_inst_pause(inst);
            }
        } else {
            if (!inst->playing)
                dsound_inst_start(inst);
        }
    } else {
        //pause all of sound
        SoundResource* sound = &sound_resources[index];
        if (sound->exists) {
            kind = sound->kind;
            REPEAT(i, INSTANCE_COUNT) {
                inst = &sound_instances[kind][i];
                if (inst->exists && inst->sound == sound) {
                    if (paused) {
                        if (inst->playing) {
                            vibe_check(inst->buffer->Stop());
                            inst->playing = false;
                        }
                    } else {
                        if (!inst->playing)
                            dsound_inst_start(inst);
                    }
                }
            }
        }
    }
}

void dsound_setpause_all(bool paused) {
    REPEAT(kind, NUM_KINDS) {
        dsound_setpause_kind(kind, paused);
    }
}

void dsound_setpause_kind(int kind, bool paused) {
    //changes the paused state of all sounds of a kind.
    
    SoundInstance* inst;
    
    REPEAT(i, INSTANCE_COUNT) {
        inst = &sound_instances[kind][i];
        if (inst->exists) {
            if (paused) {
                if (inst->playing) {
                    dsound_inst_pause(inst);
                }
            } else {
                if (!inst->playing)
                    dsound_inst_start(inst);
            }
        }
    }
}

int dsound_get_position(SoundInstance* inst) {
    //returns buffer position in samples
    
    return inst->play_cursor;
}

void dsound_set_position(SoundInstance* inst, int position) {
    //sets buffer position in samples
    
    inst->play_cursor = (max(0,min((DWORD)position, inst->sound->length-1)));
}

#pragma endregion
//---------------------------------------------------------------------------//
#pragma region mixer

void dsound_mixer_thread(SoundInstance* inst) {
    //mixes one active instance into its dsound buffer
    
    //restores a lost buffer
        DWORD status;
        inst->buffer->GetStatus(&status);
        if (status == DSERR_BUFFERLOST) {
            inst->buffer->Restore();
        }
    
    
    //get tape position, calculate size to write
        DWORD play_head_bytes, write_head_bytes;
        vibe_check(inst->buffer->GetCurrentPosition(&play_head_bytes, &write_head_bytes));
        
        int play_head = play_head_bytes / 4;
        int write_head;
        
        if (inst->write_cursor == -1) {
            //first write; initialize
            write_head = write_head_bytes / 4;
            inst->write_cursor = write_head;
        } else {
            //resume from the last write head position
            write_head = inst->write_cursor;
        }
        
        if (write_head < play_head) {
            //wrapped around the circular buffer
            write_head += BUFFERLENGTH;
        }
        
        //make sure we don't write too much data
        int samples = min(BUFFERSAFE, play_head + BUFFERSAFE - write_head);        
    
        //make sure we also aren't running too fast
        if (samples <= 0) return;
    
    
    //TIME TO MIX
    
        double
            mix_left,
            mix_right,
            mix_normal = 0;
            
        float* data = inst->sound->buffer;
        int length = inst->sound->length;
        int pc = inst->play_cursor;
        
        bool upkeep_looping = (
            inst->looping && (
                inst->loop_count < inst->loop_c ||
                inst->loop_c == 0
            )
        );
        
        //TODO: this is insanely ugly lol
        int stride = (int)floorpo2(inst->pitch);
        
        REPEAT(sample,samples) {
            mix_left = 0;
            mix_right = 0;
            
            if (pc<length) {                
                REPEAT(stride) {
                    mix_left  += data[pc*2 + 0];
                    mix_right += data[pc*2 + 1];
                    pc += 1;
                    
                    //TODO: backward direction logic
                    if (upkeep_looping && pc >= inst->loop_b) {
                        pc = inst->loop_a;
                        inst->loop_count += 1;
                    }
                }
                
                mix_left /= stride;
                mix_right /= stride;                
                
                /*
                int pc2 = pc*2;
                if (inst->pitch > 8) {
                    //8:1 mix
                    mix_left  = (data[pc2 + 0] + data[pc2 + 2] + data[pc2 + 4] + data[pc2 + 6] + data[pc2 + 8] + data[pc2 + 10] + data[pc2 + 12] + data[pc2 + 14]) * 0.125;
                    mix_right = (data[pc2 + 1] + data[pc2 + 3] + data[pc2 + 5] + data[pc2 + 7] + data[pc2 + 9] + data[pc2 + 11] + data[pc2 + 13] + data[pc2 + 15]) * 0.125;
                    pc += 8;
                } else if (inst->pitch > 4) {
                    //4:1 mix
                    mix_left  = (data[pc2 + 0] + data[pc2 + 2] + data[pc2 + 4] + data[pc2 + 6]) * 0.25;
                    mix_right = (data[pc2 + 1] + data[pc2 + 3] + data[pc2 + 5] + data[pc2 + 7]) * 0.25;
                    pc += 4;
                } else if (inst->pitch > 2) {
                    //2:1 mix
                    mix_left  = (data[pc2 + 0] + data[pc2 + 2]) * 0.5;
                    mix_right = (data[pc2 + 1] + data[pc2 + 3]) * 0.5;
                    pc += 2;
                } else {
                    //1:1 mix
                    mix_left  = data[pc2 + 0];
                    mix_right = data[pc2 + 1];
                    pc += 1;
                }
                if (upkeep_looping && pc >= inst->loop_b)
                    pc -= inst->loop_b - inst->loop_a;
                    inst->loop_count += 1;*/
            } else {
                //TODO: sound data done; mark instance as dying                
            }
            
            TertiaryBuffer[sample*2 + 0] = (int16_t)(32767.0 * mix_left);
            TertiaryBuffer[sample*2 + 1] = (int16_t)(32767.0 * mix_right);
        }
        inst->play_cursor = pc;
        
        if (inst->play_cursor>=inst->sound->length) {
            inst->playing = false;
            //todo: mark it as "done" and wait for the play cursor to hit the write area to stop the buffer
        }
    
    //END TIME TO MIX    
    
    
    //acquire secondary buffer
        void* lock_chunk1;
        DWORD lock_size1;
        void* lock_chunk2;
        DWORD lock_size2;
        
        vibe_check(inst->buffer->Lock(
            inst->write_cursor*4, //in bytes
            samples*4, //in bytes
            &lock_chunk1, &lock_size1,
            &lock_chunk2, &lock_size2,
            0
        ));
        
        inst->write_cursor = (inst->write_cursor + samples) % BUFFERLENGTH;
    
    
    //copy data to both chunks of secondary buffer
        memcpy(lock_chunk1, TertiaryBuffer, lock_size1);
        //note to future renex: Tertiary is u16 so the addition of a lock size in bytes ends up doubled
        //thanks floogle
        if (lock_chunk2 != NULL) 
            memcpy(lock_chunk2, TertiaryBuffer + lock_size1 / 2, lock_size2);
    
    
    //relinquish secondary buffer
        vibe_check(inst->buffer->Unlock(
            lock_chunk1, lock_size1,
            lock_chunk2, lock_size2
        ));
}

//---------------------------------------------------------------------------//