/*
 * audio-session-manager
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Seungbae Shin <seungbae.shin at samsung.com>, Sangchul Lee <sc11.lee at samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _ASM_TYPES_H_
#define _ASM_TYPES_H_

/**
 * @ingroup  AUDIO_SESSION_MANAGER
 * @defgroup AUDIO_SESSION_MANAGER Audio Session Manager
 * @{
 */
#include <stdbool.h>

/* Error codes */
#define ERR_ASM_ERROR_NONE                     0x00
#define ERR_ASM_THREAD_CREATE_ERROR            0x01
#define ERR_ASM_THREAD_CANCEL_ERROR            0x02
#define ERR_ASM_MSG_QUEUE_MSGID_GET_FAILED     0x03
#define ERR_ASM_MSG_QUEUE_SND_ERROR            0x04
#define ERR_ASM_MSG_QUEUE_RCV_ERROR            0x05
#define ERR_ASM_ALREADY_REGISTERED             0x06
#define ERR_ASM_ALREADY_UNREGISTERED           0x07
#define ERR_ASM_EVENT_IS_INVALID               0x08
#define ERR_ASM_LOCAL_HANDLE_IS_FULL           0x09
#define ERR_ASM_LOCAL_HANDLE_IS_INVALID        0x0A
#define ERR_ASM_POLICY_CANNOT_PLAY             0x10
#define ERR_ASM_POLICY_CANNOT_PLAY_BY_CALL     0x11 /* CALL / VIDEOCALL / VOIP */
#define ERR_ASM_POLICY_CANNOT_PLAY_BY_ALARM    0x12 /* ALARM */
#define ERR_ASM_POLICY_CANNOT_PLAY_BY_PROFILE  0x13 /* blocked by sound profile */
#define ERR_ASM_POLICY_CANNOT_PLAY_BY_CUSTOM   0x14 /* blocked by custom reason */
#define ERR_ASM_INVALID_PARAMETER              0x20
#define ERR_ASM_VCONF_ERROR                    0x21
#define ERR_ASM_NOT_SUPPORTED                  0x22
#define ERR_ASM_NO_OPERATION                   0x23
#define ERR_ASM_UNKNOWN_ERROR                  0x2F
#define ERR_ASM_SERVER_HANDLE_IS_FULL          0x30
#define ERR_ASM_SERVER_HANDLE_IS_INVALID       0x31
#define ERR_ASM_WATCH_ALREADY_REQUESTED        0x40
#define ERR_ASM_WATCH_ALREADY_UNREQUESTED      0x41
#define ERR_ASM_WATCH_NOT_SUPPORTED            0x42
#define ERR_ASM_ADD_WATCH_LIST_FAILURE         0x43
#define ERR_ASM_DEL_WATCH_LIST_FAILURE         0x44

/* Session Option */
#define ASM_SESSION_OPTION_PAUSE_OTHERS                  0x0001
#define ASM_SESSION_OPTION_UNINTERRUPTIBLE               0x0002
#define ASM_SESSION_OPTION_RESUME_BY_MEDIA_PAUSED        0x0010
#define ASM_SESSION_OPTION_RESUME_BY_MEDIA_STOPPED       0x0020


#define ASM_PRIORITY_MATRIX_MIN (ASM_EVENT_MAX-1)
#define ASM_PRIORITY_SUB_MATRIX_MIN (ASM_SUB_EVENT_MAX-1)
#define ASM_SERVER_HANDLE_MAX 256
#define ASM_HANDLE_INIT_VAL -1

#define SOUND_STATUS_KEY		"memory/Sound/SoundStatus"
/**
  * This enumeration defines sound request.
  */
typedef enum
{
	ASM_REQUEST_REGISTER = 0,
	ASM_REQUEST_UNREGISTER,
	ASM_REQUEST_GETSTATE,
	ASM_REQUEST_GETMYSTATE,
	ASM_REQUEST_SETSTATE,
	ASM_REQUEST_EMERGENT_EXIT,
	ASM_REQUEST_DUMP,
	ASM_REQUEST_SET_SUBSESSION,
	ASM_REQUEST_GET_SUBSESSION,
	ASM_REQUEST_REGISTER_WATCHER,
	ASM_REQUEST_UNREGISTER_WATCHER,
	ASM_REQUEST_SET_SUBEVENT,
	ASM_REQUEST_GET_SUBEVENT,
	ASM_REQUEST_RESET_RESUME_TAG,
	ASM_REQUEST_SET_SESSION_OPTIONS,
	ASM_REQUEST_GET_SESSION_OPTIONS
} ASM_requests_t;

