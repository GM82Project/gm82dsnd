//---------------------------------------------------------------------------//
//internals


#define __gm82dsound_gml_init
    globalvar __gm82dsound_version; __gm82dsound_version=010
    
    object_event_add(gm82core_object,ev_step,ev_step_end,"__dsound_update(1000/room_speed)")
    object_event_add(gm82core_object,ev_other,ev_room_end,"__dsound_roomend()")
    
    globalvar __dsound_error,__dsound_map,__dsound_rev_map,__dsound_prs_map,__dsound_search_dir;
    
    __dsound_error[1]="Generic DirectSound error. Please tell renex about this."
    __dsound_error[2]="Non-existing sound or instance index."
    __dsound_error[3]="Failure loading sound data from file or buffer."
    __dsound_error[4]="No more space to add sounds (100000 sounds). Check if you have a memory leak, otherwise if this is happening due to external asset loading, please enable dsound_reuse_sound_ids using sound_settings."
    
    __dsound_map=ds_map_create()
    __dsound_rev_map=ds_map_create()
    __dsound_prs_map=ds_map_create()
    __dsound_search_dir=""


#define __dsound_roomend
    //stop all non-persistent instances
    __dsound_stop_nonpersist()


#define __dsound_name_parser
    //(index,funcname,noerror)
    //converts a string name to sound index,
    //for "filename" sound id support à la 8.2 Sound
    
    if (is_string(argument0)) {
        if (ds_map_exists(__dsound_map,argument0)) return ds_map_find_value(__dsound_map,argument0)
        if (!argument2) __dsound_error(argument1,"Sound name ("+string(argument0)+") doesn't exist.")
        return noone
    }
    
    if (!sound_exists(argument0)) {
        if (!argument2) __dsound_error(argument1,"Sound index ("+string(argument0)+") doesn't exist.")
        return noone
    }
    
    return argument0


#define __dsound_error_effects
    //show_error("8.2 DirectSound error: Effects are not currently available.",false)


#define __dsound_error
    show_error("8.2 DirectSound error: "+chr(13)+chr(10)+"In function "+argument0+": "+argument1,0)


//---------------------------------------------------------------------------//
//shims


#define sound_add
    ///sound_add(fname,kind,preload)
    
    var __index,__name;
    
    if (!file_exists(argument0)) {
        __dsound_error("sound_add","File ("+string(argument0)+") doesn't exist.")
        return noone
    }
    
    if (argument1<0 or argument1>7) {
        __dsound_error("sound_add","Invalid kind ("+string(argument1)+").")
        return noone
    }
    
    if (__dsound_search_dir!="")
        __index=__dsound_add_file(__dsound_search_dir+argument0,argument1)
    else
        __index=__dsound_add_file(argument0,argument1)
    
    if (__index<0) {
        __dsound_error("sound_add",__dsound_error[-__index])
        return noone
    } else {
        __name=filename_change_ext(filename_name(argument0),"")
        ds_map_add(__dsound_map,__name,__index)
        ds_map_add(__dsound_rev_map,__index,__name)
    }
    
    return __index


#define sound_background_tempo
    ///sound_background_tempo(factor)
    

#define sound_exists
    ///sound_exists(ind)
    
    if (is_string(argument0)) {
        return ds_map_exists(__dsound_map,argument0)
    }
    return __dsound_exists(argument0)


#define sound_get_kind
    ///sound_get_kind(ind)  
    
    return __dsound_getter(__dsound_name_parser(argument0,"sound_get_kind",false),__dsound_etter_kind)


#define sound_get_name
    ///sound_get_name(ind)
    
    if (is_string(argument0)) {
        if (ds_map_exists(__dsound_map,argument0))
            return argument0
    } else {
        if (ds_map_exists(__dsound_rev_map,argument0))
            return ds_map_find_value(__dsound_rev_map,argument0)
    }
    return ""


#define sound_get_preload
    ///sound_get_preload(ind)
    
    return __dsound_getter(__dsound_name_parser(argument0,"sound_get_preload",false),__dsound_etter_preload)


#define sound_global_volume
    ///sound_global_volume(value)
    
    __dsound_glob_vol(argument0)


