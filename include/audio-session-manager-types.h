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
#define ERR_ASM_THREAD_CREATE_ERROR 		0x01
#define ERR_ASM_THREAD_CANCEL_ERROR 		0x02
#define ERR_ASM_MSG_QUEUE_MSGID_GET_FAILED 	0x03
#define ERR_ASM_MSG_QUEUE_SND_ERROR 		0x04
#define ERR_ASM_MSG_QUEUE_RCV_ERROR 		0x05
#define ERR_ASM_ALREADY_REGISTERED 		0x06
#define ERR_ASM_ALREADY_UNREGISTERED 		0x07
#define ERR_ASM_EVENT_IS_INVALID 		0x08
#define ERR_ASM_EVENT_IS_FULL 			0x09
#define ERR_ASM_POLICY_CANNOT_PLAY		0x10
#define ERR_ASM_POLICY_CANNOT_PLAY_BY_CALL	0x11	/* CALL / VIDEOCALL / RICH_CALL */
#define ERR_ASM_POLICY_CANNOT_PLAY_BY_ALARM	0x12
#define ERR_ASM_POLICY_INVALID_HANDLE		0x1f
#define ERR_ASM_INVALID_PARAMETER		0x20
#define ERR_ASM_VCONF_ERROR			0x21
#define ERR_ASM_UNKNOWN_ERROR 			0x2F
#define ERR_ASM_HANDLE_IS_FULL			0x30
#ifdef USE_SECURITY
#define ERR_ASM_CHECK_PRIVILEGE_FAILED		0x40
#define COOKIE_SIZE				20
#endif


#define ASM_PRIORITY_MATRIX_MIN (ASM_EVENT_MAX-1)
#define ASM_SERVER_HANDLE_MAX 256 


#define SOUND_STATUS_KEY		"memory/Sound/SoundStatus"
/**
  * This enumeration defines sound request.
  */
typedef enum
{
	ASM_REQUEST_REGISTER 		= 0,
	ASM_REQUEST_UNREGISTER,
	ASM_REQUEST_GETSTATE,
	ASM_REQUEST_GETMYSTATE,
	ASM_REQUEST_SETSTATE,
	ASM_REQUEST_EMERGENT_EXIT,
	ASM_REQUEST_DUMP,
	ASM_REQUEST_SET_SUBSESSION,
	ASM_REQUEST_GET_SUBSESSION,
} ASM_requests_t;

/**
  * This enumeration defines sound event for Sound Scenario in Multimedia Resources Conflict Manager.
  */

typedef enum
{
	ASM_EVENT_NONE 		= -1, // [Notice] Don't use it in application (Never use it), it is internal sound event
	ASM_EVENT_SHARE_MMPLAYER 	=  0,
	ASM_EVENT_SHARE_MMCAMCORDER,
	ASM_EVENT_SHARE_MMSOUND,
	ASM_EVENT_SHARE_OPENAL,
	ASM_EVENT_SHARE_AVSYSTEM,
	ASM_EVENT_EXCLUSIVE_MMPLAYER,
	ASM_EVENT_EXCLUSIVE_MMCAMCORDER,
	ASM_EVENT_EXCLUSIVE_MMSOUND,
	ASM_EVENT_EXCLUSIVE_OPENAL,
	ASM_EVENT_EXCLUSIVE_AVSYSTEM,
	ASM_EVENT_NOTIFY,
	ASM_EVENT_CALL,
	ASM_EVENT_SHARE_FMRADIO,
	ASM_EVENT_EXCLUSIVE_FMRADIO,
	ASM_EVENT_EARJACK_UNPLUG,
	ASM_EVENT_ALARM,
	ASM_EVENT_VIDEOCALL,
	ASM_EVENT_MONITOR,
	ASM_EVENT_RICH_CALL,
	ASM_EVENT_EMERGENCY,
	ASM_EVENT_EXCLUSIVE_RESOURCE,
	ASM_EVENT_MAX
} ASM_sound_events_t;

/*
 * This enumeration defines event source for sound conflict scenario
 */
