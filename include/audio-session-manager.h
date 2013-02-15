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

#ifndef	_ASM_LIB_H_
#define	_ASM_LIB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <audio-session-manager-types.h>
/**
 * @ingroup	AUDIO_SESSION_MANAGER
 * @defgroup	AUDIO_SESSION_MANAGER Audio Session Manager
 * @{
 */

/**
 * This function register session to ASM server.
 *
 * @return							This function returns @c true on success; @c false otherwise.
 * @param[in] application_pid			set (-1) if this library attached to application process.
 * 										or set pid of actual application if this library attached to separated working process
 * 										(e.g. client server style framework)
 * @param[out] asm_handle				handle of asm.
 * @param[in] sound_event				sound event of instance to requested register
 * @param[in] sound_state				set sound state ( 0 : None , 1 : Playing )
 * @param[in] callback					This callback function is called when sound status of other sound event is changed
 * @param[in] cb_data					This is data of callback function
 * @param[in] mm_resource				System resources will be used.
 * @param[out] error_code				specifies the error code
 * @exception 						
 */
bool 
ASM_register_sound(const int application_pid, int *asm_handle, ASM_sound_events_t sound_event,
		ASM_sound_states_t sound_state, ASM_sound_cb_t callback, void* cb_data, ASM_resource_t mm_resource, int *error_code);

bool
ASM_register_sound_ex (const int application_pid, int *asm_handle, ASM_sound_events_t sound_event,
		ASM_sound_states_t sound_state, ASM_sound_cb_t callback, void* cb_data, ASM_resource_t mm_resource, int *error_code, int (*func)(void*,void*));

/**
 * This function unregister sound event to ASM server. If unregistered, sound event is not playing.
 *
 * @return							This function returns @c true on success; @c false otherwise.
 * @param[in] sound_event				sound event of instance to requested unregister
 * @param[out] error_code				specifies the error code
 * @exception 						
 */
bool 
ASM_unregister_sound(const int asm_handle, ASM_sound_events_t sound_event, int *error_code);

bool ASM_unregister_sound_ex(const int asm_handle, ASM_sound_events_t sound_event, int *error_code, int (*func)(void*,void*));


/**
 * This function gets sound status from ASM server
 *
 * @return							This function returns @c true on success; @c false otherwise.
 * @param[out] all_sound_status			Current Sound status of All sound events defined in sound conflict manager
 * 									return value defined in ASM_sound_status_t
 *									Each bit is Sound Status Type( 0 : None , 1 : Playing )
 * @param[out] error_code				specifies the error code
 * @exception 						
 */
bool 
ASM_get_sound_status(unsigned int *all_sound_status, int *error_code);


/**
 * This function gets sound state of sound event from ASM server
 *
 * @return							This method returns @c true on success; @c false otherwise.
 * @param[in] sound_event				sound event for want to know sound state
 * @param[out] sound_state                      result of sound state
 * @param[out] error_code				specifies the error code
 * @exception
 */
bool
ASM_get_sound_state(const int asm_handle, ASM_sound_events_t sound_event, ASM_sound_states_t *sound_state, int *error_code);

/**
 * This function gets sound state of given process from ASM server
 *
 * @return							This method returns @c true on success; @c false otherwise.
 * @param[in] asm_handle					asm_handle.
 * @param[out] sound_state                      result of sound state
 * @param[out] error_code				specifies the error code
 * @exception
 */
bool
ASM_get_process_session_state(const int asm_handle, ASM_sound_states_t *sound_state, int *error_code);

/**
 * This function set sound state to ASM server.
 *
 * @return							This function returns @c true on success; @c false otherwise.
 * @param[in] sound_event				sound event of instance to requested setting
 * @param[in] sound_state				set sound state ( 0(ASM_SND_STATE_NONE) : None , 1(ASM_SND_STATE_PLAYING) : Playing )
 * @param[in] mm_resource				system resources will be used.
 * @param[out] error_code				specifies the error code
 * @exception #ERR_asm_MSG_QUEUE_SND_ERROR   - Is is failed to send to message queue.
 */
bool 
ASM_set_sound_state(const int asm_handle, ASM_sound_events_t sound_event, ASM_sound_states_t sound_state, ASM_resource_t mm_resource, int *error_code);

bool ASM_set_sound_state_ex (const int asm_handle, ASM_sound_events_t sound_event, ASM_sound_states_t sound_state, ASM_resource_t mm_resource, int *error_code, int (*func)(void*,void*));

/**
 * This function ask sound policy to ASM server.
 *
 * @return							No return value
 * @param[in] playing_sound			playing sound event 
 * @param[in] request_sound			request sound event
 * @param[out] sound_policy			Return sound case between playing sound and request sound
 */
void
ASM_ask_sound_policy(ASM_sound_events_t playing_sound, ASM_sound_events_t request_sound, ASM_sound_cases_t *sound_policy) __attribute__((deprecated)) ; 

bool
ASM_change_callback(const int asm_handle, ASM_sound_events_t sound_event, ASM_sound_cb_t callback, void* cb_data, int *error_code);



#ifdef __cplusplus
}
#endif

#endif