#define sound_loop
    ///sound_loop(index)
    
    return __dsound_play(__dsound_name_parser(argument0,"sound_loop",false),1,1,0,1,0)


#define sound_play
    ///sound_play(index)   
    
    return __dsound_play(__dsound_name_parser(argument0,"sound_play",false),0,1,0,1,0)


#define sound_isplaying
    ///sound_isplaying(index)
    
    return __dsound_insts(__dsound_name_parser(argument0,"sound_isplaying",true))


#define sound_volume
    ///sound_volume(index,value)
    
    __dsound_setter(__dsound_name_parser(argument0,"sound_volume",false),0,argument1)


#define sound_pan
    ///sound_pan(index,value)
    
    __dsound_setter(__dsound_name_parser(argument0,"sound_pan",false),1,argument1)


#define sound_fade
    ///sound_fade(index,value,time)
    

#define sound_stop
    ///sound_stop(index)
    
    __dsound_stop(__dsound_name_parser(argument0,"sound_stop",true))

    
#define sound_stop_all
    ///sound_stop_all()

    __dsound_stop_all()


#define sound_delete
    ///sound_delete(index)
    
#define sound_discard
    ///sound_discard(index)
    
#define sound_replace
    ///sound_replace(index,fname,kind,preload)
    
#define sound_restore
    ///sound_restore(index)
    