/**
  * This enumeration defines sound event for Sound Scenario in Multimedia Resources Conflict Manager.
  */
typedef enum
{
	ASM_EVENT_NONE = -1,             // [Notice] Don't use it in application (Never use it), it is internal sound event
	ASM_EVENT_MEDIA_MMPLAYER = 0,
	ASM_EVENT_MEDIA_MMCAMCORDER,
	ASM_EVENT_MEDIA_MMSOUND,
	ASM_EVENT_MEDIA_OPENAL,
	ASM_EVENT_MEDIA_FMRADIO,
	ASM_EVENT_MEDIA_WEBKIT,
	ASM_EVENT_NOTIFY,
	ASM_EVENT_ALARM,
	ASM_EVENT_EARJACK_UNPLUG,
	ASM_EVENT_CALL,
	ASM_EVENT_VIDEOCALL,
	ASM_EVENT_VOIP,
	ASM_EVENT_MONITOR,
	ASM_EVENT_EMERGENCY,
	ASM_EVENT_EXCLUSIVE_RESOURCE,
	ASM_EVENT_VOICE_RECOGNITION,
	ASM_EVENT_MMCAMCORDER_AUDIO,
	ASM_EVENT_MMCAMCORDER_VIDEO,
	ASM_EVENT_MAX
} ASM_sound_events_t;

typedef enum
{
	ASM_SUB_EVENT_NONE = 0,
	ASM_SUB_EVENT_SHARE,
	ASM_SUB_EVENT_EXCLUSIVE,
	ASM_SUB_EVENT_MAX
} ASM_sound_sub_events_t;

typedef enum {
	ASM_SUB_SESSION_TYPE_VOICE = 0,
	ASM_SUB_SESSION_TYPE_RINGTONE,
	ASM_SUB_SESSION_TYPE_MEDIA,
	ASM_SUB_SESSION_TYPE_INIT,
	ASM_SUB_SESSION_TYPE_VR_NORMAL,
	ASM_SUB_SESSION_TYPE_VR_DRIVE,
	ASM_SUB_SESSION_TYPE_RECORD_STEREO,
	ASM_SUB_SESSION_TYPE_RECORD_MONO,
	ASM_SUB_SESSION_TYPE_MAX
} ASM_sound_sub_sessions_t;

/*
 * This enumeration defines event source for sound conflict scenario
 */
typedef enum
{
	ASM_EVENT_SOURCE_MEDIA = 0,
	ASM_EVENT_SOURCE_CALL_START,
	ASM_EVENT_SOURCE_CALL_END,
	ASM_EVENT_SOURCE_EARJACK_UNPLUG,
	ASM_EVENT_SOURCE_RESOURCE_CONFLICT,
	ASM_EVENT_SOURCE_ALARM_START,
	ASM_EVENT_SOURCE_ALARM_END,
	ASM_EVENT_SOURCE_EMERGENCY_START,
	ASM_EVENT_SOURCE_EMERGENCY_END,
	ASM_EVENT_SOURCE_OTHER_PLAYER_APP,
	ASM_EVENT_SOURCE_NOTIFY_START,
	ASM_EVENT_SOURCE_NOTIFY_END
} ASM_event_sources_t;

/**
  * This enumeration defines sound case between playing sound and request sound
  */
typedef enum
{
	ASM_CASE_NONE = 0,
	ASM_CASE_1PLAY_2STOP,
	ASM_CASE_1STOP_2PLAY,
	ASM_CASE_1PAUSE_2PLAY,
	ASM_CASE_1PLAY_2PLAY_MIX,
	ASM_CASE_RESOURCE_CHECK,
	ASM_CASE_SUB_EVENT,
} ASM_sound_cases_t;