typedef enum
{
	ASM_EVENT_SOURCE_MEDIA = 0,
	ASM_EVENT_SOURCE_CALL_START,
	ASM_EVENT_SOURCE_EARJACK_UNPLUG,
	ASM_EVENT_SOURCE_RESOURCE_CONFLICT,
	ASM_EVENT_SOURCE_ALARM_START,
	ASM_EVENT_SOURCE_ALARM_END,
	ASM_EVENT_SOURCE_EMERGENCY_START,
	ASM_EVENT_SOURCE_EMERGENCY_END,
	ASM_EVENT_SOURCE_OTHER_PLAYER_APP,
	ASM_EVENT_SOURCE_RESUMABLE_MEDIA,
} ASM_event_sources_t;

/**
  * This enumeration defines sound case between playing sound and request sound
  * ALTER_PLAY : DTMF tone
  */
typedef enum
{
	ASM_CASE_NONE					= 0,
	ASM_CASE_1PLAY_2STOP			= 1,
	ASM_CASE_1STOP_2PLAY			= 5,
	ASM_CASE_1PAUSE_2PLAY			= 6,
	ASM_CASE_1PLAY_2PLAY_MIX		= 8,
	ASM_CASE_RESOURCE_CHECK			= 9
} ASM_sound_cases_t;



/*
 * This enumeration defines Sound Playing Status Information
 * Each bit is Sound Status Type( 0 : None , 1 : Playing)
 */
typedef enum
{
	ASM_STATUS_NONE						= 0x00000000,
	ASM_STATUS_SHARE_MMPLAYER			= 0x00000001,
	ASM_STATUS_SHARE_MMCAMCORDER			= 0x00000002,
	ASM_STATUS_SHARE_MMSOUND				= 0x00000004,
	ASM_STATUS_SHARE_OPENAL				= 0x00000008,
	ASM_STATUS_SHARE_AVSYSTEM			= 0x00000010,
	ASM_STATUS_EXCLUSIVE_MMPLAYER		= 0x00000020,
	ASM_STATUS_EXCLUSIVE_MMCAMCORDER		= 0x00000040,
	ASM_STATUS_EXCLUSIVE_MMSOUND			= 0x00000080,
	ASM_STATUS_EXCLUSIVE_OPENAL			= 0x00000100,
	ASM_STATUS_EXCLUSIVE_AVSYSTEM		= 0x00000200,
	ASM_STATUS_NOTIFY					= 0x00000400,
	ASM_STATUS_CALL						= 0x10000000, //Watch out
	ASM_STATUS_SHARE_FMRADIO				= 0x00000800,
	ASM_STATUS_EXCLUSIVE_FMRADIO			= 0x00001000,
	ASM_STATUS_EARJACK_UNPLUG			= 0x00002000,
	ASM_STATUS_ALARM						= 0x00100000,
	ASM_STATUS_VIDEOCALL						= 0x20000000, //Watch out
	ASM_STATUS_MONITOR					= 0x80000000, //watch out
	ASM_STATUS_RICH_CALL				= 0x40000000, //Watch out
	ASM_STATUS_EMERGENCY				= 0x00004000,
	ASM_STATUS_EXCLUSIVE_RESOURCE		= 0x00008000,
} ASM_sound_status_t;


/**
  * This enumeration defines sound state.
  */
typedef enum
{
	ASM_STATE_NONE 			= 0,
	ASM_STATE_PLAYING			= 1,
	ASM_STATE_WAITING			= 2,
	ASM_STATE_STOP			= 3,
	ASM_STATE_PAUSE			= 4,
	ASM_STATE_PAUSE_BY_APP	= 5,
	ASM_STATE_IGNORE			= 6,
} ASM_sound_states_t;


/*
 * This enumeration defines resume state
 */
typedef enum
{
	ASM_NEED_NOT_RESUME = 0,
	ASM_NEED_RESUME = 1,
}ASM_resume_states_t;
/*
 * This enumeration defines state return of client.
 */
typedef enum
{
	ASM_CB_RES_IGNORE	= -1,
	ASM_CB_RES_NONE 	= 0,
	ASM_CB_RES_PLAYING 	= 1,
	ASM_CB_RES_STOP 	= 2,
	ASM_CB_RES_PAUSE 	= 3,
}ASM_cb_result_t;

/*
 * This enumeration defines type of multimedia resource
 */

