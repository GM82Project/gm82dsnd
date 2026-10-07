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

#define REPEAT(x,n) for (int x = 0; x < (n); ++x)

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
    LPDIRECTSOUND8 Device;
    LPDIRECTSOUNDBUFFER PrimaryBuffer;
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
        LPDIRECTSOUNDBUFFER buffer;
        int index;
        int inst_count;
        int frequency;
        int kind;
        int loop_a;
        int loop_b;
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
        LPDIRECTSOUNDBUFFER clone_buffer;
        int index;
        int fade_length;
        int fade_amount;
        int age;
        int posmul;
        int loop_a;
        int loop_b;
        float volume;
        float pan;
        float pitch;
        float volume_from;
        float volume_to;
        bool exists = false;
        bool playing;
        bool looping;        
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
    
    bool SET_LIN_VOLUME = true;
    bool SET_SCHEDULER = true;
    bool SET_REUSE_SNDIDS = true;
    bool SET_PERSISTENCE = true;


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

//adding sounds
    void dsound_load_builtin(int);
    int dsound_add_file(char*, int);
    int dsound_add_mem(uint8_t*, int, int);
    int dsound_add_mem_index(int, uint8_t*, int, int);

//instance control
    int dsound_play(int, bool, double, double, double);
    void dsound_inst_start(SoundInstance*);
    void dsound_inst_stop(int);
    void dsound_inst_free(SoundInstance*);
    void dsound_sound_stop(int);
    void dsound_stop_nonp();
    void dsound_stop_all();

//setters getters
    void dsound_set_volume(double);
    void dsound_set_pause(int, bool);

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
    IPropertyStore* store = nullptr;
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
    if (index < 0) return 0;
    if (index >= RESOURCE_COUNT) {
        dsound_inst_stop((int)index);
        return 0;
    }
    dsound_sound_stop((int)index);
    return 0;
}

GMREAL __dsound_glob_vol(double vol) {
    dsound_set_volume(vol);
    return 0;
}

GMREAL __dsound_exists(double index) {
    if (index < 0) return 0;
    if (index >= RESOURCE_COUNT) {
        int kind, iid;
        SoundInstance* inst = NULL;
        if (dsound_instance_from_iid((int)index, &kind, &iid, &inst)) {
            return inst -> exists?1:0;
        }
        return 0;
    }
    return sound_resources[(int)index].exists?1:0;
}

GMREAL __dsound_setter(double index, double op, double value) {
    if (index < 0) return 0;
    
    if (index >= RESOURCE_COUNT) {
        int kind, iid;
        SoundInstance* inst = NULL;
        if (dsound_instance_from_iid((int)index, &kind, &iid, &inst)) {
            switch ((int)op) {
                case 0: inst->volume = value; break;
                case 1: inst->pan    = value; break;
                case 2: inst->pitch  = value; break;
                case 5: inst->loop_a = (int)value; break;
                case 6: inst->loop_b = (int)value; break;
            }
        }
        
        return 0;
    }
    
    if (!sound_resources[(int)index].exists) return 0;
    
    switch ((int)op) {
        case 0: sound_resources[(int)index].volume = value; break;
        case 1: sound_resources[(int)index].pan    = value; break;
        case 2: sound_resources[(int)index].pitch  = value; break;
        case 5: sound_resources[(int)index].loop_a = (int)value; break;
        case 6: sound_resources[(int)index].loop_b = (int)value; break;
    }
    
    return 0;
}

GMREAL __dsound_getter(double index, double op) {
    if (index < 0) return 0;
    
    if (index >= RESOURCE_COUNT) {
        int kind, iid;
        SoundInstance* inst = NULL;
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
    dsound_set_pause((int)index,pause>0.5);
    return 0;
}

GMREAL __dsound_getbgid() {
    int kind,index;
    SoundInstance* inst = NULL;
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
    }    
    return 0;
}