/*
 * This enumeration defines Sound Playing Status Information
 * Each bit is Sound Status Type( 0 : None , 1 : Playing)
 */
typedef enum
{
	ASM_STATUS_NONE                = 0x00000000,
	ASM_STATUS_MEDIA_MMPLAYER      = 0x00000001,
	ASM_STATUS_MEDIA_MMCAMCORDER   = 0x00000002,
	ASM_STATUS_MEDIA_MMSOUND       = 0x00000004,
	ASM_STATUS_MEDIA_OPENAL        = 0x00000008,
	ASM_STATUS_MEDIA_FMRADIO       = 0x00000010,
	ASM_STATUS_MEDIA_WEBKIT        = 0x00000020,
	ASM_STATUS_NOTIFY              = 0x00000040,
	ASM_STATUS_ALARM               = 0x00000080,
	ASM_STATUS_EARJACK_UNPLUG      = 0x00001000,
	ASM_STATUS_CALL                = 0x10000000,
	ASM_STATUS_VIDEOCALL           = 0x20000000,
	ASM_STATUS_VOIP                = 0x40000000,
	ASM_STATUS_MONITOR             = 0x80000000,
	ASM_STATUS_EMERGENCY           = 0x00002000,
	ASM_STATUS_EXCLUSIVE_RESOURCE  = 0x00004000,
	ASM_STATUS_VOICE_RECOGNITION   = 0x00010000,
	ASM_STATUS_MMCAMCORDER_AUDIO   = 0x00020000,
	ASM_STATUS_MMCAMCORDER_VIDEO   = 0x00040000
} ASM_sound_status_t;


/**
  * This enumeration defines sound state.
  */
typedef enum
{
	ASM_STATE_NONE         = 0,
	ASM_STATE_PLAYING      = 1,
	ASM_STATE_WAITING      = 2,
	ASM_STATE_STOP         = 3,
	ASM_STATE_PAUSE        = 4
} ASM_sound_states_t;


/*
 * This enumeration defines resume state
 */
typedef enum
{
	ASM_NEED_NOT_RESUME      = 0,
	ASM_NEED_RESUME          = 1
}ASM_resume_states_t;
/*
 * This enumeration defines state return of client.
 */
typedef enum
{
	ASM_CB_RES_IGNORE   = -1,
	ASM_CB_RES_NONE     = 0,
	ASM_CB_RES_PLAYING  = 1,
	ASM_CB_RES_STOP     = 2,
	ASM_CB_RES_PAUSE    = 3,
}ASM_cb_result_t;

/*
 * This enumeration defines type of multimedia resource
 */

typedef enum
{
	ASM_RESOURCE_NONE           = 0x0000,
	ASM_RESOURCE_CAMERA         = 0x0001,
	ASM_RESOURCE_VIDEO_OVERLAY  = 0x0002,
	ASM_RESOURCE_STREAMING      = 0x0004,
	ASM_RESOURCE_HW_DECODER     = 0x0100,
	ASM_RESOURCE_HW_ENCODER     = 0x0200,
	ASM_RESOURCE_RADIO_TUNNER   = 0x1000,
	ASM_RESOURCE_TV_TUNNER      = 0x2000,
	ASM_RESOURCE_VOICECONTROL   = 0x10000,
}ASM_resource_t;


/* Sound command for applications */
typedef enum
{
	ASM_COMMAND_NONE   = 0,
	ASM_COMMAND_PLAY   = 2,
	ASM_COMMAND_STOP   = 3,
	ASM_COMMAND_PAUSE  = 4,
	ASM_COMMAND_RESUME = 5,
} ASM_sound_commands_t;

/**
  * This structure defines the message data from library to conflict manager.
  */
typedef struct
{
	int					handle;
	ASM_requests_t				request_id;
	ASM_sound_events_t			sound_event;
	ASM_sound_states_t			sound_state;
	ASM_resource_t				system_resource;
} __ASM_msg_data_lib_to_asm_t;

/**
  * This structure defines the message data from conflict manager to library.
  */