#define sound_set_search_directory
    ///sound_set_search_directory(dir)
    
    __dsound_search_dir=string_replace_all(string(argument0),"/","\")
    if (__dsound_search_dir!="")
        if (!string_ends_with(__dsound_search_dir,"\"))
            __dsound_search_dir+="\"



#define sound_effect_chorus
    __dsound_error_effects()
#define sound_effect_compressor
    __dsound_error_effects()
#define sound_effect_echo
    __dsound_error_effects()
#define sound_effect_equalizer
    __dsound_error_effects()
#define sound_effect_flanger
    __dsound_error_effects()
#define sound_effect_gargle
    __dsound_error_effects()
#define sound_effect_reverb
    __dsound_error_effects()
#define sound_effect_set
    __dsound_error_effects()

#define sound_3d_set_sound_cone

#define sound_3d_set_sound_distance

#define sound_3d_set_sound_position

#define sound_3d_set_sound_velocity


//---------------------------------------------------------------------------//
//new api


#define sound_settings
    ///sound_settings(setting,value)
    //Changes extension configuration. All settings are on by default. Turning all settings off emulates vanilla Game Maker behavior.
    //dsound_use_linear_volume - Game Maker's volume scale is a logarithmic attenuation value, from 30 to 100, where half loudness is somewhere around 85, and 60 is inaudible. Our extension instead uses a more intuitive linear volume scale where 50 is half as loud, and 0 is inaudible. If your project uses logarithmic volume, you can disable this option to restore the vanilla volume scale. Note that the volume value from the slider in the sound resource window is always logarithmic.
    //dsound_use_scheduler - Game maker sound functions act immediately upon call. Sometimes this is undesirable, such as when you want to play a sound and then immediately change the settings for it somewhere else within the same frame - if your game is laggy, you could hear a spike as the sound plays at full volume for a very short period of time. In order to mitigate this, our extension uses a system where newly played sounds and changes to sound instances are only executed once per step, in a way where sound operations are more consistent and predictable. Turning this option off will instead apply sound operations immediately.
    //dsound_reuse_sound_ids - Normally, Game Maker assigns incrementing ids to newly added sounds, but this means you will eventually run out of space for sounds at 100000 where our extension's instance ids start. Here we provide an option to reuse dead sound indexes for newly added resources. If your code is not designed to handle that, you can disable this option to use incrementing ids only and leave deleted sounds permanently deleted. Additionally, sound resource id 0 is never used.
    
    __dsound_settings(argument0,argument1)


#define sound_add_ext
    ///sound_add_ext(fname,kind,name,vol,pan,pitch,persistent)
    //Adds a sound, and sets its internal name and properties.
    var __snd;
    
    if (!file_exists(argument0)) {
        __dsound_error("sound_add_ext","File ("+string(argument0)+") doesn't exist.")
        return noone
    }
    
    if (argument1<0 or argument1>7) {
        __dsound_error("sound_add_ext","Invalid kind ("+string(argument1)+").")
        return noone
    }
    
    __snd=sound_add(argument0,argument1,1)
    
    sound_set_properties(__snd,argument3,argument4,argument5,argument6)
    
    return __snd
    
    
#define sound_add_included
    ///sound_add_included(fname,kind)
    //Adds a sound from an included file.
    
    var __fname;
    
    if (argument1<0 or argument1>7) {
        __dsound_error("sound_add_included","Invalid kind ("+string(argument1)+").")
        return noone
    }    
    
    __fname=temp_directory+"\gm82\sound\"+argument0
    export_include_file_location(argument0,__fname)
    
    if (!file_exists(__fname)) {
        __dsound_error("sound_add_included","Included file ("+string(argument0)+") failed to export.")
        return noone
    }
    
    return sound_add(__fname,argument1,1)


#define sound_add_included_ext
    ///sound_add_included_ext(fname,kind,name,vol,pan,pitch,persistent)
    //Adds a sound from an included file.
    
    var __fname,__snd;
    
    if (argument1<0 or argument1>7) {
        __dsound_error("sound_add_included_ext","Invalid kind ("+string(argument1)+").")
        return noone
    }     
    
    __fname=temp_directory+"\gm82\sound\"+argument0
    export_include_file_location(argument0,__fname)
    
    if (!file_exists(__fname)) {
        __dsound_error("sound_add_included_ext","Included file ("+string(argument0)+") failed to export.")
        return noone
    }
    
    __snd=sound_add(__fname,argument1,1)
    
    sound_set_properties(__snd,argument3,argument4,argument5,argument6)
    
    return __snd


#define sound_add_directory

#define sound_set_properties
    ///sound_set_properties(index,vol,pan,pitch,persistent)
    //Sets all properties of a sound at once.
    
    if (!sound_exists(argument0)) {
        __dsound_error("sound_set_properties","Sound ("+string(argument0)+") doesn't exist.")
        exit
    }
    
    sound_set_name(argument0,argument1)
    sound_volume(argument0,argument2)
    sound_pan(argument0,argument3)
    sound_pitch(argument0,argument4)
    sound_set_persistent(argument0,argument5)


#define sound_set_name
    ///sound_set_name(index,name)
    //index: old sound name, or resource index
    //name: new name to use
    //Renames a sound such that it can be addressed by the new name string.
    
    var __index,__name;
    
    __index=__dsound_name_parser(argument0,"sound_set_name",false)
    
    if (__index!=noone) {    
        __name=ds_map_find_value(__dsound_rev_map,__index)
        
        if (ds_map_exists(__dsound_map,string(argument1))) {
            __dsound_error("sound_set_name","Trying to rename sound ("+__name+"), but new name ("+argument1+") already exists.")
            exit
        }
        
        ds_map_delete(__dsound_map,__name)
        ds_map_delete(__dsound_rev_map,__index)
        
        __name=string(argument1)
        ds_map_add(__dsound_map,__name,__index)
        ds_map_add(__dsound_rev_map,__index,__name)
    }


#define sound_set_persistent
    ///sound_set_persistent(index,persistent)
    
    var __index;
    __index=__dsound_name_parser(argument0,"sound_set_persistent",false)
    if (__index!=noone) {
        ds_map_set(__dsound_prs_map,__index,!!argument1)
    }


#define sound_loop_ext
    ///sound_loop_ext(index,vol,pan,pitch,paused,single)
    
    var __snd;
    
    __snd=__dsound_name_parser(argument0,"sound_loop_ext",false)
    
    if (argument5) __dsound_stop(__snd)
    return __dsound_play(__snd,1,argument1,argument2,argument3,argument4)


#define sound_play_ext
    ///sound_play_ext(index,vol,pan,pitch,paused,single)
    
    var __snd;
    
    __snd=__dsound_name_parser(argument0,"sound_play_ext",false)
    
    if (argument5) __dsound_stop(__snd)
    return __dsound_play(__snd,0,argument1,argument2,argument3,argument4)


#define sound_play_single
    ///sound_play_single(index)
    //Plays a sound, ensuring only one copy of it is playing at a time.
    
    sound_stop(argument0)
    return sound_play(argument0)
    
    
#define sound_loop_single
    ///sound_loop_single(index)
    //Loops a sound, ensuring only one copy of it is playing at a time.
    
    sound_stop(argument0)
    return sound_loop(argument0)


#define sound_background_instance
    ///sound_background_instance()
    //Returns the instance id of the currently playing background music, or noone if there isn't one.
    //If there are multiple layers of music currently active, the function returns the newest one.
    
    return __dsound_getbgid()


#define sound_pitch
    ///sound_pitch(index,value)
    
    __dsound_setter(__dsound_name_parser(argument0,"sound_pitch",false),__dsound_etter_pitch,argument1)


#define sound_get_frequency
    ///sound_get_frequency(ind)

    return __dsound_getter(__dsound_name_parser(argument0,"sound_get_frequency",false),__dsound_etter_frequency)


#define sound_get_instance_count

#define sound_get_instance_list

#define sound_get_loop_a
    ///sound_get_loop_a(ind)
    
    return __dsound_getter(__dsound_name_parser(argument0,"sound_get_loop_a",false),__dsound_etter_loopa)


#define sound_get_loop_b
    ///sound_get_loop_b(ind)
    
    return __dsound_getter(__dsound_name_parser(argument0,"sound_get_loop_b",false),__dsound_etter_loopb)


#define sound_get_volume
    ///sound_get_volume(ind)
    
    return __dsound_getter(__dsound_name_parser(argument0,"sound_get_volume",false),__dsound_etter_volume)


#define sound_get_pan
    ///sound_get_pan(ind)
    
    return __dsound_getter(__dsound_name_parser(argument0,"sound_get_pan",false),__dsound_etter_pan)


#define sound_get_pitch
    ///sound_get_pitch(ind)
    
    return __dsound_getter(__dsound_name_parser(argument0,"sound_get_pitch",false),__dsound_etter_pitch)


#define sound_get_length
    ///sound_get_length(ind,[unit])
    
    var __len;
    
    __len=__dsound_getter(__dsound_name_parser(argument0,"sound_get_length",false),__dsound_etter_length)
    
    if (argument_count<2) return __len/__dsound_getter(__snd,__dsound_etter_frequency)
    
    switch (argument1) {
        case unit_samples: return __len
        case unit_seconds: return __len/__dsound_getter(__snd,__dsound_etter_frequency)
        case unit_unitary: {__dsound_error("sound_get_length","unit_unitary is not valid for this function.") return noone}
        default: {__dsound_error("sound_get_length","("+string(argument1)+") is not a valid unit type.") return noone}
    }


#define sound_set_loop
    ///sound_set_loop(index,start,end,count,unit)
    //index: sound instance
    //start: start of the looping region
    //end: end of the looping region
    //count: how many times to loop, or 0 for infinite
    //unit: one of the unit_ constants
    //Sets the loop points to use when looping a sound.
    //You can pass in 'noone' or a negative value for the endpoint to use the end of the file.

    var __snd,__samples,__rcp,__a,__b;
    
    __snd=__dsound_name_parser(argument0,"sound_set_loop",false)
    if (__snd<0) exit
    
    __samples=sound_get_length(__snd,unit_samples)
    __rcp=sound_get_frequency(__snd)/sound_get_length(__snd,argument4)
    
    __a=round(argument1*__rcp)
    
    if (argument2>argument1)
        __b=round(argument2*__rcp)
    else
        __b=__samples
        
    __dsound_loopsetter(
        __snd,
        clamp(__a,0,__samples),
        clamp(__b,0,__samples),
        max(0,floor(argument3)
    )


#define sound_get_pos
    ///sound_get_pos(index)

    
#define sound_set_pos
    ///sound_set_pos(index,pos)


#define sound_pause
    ///sound_pause(index)
    //Pauses a sound instance. If a sound index is passed, all instances of the sound will be paused.
    __dsound_setpause(__dsound_name_parser(argument0,"sound_pause",false),1)


#define sound_resume
    ///sound_resume(index)
    //Resumes a sound instance. If a sound index is passed, all instances of the sound will be resumed.
    __dsound_setpause(__dsound_name_parser(argument0,"sound_resume",false),0)


#define sound_pause_all

#define sound_resume_all
//
//