typedef enum
{
	ASM_RESOURCE_NONE				= 0x0000,
	ASM_RESOURCE_CAMERA				= 0x0001,
	ASM_RESOURCE_VIDEO_OVERLAY		= 0x0002,
	ASM_RESOURCE_HW_DECORDER		= 0x0100, //this should be removed. miss type
	ASM_RESOURCE_HW_ENCORDER		= 0x0200, //this should be removed. miss type
	ASM_RESOURCE_HW_DECODER			= 0x0100,
	ASM_RESOURCE_HW_ENCODER			= 0x0200,
	ASM_RESOURCE_RADIO_TUNNER		= 0x1000,
	ASM_RESOURCE_TV_TUNNER			= 0x2000,
}ASM_resource_t;


/* Sound command for applications */
typedef enum
{
	ASM_COMMAND_NONE 		= 0x0,
	ASM_COMMAND_PLAY 		= 0x2,
	ASM_COMMAND_STOP 		= 0x3,
	ASM_COMMAND_PAUSE 		= 0x4,
	ASM_COMMAND_RESUME 		= 0x5,
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
#ifdef USE_SECURITY
	unsigned char 	cookie [COOKIE_SIZE];
#endif
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
	ASM_sound_events_t			former_sound_event;
#ifdef USE_SECURITY
	int					check_privilege;
#endif
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
    { ASM_EVENT_SHARE_MMPLAYER, 			ASM_STATUS_SHARE_MMPLAYER },
    { ASM_EVENT_SHARE_MMCAMCORDER, 		ASM_STATUS_SHARE_MMCAMCORDER },
    { ASM_EVENT_SHARE_MMSOUND, 			ASM_STATUS_SHARE_MMSOUND },
    { ASM_EVENT_SHARE_OPENAL, 			ASM_STATUS_SHARE_OPENAL },
    { ASM_EVENT_SHARE_AVSYSTEM, 			ASM_STATUS_SHARE_AVSYSTEM },
    { ASM_EVENT_EXCLUSIVE_MMPLAYER,		ASM_STATUS_EXCLUSIVE_MMPLAYER },
    { ASM_EVENT_EXCLUSIVE_MMCAMCORDER,	ASM_STATUS_EXCLUSIVE_MMCAMCORDER },
    { ASM_EVENT_EXCLUSIVE_MMSOUND,		ASM_STATUS_EXCLUSIVE_MMSOUND },
    { ASM_EVENT_EXCLUSIVE_OPENAL,		ASM_STATUS_EXCLUSIVE_OPENAL },
    { ASM_EVENT_EXCLUSIVE_AVSYSTEM,		ASM_STATUS_EXCLUSIVE_AVSYSTEM },
    { ASM_EVENT_NOTIFY, 				ASM_STATUS_NOTIFY },
    { ASM_EVENT_CALL, 					ASM_STATUS_CALL },
    { ASM_EVENT_SHARE_FMRADIO, 			ASM_STATUS_SHARE_FMRADIO },
    { ASM_EVENT_EXCLUSIVE_FMRADIO,		ASM_STATUS_EXCLUSIVE_FMRADIO },
    { ASM_EVENT_EARJACK_UNPLUG,			ASM_STATUS_EARJACK_UNPLUG },
    { ASM_EVENT_ALARM, 					ASM_STATUS_ALARM },
    { ASM_EVENT_VIDEOCALL, 					ASM_STATUS_VIDEOCALL },
    { ASM_EVENT_MONITOR,				ASM_STATUS_MONITOR },
    { ASM_EVENT_RICH_CALL, 				ASM_STATUS_RICH_CALL },
    { ASM_EVENT_EMERGENCY,				ASM_STATUS_EMERGENCY },
    { ASM_EVENT_EXCLUSIVE_RESOURCE,		ASM_STATUS_EXCLUSIVE_RESOURCE },
};






/**
 * This callback function is called when sound status of other sound event is changed
 *
 * @return							No return value
 * @param[in]	sound_status		Argument passed when callback was set
 */
typedef ASM_cb_result_t	(*ASM_sound_cb_t) (int handle, ASM_event_sources_t event_source, ASM_sound_commands_t command, unsigned int sound_status, void* cb_data);

#endif