typedef struct
{
	int					alloc_handle;
	int 					cmd_handle;
	ASM_sound_commands_t			result_sound_command;
	ASM_sound_states_t			result_sound_state;
	ASM_requests_t				source_request_id;
	int						option_flags;
	int						error_code;
} __ASM_msg_data_asm_to_lib_t;

/**
  * This structure defines the message data from conflict manager to library.
  */
typedef struct
{
	int 						handle;
	ASM_sound_commands_t      result_sound_command;
} __ASM_msg_data_asm_to_cb_t;

/**
  * This structure defines the message from library to conflict manager.
  */
typedef struct
{
	long int                    	instance_id;
	__ASM_msg_data_lib_to_asm_t    data;
} ASM_msg_lib_to_asm_t;

/**
  * This structure defines the message from conflict manager to library.
  */
typedef struct
{
	long int                        instance_id;
	__ASM_msg_data_asm_to_lib_t	data;
} ASM_msg_asm_to_lib_t;

/**
  * This structure defines the message from conflict manager to library.
  */
typedef struct
{
	long int            			instance_id;
	__ASM_msg_data_asm_to_cb_t     data;
} ASM_msg_asm_to_cb_t;


typedef struct
{
	ASM_sound_events_t		sound_event;
	ASM_sound_status_t		sound_status;
}ASM_sound_event_type_t;

static const ASM_sound_event_type_t ASM_sound_type[] = {
    { ASM_EVENT_NONE, 					ASM_STATUS_NONE },
    { ASM_EVENT_MEDIA_MMPLAYER, 			ASM_STATUS_MEDIA_MMPLAYER },
    { ASM_EVENT_MEDIA_MMCAMCORDER, 		ASM_STATUS_MEDIA_MMCAMCORDER },
    { ASM_EVENT_MEDIA_MMSOUND, 			ASM_STATUS_MEDIA_MMSOUND },
    { ASM_EVENT_MEDIA_OPENAL, 			ASM_STATUS_MEDIA_OPENAL },
    { ASM_EVENT_MEDIA_FMRADIO, 			ASM_STATUS_MEDIA_FMRADIO },
    { ASM_EVENT_MEDIA_WEBKIT, 			ASM_STATUS_MEDIA_WEBKIT },
    { ASM_EVENT_NOTIFY, 				ASM_STATUS_NOTIFY },
    { ASM_EVENT_ALARM, 					ASM_STATUS_ALARM },
    { ASM_EVENT_EARJACK_UNPLUG,			ASM_STATUS_EARJACK_UNPLUG },
    { ASM_EVENT_CALL, 					ASM_STATUS_CALL },
    { ASM_EVENT_VIDEOCALL, 				ASM_STATUS_VIDEOCALL },
    { ASM_EVENT_VOIP, 					ASM_STATUS_VOIP },
    { ASM_EVENT_MONITOR,				ASM_STATUS_MONITOR },
    { ASM_EVENT_EMERGENCY,				ASM_STATUS_EMERGENCY },
    { ASM_EVENT_EXCLUSIVE_RESOURCE,		ASM_STATUS_EXCLUSIVE_RESOURCE },
    { ASM_EVENT_VOICE_RECOGNITION,		ASM_STATUS_VOICE_RECOGNITION },
    { ASM_EVENT_MMCAMCORDER_AUDIO,		ASM_STATUS_MMCAMCORDER_AUDIO },
    { ASM_EVENT_MMCAMCORDER_VIDEO,		ASM_STATUS_MMCAMCORDER_VIDEO }
};


/**
 * This callback function is called when sound status of other sound event is changed
 *
 * @return							No return value
 * @param[in]	sound_status		Argument passed when callback was set
 */
typedef ASM_cb_result_t	(*ASM_sound_cb_t) (int handle, ASM_event_sources_t event_source, ASM_sound_commands_t command, unsigned int sound_status, void* cb_data);


/**
 * This callback function is called when sound status of other sound event and state that you want to watch is changed
 *
 * @return							No return value
 * @param[in]	sound_status		Argument passed when callback was set
 */
typedef ASM_cb_result_t	(*ASM_watch_cb_t) (int handle, ASM_sound_events_t sound_event, ASM_sound_states_t sound_state, void* cb_data);

#endif