#pragma endregion
//---------------------------------------------------------------------------//
#pragma region internals


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
        
        vibe_check(DirectSoundCreate8(NULL, &Device, NULL));
        
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
    //thread; use THREAD_MS increments
    
    //todo: update all loop points and instance fading here
}

void dsound_frame_update(int frame_ms) {
    //gml frame
    //update instance life and cleanup here
    
    SoundInstance* inst;
    
    REPEAT(i, INSTANCE_COUNT) REPEAT(kind, NUM_KINDS) {
        inst = &sound_instances[kind][i];
        if (inst->exists) {            
            inst->age++;
            if (SET_SCHEDULER) {
                //if the scheduler is enabled, check that
                if (inst->scheduled) {
                    inst->scheduled = false;
                    dsound_inst_start(inst);
                }
            }
            if (inst->playing && !inst->looping) {
                DWORD status = 0;
                inst->clone_buffer->GetStatus(&status);
                if (!(status & DSBSTATUS_PLAYING)) {
                    dsound_inst_free(inst);
                }
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
        SoundInstance* inst = NULL;
        if (dsound_instance_from_iid(unknown_id, &kind, &iid, &inst)) {
            return inst -> sound -> index;
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
    
    return (LONG)(3333.3 * log10(max(0.001,min(1.0,vol))));
}

LONG dsound_pan_formula(double pan) {
    //decode log volume used by directsound
    
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
        
        sound_resources[index].volume = pow(
            10.0,
            (sound->volume) * 3.0 - 1.0
        ) / 100.0;
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
        sound_resources[index].volume = pow(10.0,(sound->volume)*3.0-1.0)/100.0;
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
    int samplerate, channels, bits;
    uint32_t data_length;
    uint8_t* data;
    int mode;
    
    //debug_message(L"loading sound %i",id);
    
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
            &&  data-buffer < length - 16
            );
            data += 4;            
    
        samplerate = format->SampleRate;
        channels = format->Channels;
        bits = format->BitsPerSample;
    } else if (memcmp("Ogg",buffer,3)==0) {
        //read ogg
        mode = 1;

        int16_t* ogg_data = NULL;
        data_length = stb_vorbis_decode_memory(
            (const uint8_t*)buffer, length,
            &channels, &samplerate, &ogg_data
        );
        
        if (data_length <= 0) return ERROR_FAIL_LOAD;
        
        data = (uint8_t*)ogg_data;
        data_length *= channels * 2; //16 bit, but the buffer is char*
        bits = 16;
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
        
        data = (uint8_t*)info.buffer;
        data_length = info.samples * 2; //16 bit, but the buffer is char*
        samplerate = info.hz;
        channels = info.channels;
        bits = 16;        
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
    }
    
    //debug_message(L"sample rate %i",samplerate);
    //debug_message(L"channels %i",channels);
    //debug_message(L"bits %i",bits);        
    //debug_message(L"data length %i",data_length);
    
    //create the secondary buffer and fill it with pcm data
        vibe_check(Device->CreateSoundBuffer(
            describe_buffer(
                DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY,
                describe_format(samplerate, channels, bits),
                data_length
            ),
            &secbuffer,
            NULL
        ));
        
        void* lock_chunk;
        DWORD lock_size;
        vibe_check(secbuffer->Lock(
            0, data_length,
            &lock_chunk, &lock_size,
            NULL, NULL, 0
        ));
        
        memcpy(lock_chunk, data, lock_size);
        
        vibe_check(secbuffer->Unlock(
            lock_chunk, lock_size,
            NULL, NULL
        ));
    
    //create the sound resource
        SoundResource* sound = &sound_resources[id];
        sound->index = id;
        sound->buffer = secbuffer;
        sound->kind = kind;
        sound->posmul = channels * (bits / 8);
        sound->length = data_length / sound->posmul;
        sound->loop_a = 0;
        sound->loop_b = sound->length;
        sound->exists = true;
        sound->loaded = true;
        sound->persistent = false;
        sound->frequency = samplerate;
        sound->volume = 1.0;
        sound->pan = 0.0;
        sound->pitch = 1.0;
        sound->inst_count = 0;
    
    if (mode != 0) {
        //clean up temporary buffer from compressed types
        free(data);
    }
    
    return id;
}

#pragma endregion
//---------------------------------------------------------------------------//
#pragma region instance control


int dsound_play(int index, bool loop, double vol, double pan, double pitch) {
    SoundResource* sound = &sound_resources[index];
    if (!sound->exists) return ERROR_NON_EXIST;
    
    LPDIRECTSOUNDBUFFER clone;
    vibe_check(Device->DuplicateSoundBuffer(sound->buffer, &clone));
    
    double volume_final = sound->volume * vol * VOLUME;
    double pan_final = sound->pan + pan;
    double pitch_final = sound->pitch * pitch;
    
    clone->SetVolume(dsound_volume_formula(volume_final));
    clone->SetPan(dsound_pan_formula(pan_final));
    clone->SetFrequency((DWORD)(pitch_final * sound->frequency));

    int kind = sound->kind;
    
    if (kind == 1) {
        dsound_inst_stop(BGM_INST_ID);
        BGM_INST_ID = LAST_INST_ID;
    }
    if (kind == 3) {
        dsound_inst_stop(MM_INST_ID);
        MM_INST_ID = LAST_INST_ID;
    }
    
    SoundInstance* inst = &sound_instances[kind][dsound_get_free_instance(kind)];
    inst->sound = sound;
    inst->clone_buffer = clone;
    inst->index = LAST_INST_ID;
    inst->volume = volume_final;
    inst->pan = pan_final;
    inst->pitch = pitch_final;
    inst->volume_from = volume_final;
    inst->volume_to = volume_final;
    inst->loop_a = sound->loop_a;
    inst->loop_b = sound->loop_b;
    inst->fade_length = 0;
    inst->fade_amount = 0;
    inst->age = 0;
    inst->exists = true;
    inst->playing = true;
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

void dsound_inst_start(SoundInstance* inst) {
    if (inst->looping) {
        vibe_check(
            inst->clone_buffer->Play(0, 0, DSBPLAY_LOOPING)
        );
    } else {
        vibe_check(
            inst->clone_buffer->Play(0, 0, 0)
        );
    }
}

void dsound_inst_stop(int iid) {
    //stops an instance given its instance id
    
    int kind,index;
    SoundInstance* inst = NULL;
    
    if (dsound_instance_from_iid(iid, &kind, &index, &inst)) {
        dsound_inst_free(inst);
    }
}

void dsound_inst_free(SoundInstance* inst) {
    //frees an instance by reference
    
    if (inst->exists) {
        inst->sound->inst_count--;
        inst->clone_buffer->Stop();
        inst->clone_buffer->Release();
        inst->exists = false;
    }
}

void dsound_sound_stop(int index) {
    //stops all instances of a sound
    
    SoundResource* sound = &sound_resources[index];
    
    SoundInstance* inst;
    
    REPEAT(i, INSTANCE_COUNT) REPEAT(kind,4) {
        inst = &sound_instances[kind][i];
        if (inst->exists && inst->sound == sound) {
            dsound_inst_free(inst);
        }
    }
}

void dsound_stop_nonp() {
    //stops all instances that are not persistent - called in room end
    
    if (SET_PERSISTENCE) {
        SoundInstance* inst;
        
        REPEAT(i, INSTANCE_COUNT) REPEAT(kind,4) {
            inst = &sound_instances[kind][i];
            if (inst->exists && !inst->persistent) {
                dsound_inst_free(inst);
            }
        }
    }
}

void dsound_stop_all() {
    //stops all sounds.
    
    REPEAT(i, INSTANCE_COUNT) REPEAT(kind,NUM_KINDS) {
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
    //etc
}

#pragma endregion
//---------------------------------------------------------------------------//