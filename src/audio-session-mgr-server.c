/*
 * audio-session-manager
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Seungbae Shin <seungbae.shin@samsung.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <poll.h>

#include <vconf.h>
#include <audio-session-manager.h>
#include <asm-log.h>
#include <string.h>
#include <errno.h>

#include <sysman.h>

#ifdef USE_SECURITY
#include <security-server.h>
#endif

#include <avsys-audio.h>

#define HIBERNATION_CHECK_KEY	"memory/hibernation/audio_ready"
#define HIBERNATION_READY		1
#define USE_SYSTEM_SERVER_PROCESS_MONITORING

#define SUPPORT_GCF /* currently in use */

static const ASM_sound_cases_t ASM_sound_case[ASM_PRIORITY_MATRIX_MIN+1][ASM_PRIORITY_MATRIX_MIN+1] =
{
	/*	  SP SC SS SO SA,  EP EC ES EO EA,  N C SF EF EU, A VC M */
		{ 8, 8, 8, 8, 8,  6, 6, 6, 6, 6,  8, 8, 8, 6, 6,  6, 6, 8 },	/* 00 Shared MMPlayer */
		{ 8, 8, 8, 8, 8,  5, 5, 5, 5, 5,  8, 5, 8, 5, 8,  5, 5, 8 },	/* 01 Shared MMCamcorder */
		{ 8, 8, 8, 8, 8,  5, 5, 5, 5, 5,  8, 5, 8, 5, 6,  5, 5, 8 },	/* 02 Shared MMSound */
		{ 8, 8, 8, 8, 8,  5, 5, 5, 5, 5,  8, 5, 8, 5, 6,  5, 5, 8 },	/* 03 Shared OpenAL */
		{ 8, 8, 8, 8, 8,  5, 5, 5, 5, 5,  8, 5, 8, 5, 8,  5, 5, 8 },	/* 04 Shared AVsystem */
		{ 6, 6, 6, 6, 6,  6, 6, 6, 6, 6,  8, 8, 6, 6, 6,  6, 6, 8 },	/* 05 Exclusive MMPlayer */
		{ 5, 5, 5, 5, 5,  5, 5, 5, 5, 5,  8, 5, 5, 5, 8,  5, 5 ,8 },	/* 06 Exclusive MMCamcorder */
		{ 5, 5, 5, 5, 5,  5, 5, 5, 5, 5,  8, 5, 5, 5, 6,  5, 5, 8 },	/* 07 Exclusive MMSound */
		{ 5, 5, 5, 5, 5,  5, 5, 5, 5, 5,  8, 5, 5, 5, 6,  5, 5, 8 },	/* 08 Exclusive OpenAL */
		{ 5, 5, 5, 5, 5,  5, 5, 5, 5, 5,  8, 5, 5, 5, 8,  5, 5, 8 },	/* 09 Exclusive AVsystem */
		{ 8, 8, 8, 8, 8,  8, 8, 8, 8, 8,  8, 5, 8, 8, 8,  5, 5, 8 },	/* 10 Notify */
		{ 1, 1, 1, 1, 1,  1, 1, 1, 1, 1,  8, 0, 1, 1, 8,  8, 1, 8 },	/* 11 Call */
		{ 8, 8, 8, 8, 8,  5, 5, 5, 5, 5,  8, 5, 5, 5, 8,  5, 5, 8 },	/* 12 Shared FMradio */
		{ 5, 5, 5, 5, 5,  5, 5, 5, 5, 5,  8, 5, 5, 5, 8,  5, 5, 8 },	/* 13 Exclusive FMradio */
		{ 8, 8, 8, 8, 8,  8, 8, 8, 8, 8,  8, 8, 8, 8, 8,  8, 8, 8 },	/* 14 Earjack Unplug */
		{ 1, 1, 1, 1, 1,  1, 1, 1, 1, 1,  8, 5, 1, 1, 8,  8, 5, 8 },	/* 15 Alarm */
		{ 1, 1, 1, 1, 1,  1, 1, 1, 1, 1,  8, 1, 1, 1, 8,  8, 0, 8 },	/* 16 Video Call */
		{ 8, 8, 8, 8, 8,  8, 8, 8, 8, 8,  8, 8, 8, 8, 8,  8, 8, 8 },	/* 17 Monitor */
};

typedef struct _list
{
	long int 				instance_id;
	int 					sound_handle;
	ASM_sound_events_t 		sound_event;
	ASM_sound_states_t 		sound_state;
	ASM_resume_states_t		need_resume;
	ASM_resource_t			mm_resource;
	avsys_audio_playing_devcie_t	device_when_interrupted;
	unsigned short			monitor_active;
	unsigned short			monitor_dirty;
	struct _list 			*next;
} asm_instance_list_t;

asm_instance_list_t *head_list, *tail_list;

int asm_snd_msgid;
int asm_rcv_msgid;
int asm_cb_msgid;
ASM_msg_lib_to_asm_t asm_rcv_msg;
ASM_msg_asm_to_lib_t asm_snd_msg;
ASM_msg_asm_to_cb_t asm_cb_msg;

bool asm_is_send_msg_to_cb = false;

#ifdef SUPPORT_GCF
#define GCF_DEFAULT	0
int is_gcf = GCF_DEFAULT;
#endif

unsigned int g_sound_status_pause = 0;
unsigned int g_sound_status_playing = 0;

#define SERVER_HANDLE_MAX_COUNT	256

static const char* ASM_sound_events_str[] =
{
	"SHARE_MMPLAYER",
	"SHARE_MMCAMCORDER",
	"SHARE_MMSOUND",
	"SHARE_OPENAL",
	"SHARE_AVSYSTEM",
	"EXCLUSIVE_MMPLAYER",
	"EXCLUSIVE_MMCAMCORDER",
	"EXCLUSIVE_MMSOUND",
	"EXCLUSIVE_OPENAL",
	"EXCLUSIVE_AVSYSTEM",
	"NOTIFY",
	"CALL",
	"SHARE_FMRADIO",
	"EXCLUSIVE_FMRADIO",
	"EARJACK_UNPLUG",
	"ALARM",
	"VIDEOCALL",
	"MONITOR"
};

static const char* ASM_sound_state_str[] =
{
	"STATE_NONE",
	"STATE_PLAYING",
	"STATE_WAITING",
	"STATE_STOP",
	"STATE_PAUSE",
	"STATE_PAUSE_BY_APP",
	"STATE_ALTER_PLAYING"
};


static const char* ASM_sound_request_str[] =
{
	"REQUEST_REGISTER",
	"REQUEST_UNREGISTER",
	"REQUEST_GETSTATE",
	"REQUEST_GETMYSTATE",
	"REQUEST_SETSTATE",
	"REQUEST_EMEGENT_EXIT",
	"REQUEST_DUMP"
};


static const char* ASM_sound_cases_str[] =
{
	"CASE_NONE",
	"CASE_1PLAY_2STOP",
	"CASE_1PLAY_2ALTER_PLAY",
	"CASE_1PLAY_2WAIT",
	"CASE_1ALTER_PLAY_2PLAY",
	"CASE_1STOP_2PLAY",
	"CASE_1PAUSE_2PLAY",
	"CASE_1VIRTUAL_2PLAY",
	"CASE_1PLAY_2PLAY_MIX"
};

static const char* ASM_sound_resume_str[] =
{
		"NO-RESUME",
		"RESUME"
};


static const char* ASM_sound_command_str[] =
{
	"CMD_NONE",
	"CMD_WAIT",
	"CMD_PLAY",
	"CMD_STOP",
	"CMD_PAUSE",
	"CMD_RESUME",
};

#define ASM_SND_MSG_SET(asm_snd_msg, x_alloc_handle, x_cmd_handle, x_result_sound_command, x_result_sound_state) \
do { \
	asm_snd_msg.data.alloc_handle 			= x_alloc_handle; 			\
	asm_snd_msg.data.cmd_handle 			= x_cmd_handle; 			\
	asm_snd_msg.data.result_sound_command 	= x_result_sound_command; 	\
	asm_snd_msg.data.result_sound_state 	= x_result_sound_state;		\
} while (0)

void selectSleep(int secs)
{
	struct timeval timeout;
	timeout.tv_sec = (secs < 1 || secs > 10) ? 3 : secs;
	timeout.tv_usec = 0;
	select(0, NULL, NULL, NULL, &timeout);
	return;
}


gboolean __find_clean_monitor_handle(int instance_id, int *handle)
{
	asm_instance_list_t *temp_list = head_list;
	int lhandle = -1;

	while (temp_list->next != tail_list) {
		if (temp_list->instance_id == instance_id && temp_list->sound_event == ASM_EVENT_MONITOR) {
			if (temp_list->monitor_dirty == 0) {
				lhandle = temp_list->sound_handle;
			}
			break;
		}
		temp_list = temp_list->next;
	}
	if (lhandle == -1) {
		return FALSE;
	} else {
		*handle = lhandle;
		return TRUE;
	}
}

void __update_monitor_active(long int instance_id)
{
	asm_instance_list_t *temp_list = head_list;
	asm_instance_list_t *monitor_list = NULL;
	unsigned short active = 0;
	asm_info("[ASM_Server] %s\n",__func__);

	while (temp_list->next != tail_list) {
		if (temp_list->instance_id == instance_id && temp_list->sound_event == ASM_EVENT_MONITOR) {
			/* backup monitor pointer */
			monitor_list = temp_list;
			break;
		}
		temp_list = temp_list->next;
	}
	if (NULL == monitor_list) {
		asm_warning("[ASM_Server] %s : No monitor instance for %d\n",__func__, instance_id);
		return;
	}

	temp_list = head_list;
	while (temp_list->next != tail_list) {
		if (temp_list->instance_id == instance_id && temp_list->sound_event != ASM_EVENT_MONITOR) {
			if (ASM_STATE_PLAYING == temp_list->sound_state) {
				active = 1;
				break;
			}
		}
		temp_list = temp_list->next;
	}

	monitor_list->monitor_active = active;
}

void __set_all_monitor_clean()
{
	asm_instance_list_t *temp_list = head_list;

	while (temp_list->next != tail_list) {
		if (temp_list->sound_event == ASM_EVENT_MONITOR) {
			temp_list->monitor_dirty = 0;
		}
		temp_list = temp_list->next;
	}
}

void __set_monitor_dirty(long int instance_id)
{
	asm_instance_list_t *temp_list = head_list;

	while (temp_list->next != tail_list) {
		if (temp_list->instance_id == instance_id && temp_list->sound_event == ASM_EVENT_MONITOR) {
			temp_list->monitor_dirty = 1;
			break;
		}
		temp_list = temp_list->next;
	}
}

/* callback without retcb */
void __do_callback_wo_retcb(int instance_id,int handle,int command)
{
	int fd_ASM = 0, cur_handle = 0;
	char *filename = g_strdup_printf("/tmp/ASM.%d.%d", instance_id, handle);

	if ((fd_ASM = open(filename,O_WRONLY|O_NONBLOCK)) < 0) {
		asm_info("[ASM_Server][CallCB] %s open error",filename);
		g_free(filename);
		return;
	}
	cur_handle = (unsigned int)(handle |(command << 4));
	if (write(fd_ASM, (void *)&cur_handle, sizeof(cur_handle)) < 0) {
		asm_info("[ASM_Server][CallCB] %s write error",filename);
		g_free(filename);
		return;
	}
	close(fd_ASM);
	g_free(filename);
	selectSleep(2); /* if return immediately bad sound occur */
}


int __do_callback(int instance_id,int handle,int command, ASM_event_sources_t event_src)
{
	char *filename = NULL;
	char *filename2 = NULL;
	struct timeval time;
	int starttime = 0;
	int endtime = 0;
	int fd=0,nread = 0;
	int fd_ASM = 0, cur_handle = 0;
	int buf = 0;
	struct pollfd pfd;
	int pret = 0;
	int pollingTimeout = 7000;

	asm_info("[ASM_Server] __do_callback for pid(%d) handle(%d)\n", instance_id, handle);

	/* Set start time */
	gettimeofday(&time, NULL);
	starttime = time.tv_sec * 1000000 + time.tv_usec;

	/**************************************
	 *
	 * Open callback cmd pipe
	 *
	 **************************************/
	filename = g_strdup_printf("/tmp/ASM.%d.%d", instance_id, handle);
	if ((fd_ASM = open(filename, O_WRONLY|O_NONBLOCK)) < 0) {
		asm_error("[ASM_Server][CallCB] %s open error\n", filename);
		goto fail;
	}

	/******************************************
	 *
	 * Open callback result pipe
	 * before writing callback cmd to pipe
	 *
	 ******************************************/
	filename2 = g_strdup_printf("/tmp/ASM.%d.%dr", instance_id, handle);
	if ((fd=open(filename2,O_RDONLY|O_NONBLOCK))== -1) {
		char str_error[256];
		strerror_r (errno, str_error, sizeof(str_error));
		asm_error("[ASM_Server][RETCB] Fail to open fifo (%s)\n", str_error);
		goto fail;
	}
	asm_info("[ASM_Server] open return cb %s\n", filename2);


	/*******************************************
	 * Write Callback msg
	 *******************************************/
	cur_handle = (unsigned int)((0x0000ffff & handle) |(command << 16) | (event_src << 24));
	if (write(fd_ASM, (void *)&cur_handle, sizeof(cur_handle)) < 0) {
		asm_error("[ASM_Server][CallCB] %s write error\n", filename);
		goto fail;
	}
	/**************************************
	 *
	 * Close callback cmd pipe
	 *
	 **************************************/
	close(fd_ASM);
	fd_ASM = -1;
	g_free(filename);
	filename = NULL;

	pfd.fd = fd;
	pfd.events = POLLIN;

	asm_instance_list_t *temp_list = head_list;


	/*********************************************
	 *
	 * Wait callback result msg
	 *
	 ********************************************/
	asm_critical("[ASM_Server][RETCB]wait callback(tid=%d, handle=%d, cmd=%d, timeout=%d)\n", instance_id, handle, command, pollingTimeout);
	pret = poll(&pfd, 1, pollingTimeout); //timeout 7sec
	if (pret < 0) {
		asm_error("[ASM_Server][RETCB]poll failed (%d)\n", pret);
		goto fail;
	}
	if (pfd.revents & POLLIN) {
		nread=read(fd, (void *)&buf, sizeof(buf));
	}
	g_free(filename2);
	filename2 = NULL;

	/* Calculate endtime and display*/
	gettimeofday(&time, NULL);
	endtime = time.tv_sec * 1000000 + time.tv_usec;
	asm_critical("[ASM_Server][RETCB] ASM_CB_END cbtimelab=%3.3f(second), timeout=%d(milli second) (reciever=%d)\n", ((endtime-starttime)/1000000.), pollingTimeout, instance_id);

	/**************************************
	 *
	 * Close callback result pipe
	 *
	 **************************************/
	close(fd);
	fd = -1;
	asm_info("[ASM_Server][RETCB] Return value 0x%x\n", buf);
	return buf;

fail:
	if (filename) {
		g_free (filename);
		filename = NULL;
	}
	if (filename2) {
		g_free (filename2);
		filename2 = NULL;
	}
	if (fd_ASM != -1) {
		close(fd_ASM);
		fd_ASM = -1;
	}
	if (fd != -1) {
		close (fd);
		fd = -1;
	}

	return -1;
}

gboolean __isPlayingNow()
{
	asm_instance_list_t *temp_list = head_list;
	while (temp_list->next != tail_list) {
		if (temp_list->sound_state == ASM_STATE_PLAYING ) {
			return TRUE;
		}

		temp_list = temp_list->next;
	}
	return FALSE;
}

gboolean __isItPlayingNow(int instance_id, int handle)
{
	asm_instance_list_t *temp_list = head_list;
	while (temp_list->next != tail_list) {
		if (temp_list->instance_id == instance_id && temp_list->sound_handle == handle) {
			if (temp_list->sound_state == ASM_STATE_PLAYING) {
				return TRUE;
			}
		}

		temp_list = temp_list->next;
	}
	return FALSE;
}

void __temp_print_list(char * msg)
{
	asm_instance_list_t *temp_list = head_list;
	int i = 0;

	if (NULL != msg) {
		asm_warning("[ASM_Server] %s\n", msg);
	}
	while (temp_list->next != tail_list) {
		asm_info("[ASM_Server] List[%02d] ( %5ld, %2d, %-20s, %-20s, %9s, 0x%04x)\n", i, temp_list->instance_id, temp_list->sound_handle,
												ASM_sound_events_str[temp_list->sound_event],
												ASM_sound_state_str[temp_list->sound_state],
												ASM_sound_resume_str[temp_list->need_resume],
												temp_list->mm_resource);
		temp_list = temp_list->next;
		i++;
	}
}

void updatePhoneStatus()
{
	asm_instance_list_t *temp_list = head_list;
	int i = 0, error = 0;

	g_sound_status_pause = 0;
	g_sound_status_playing = 0;

	while (temp_list->next != tail_list) {
		if (temp_list->sound_state == ASM_STATE_PLAYING) {
			if (temp_list->sound_event >= ASM_EVENT_SHARE_MMPLAYER && temp_list->sound_event  < ASM_EVENT_MAX) {
				g_sound_status_playing |= ASM_sound_type[(temp_list->sound_event) + 1].sound_status;
			}
		} else if (temp_list->sound_state == ASM_STATE_PAUSE ) {
			if (temp_list->sound_event >= ASM_EVENT_SHARE_MMPLAYER && temp_list->sound_event < ASM_EVENT_MAX) {
				g_sound_status_pause |= ASM_sound_type[(temp_list->sound_event) + 1].sound_status;
			}
		}
		temp_list = temp_list->next;
	}

	if (vconf_set_int(SOUND_STATUS_KEY, g_sound_status_playing)) {
		asm_info("[ASM_Server[Error = %d][1st try] phonestatus_set \n", error);
		if (vconf_set_int(SOUND_STATUS_KEY, g_sound_status_playing)) {
			asm_critical("[ASM_Server][Error = %d][2nd try]  phonestatus_set \n", error);
		}
	}

	asm_info("[ASM_Server] soundstatus set to (0x%08x)\n", g_sound_status_playing);
}


void __asm_register_list(long int instance_id, int handle, ASM_sound_events_t sound_event, ASM_sound_states_t sound_state, ASM_resource_t mm_resource)
{
	asm_instance_list_t *temp_list;
	temp_list = (asm_instance_list_t *)malloc(sizeof(asm_instance_list_t));
	temp_list->instance_id = instance_id;
	temp_list->sound_handle = handle;
	temp_list->sound_event = sound_event;
	temp_list->sound_state = sound_state;
	temp_list->need_resume = 0;
	temp_list->mm_resource = mm_resource;
	temp_list->monitor_active = 0;
	temp_list->monitor_dirty = 0;
	temp_list->device_when_interrupted = AVSYS_AUDIO_ROUTE_DEVICE_UNKNOWN;
	temp_list->next = head_list;
	head_list = temp_list;

	__temp_print_list("Register List");
	updatePhoneStatus();
}

int __asm_unregister_list(int handle)
{
	asm_instance_list_t *temp_list = head_list;
	asm_instance_list_t *temp_list2 = head_list; 
	int instance_id = -1;

	asm_info("[ASM_Server] __asm_unregister_list \n");

	while (temp_list->next != tail_list) {
		if (temp_list->sound_handle == handle) {
			instance_id = temp_list->instance_id;
			if (temp_list == head_list)
				head_list = temp_list->next;
			else
				temp_list2->next = temp_list->next; 
			free(temp_list);
			break;
		}
		temp_list2 = temp_list; 
		temp_list = temp_list->next;
	}

	__temp_print_list("Unregister List for handle");
	updatePhoneStatus();
	return instance_id;
}


/* -------------------------
 * if PID exist return true, else return false
 */
gboolean isPIDExist(int pid)
{
	if (pid > 999999 || pid < 2)
		return FALSE;
	gchar *tmp = g_malloc0(25);
	g_sprintf(tmp, "/proc/%d", pid);
	if (access(tmp, R_OK)==0) {
		g_free(tmp);
		return TRUE;
	}
	g_free(tmp);
	return FALSE;
}


/* -------------------------
 *
 */
void __check_dead_process()
{
	asm_instance_list_t *temp_list = head_list;
	asm_instance_list_t *temp_list2 = head_list;
	while (temp_list->next != tail_list) {
		if (!isPIDExist(temp_list->instance_id)) {
			asm_critical_r("[ASM_Server] PID(%ld) not exist! -> ASM_Server resource of pid(%ld) will be cleared \n", temp_list->instance_id, temp_list->instance_id);

			if (temp_list == head_list) {
				head_list = temp_list->next;
			}
			temp_list2->next = temp_list->next;
			free(temp_list);
		} else {
			temp_list2 = temp_list;
		}
		temp_list = temp_list2->next;
	}
	updatePhoneStatus();
}




void emergent_exit(int exit_pid)
{
	asm_instance_list_t *temp_list = head_list;
	int handle = -1;
	int instance_id = -1;

	while (temp_list->next != tail_list) {
		if (temp_list->instance_id == exit_pid) {
			handle = temp_list->sound_handle;

			instance_id = __asm_unregister_list(handle);

			if (instance_id != -1) {
				char str_error[256];
				char* filename = g_strdup_printf("/tmp/ASM.%d.%d", instance_id, handle);
				char* filename2 = g_strdup_printf("/tmp/ASM.%d.%dr", instance_id, handle);
				if (!remove(filename)) {
					asm_info("[ASM_Server] remove %s success\n", filename);
				} else {
					strerror_r (errno, str_error, sizeof (str_error));
					asm_error("[ASM_Server] remove %s failed with %s\n", filename, str_error);
				}

				if (!remove(filename2)) {
					asm_info("[ASM_Server] remove %s success\n", filename2);
				} else {
					strerror_r (errno, str_error, sizeof (str_error));
					asm_error("[ASM_Server] remove %s failed with %s\n", filename2, str_error);
				}

				g_free(filename);
				g_free(filename2);
			}
			temp_list = head_list;
		} else {
			temp_list = temp_list->next;
		}
	}

	asm_info("[ASM_Server][EMERGENT_EXIT] complete\n");
	return;
}


int ___reorder_state(ASM_sound_states_t input)
{
	int res = 0;

	switch (input) {
	case ASM_STATE_IGNORE:
	case ASM_STATE_NONE:
		res = 0;
		break;
	case ASM_STATE_WAITING:
	case ASM_STATE_STOP:
		res = 1;
		break;
	case ASM_STATE_PAUSE:
	case ASM_STATE_PAUSE_BY_APP:
		res = 2;
		break;
	case ASM_STATE_PLAYING:
		res = 3;
		break;
	}
	return res;
}

ASM_sound_states_t __asm_find_process_status(int pid)
{
	asm_instance_list_t *temp_list = head_list;
	ASM_sound_states_t result_state = ASM_STATE_NONE;

	asm_info("[ASM_Server] __asm_find_process_status for pid %d\n", pid);

	while (temp_list->next != tail_list) {
		if (temp_list->instance_id == pid) {
			if ( ___reorder_state(temp_list->sound_state) >= ___reorder_state(result_state)) {
				result_state = temp_list->sound_state;
			}
		}
		temp_list = temp_list->next;
	}

	return result_state;
}

ASM_sound_states_t __asm_find_list(ASM_requests_t request_id, int handle)
{
	asm_instance_list_t *temp_list = head_list;

	asm_info("[ASM_Server] __asm_find_list\n");

	while (temp_list->next != tail_list) {
		if ((request_id == ASM_REQUEST_GETSTATE && temp_list->sound_handle == handle)) {
			return temp_list->sound_state;
		} else {
			temp_list = temp_list->next;
		}
	}

	return ASM_STATE_NONE;
}

void __asm_change_state_list(long int instance_id, int handle, ASM_sound_states_t sound_state, ASM_resource_t mm_resource)
{
	asm_instance_list_t *temp_list = head_list;
	int monitor_handle = -1;
	asm_info("[ASM_Server] __asm_change_state_list\n");
	if (sound_state == ASM_STATE_IGNORE) {
		asm_info("[ASM_Server] skip update state list %ld-%d\n", instance_id, handle);
		return;
	}

	while (temp_list->next != tail_list) {
		if (temp_list->instance_id == instance_id && temp_list->sound_handle == handle) {
			temp_list->sound_state = sound_state;
			temp_list->mm_resource = mm_resource;
			break;
		}
		temp_list = temp_list->next;
	}
	__update_monitor_active(instance_id);
	updatePhoneStatus();
}

void __asm_change_need_resume_list(long int instance_id, int handle, ASM_resume_states_t need_resume)
{
	asm_instance_list_t *temp_list = head_list;
	asm_info("[ASM_Server] __asm_change_need_resume_list\n");
	while (temp_list->next != tail_list) {
		if (temp_list->instance_id == instance_id && temp_list->sound_handle == handle) {
			temp_list->need_resume = need_resume;

			if (need_resume == ASM_NEED_RESUME) {
				avsys_audio_playing_devcie_t dev = AVSYS_AUDIO_ROUTE_DEVICE_UNKNOWN;
				if (AVSYS_SUCCESS(avsys_audio_get_playing_device_info(&dev))) {
					temp_list->device_when_interrupted = dev;
				}
			}
			break;
		}
		temp_list = temp_list->next;
	}
}


void __asm_create_message_queue()
{
	asm_rcv_msgid = msgget((key_t)2014, 0666 | IPC_CREAT);
	asm_snd_msgid = msgget((key_t)4102, 0666 | IPC_CREAT);
	asm_cb_msgid = msgget((key_t)4103, 0666 | IPC_CREAT);

	if (asm_snd_msgid == -1 || asm_rcv_msgid == -1 || asm_cb_msgid == -1) {
		asm_critical("[ASM_Server] msgget failed with error: \n");
		exit(EXIT_FAILURE);
	}
}

void __asm_snd_message()
{
	if (msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0) == -1) {
		asm_critical("[ASM_Server] msgsnd failed with error %d\n", errno);
		exit(EXIT_FAILURE);
	}
}

void __asm_cb_message()
{
	if (msgsnd(asm_cb_msgid, (void *)&asm_cb_msg, sizeof(asm_cb_msg.data), 0) == -1) {
		asm_critical_r("[ASM_Server] msgsnd(Callback msg) failed with error %d\n", errno);
		exit(EXIT_FAILURE);
	}
}

void __asm_rcv_message()
{
	if (msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), 0, 0) == -1) {
		asm_critical_r("[ASM_Server] msgrcv failed with error %d\n", errno);
		exit(EXIT_FAILURE);
	}
}

void __asm_get_empty_handle(long int instance_id, int *handle)
{
	asm_instance_list_t *temp_list = head_list;
	unsigned int i = 0, find_empty = 0, j = 0;
	char handle_info[SERVER_HANDLE_MAX_COUNT];

	asm_info("[ASM_Server] __asm_make_handle for %ld\n", instance_id);
	__temp_print_list("current list before get new handle");

	memset(handle_info, 0, sizeof(char) * SERVER_HANDLE_MAX_COUNT);

	while (temp_list->next != tail_list) {
		handle_info[temp_list->sound_handle] = 1;
		temp_list = temp_list->next;
	}

	for (i = 0; i < ASM_SERVER_HANDLE_MAX; i++) {
		if (handle_info[i] == 0) {
			find_empty = 1;
			break;
		}
	}
	if (find_empty && (i != ASM_SERVER_HANDLE_MAX)) {
		asm_error_r("[ASM_Server] New handle for %ld is %d\n", instance_id, i);
		*handle = i;
	} else {
		asm_error_r("[ASM_Server] Handle is full for pid %ld\n", instance_id);
		*handle = -1;
	}

}

void __print_resource(unsigned short resource_status)
{
	if (resource_status == ASM_RESOURCE_NONE)
		asm_info("[ASM_Server] resource NONE\n");
	if (resource_status | ASM_RESOURCE_CAMERA)
		asm_info("[ASM_Server] resource CAMERA\n");
	if (resource_status | ASM_RESOURCE_VIDEO_OVERLAY)
		asm_info("[ASM_Server] resource VIDEO OVERLAY\n");
	if (resource_status | ASM_RESOURCE_HW_ENCORDER)
		asm_info("[ASM_Server] resource HW ENCORDER\n");
	if (resource_status | ASM_RESOURCE_HW_DECORDER)
		asm_info("[ASM_Server] resource HW DECORDER\n");
	if (resource_status | ASM_RESOURCE_RADIO_TUNNER)
		asm_info("[ASM_Server] resource RADIO TUNNER\n");
	if (resource_status | ASM_RESOURCE_TV_TUNNER)
		asm_info("[ASM_Server] resource TV TUNNER\n");
}

void __asm_compare_priority_matrix(long int instance_id, int handle, ASM_requests_t request_id,
                                  ASM_sound_events_t sound_event,ASM_sound_states_t sound_state, ASM_resource_t mm_resource)
{
	int no_conflict_flag = 0;

	/* If nobody is playing now, this means no conflict */
	if (ASM_STATUS_NONE == g_sound_status_playing) {
		asm_info("[ASM_Server] __asm_compare_priority_matrix : No conflict ( No existing Sound )\n");

		ASM_SND_MSG_SET(asm_snd_msg, handle, -1, ASM_COMMAND_NONE, sound_state);

		no_conflict_flag = 1;
	} else { /* Somebody is playing */
		asm_instance_list_t *temp_list = head_list;
		int updatedflag = 0;
		int cb_res = 0;
		int update_state = ASM_STATE_NONE;

		while (temp_list->next != tail_list) {
			/* Find who's playing now */
			if (temp_list->sound_state == ASM_STATE_PLAYING) {
				/* Found it */
				ASM_sound_states_t current_play_state = temp_list->sound_state;
				ASM_sound_events_t current_play_sound_event = temp_list->sound_event;
				long int current_play_instance_id = temp_list->instance_id;
				int current_play_handle = temp_list->sound_handle;
				ASM_resource_t current_using_resource = temp_list->mm_resource;

				if ((current_play_instance_id == instance_id) && (current_play_handle == handle)) {
					asm_warning("[ASM_Server] This is my handle. skip %d %d\n", instance_id, handle);
					temp_list = temp_list->next;
					continue;
				}

				/* Request is PLAYING */
				if (sound_state == ASM_STATE_PLAYING) {
					/* Determine sound policy */
					ASM_sound_cases_t sound_case = ASM_sound_case[current_play_sound_event][sound_event];

#ifdef SUPPORT_GCF
					/* GCF case is exception case */
					/* NOTE : GCF exception case only */
					if ((is_gcf) && (sound_case != ASM_CASE_1PLAY_2PLAY_MIX)) {
						sound_case = ASM_CASE_1PLAY_2PLAY_MIX;;
					}
#endif

					asm_critical("[ASM_Server] Conflict policy[%x][%x]: %s\n", current_play_sound_event,sound_event,ASM_sound_cases_str[sound_case]);
					switch (sound_case) {
					case ASM_CASE_1PLAY_2STOP:
					{
						if (current_play_instance_id == instance_id) {
							/* PID is policy group.*/
							asm_info("[ASM_Server] Do not send Stop callback in same pid %ld\n", instance_id);
						} else {
							ASM_SND_MSG_SET(asm_snd_msg, handle, handle, ASM_COMMAND_STOP, sound_state);
							temp_list = tail_list;	/* skip all remain list */
							break;
						}

						/* Prepare msg to send */
						ASM_SND_MSG_SET(asm_snd_msg, handle, handle, ASM_COMMAND_PLAY, sound_state);

						if (!updatedflag) {
							if (request_id == ASM_REQUEST_REGISTER){
								__asm_register_list(instance_id, handle, sound_event, sound_state, mm_resource);
							} else {
								__asm_change_state_list(instance_id, handle, sound_state, mm_resource);
							}
							updatedflag = 1;
						}
						break;
					}

					case ASM_CASE_1STOP_2PLAY:
					{
						if (current_play_instance_id == instance_id) {
							/* PID is policy group. */
							asm_info("[ASM_Server] Do not send Stop callback in same pid %ld\n", instance_id);
						} else {
							ASM_event_sources_t event_src;
							unsigned short resource_status = current_using_resource & mm_resource;

							/* Determine root cause of conflict */
							if (resource_status != ASM_RESOURCE_NONE) {
								event_src = ASM_EVENT_SOURCE_RESOURCE_CONFLICT;
							} else {
								switch (sound_event) {
								case ASM_EVENT_CALL:
								case ASM_EVENT_VIDEOCALL:
									event_src = ASM_EVENT_SOURCE_CALL_START;
									break;

								case ASM_EVENT_EARJACK_UNPLUG:
									event_src = ASM_EVENT_SOURCE_EARJACK_UNPLUG;
									break;

								case ASM_EVENT_ALARM:
									event_src = ASM_EVENT_SOURCE_ALARM_START;
									break;

								default:
									event_src = ASM_EVENT_SOURCE_OTHER_APP;
									break;
								}
							}

							/* Execute callback function for monitor handle */
							int monitor_handle = -1;
							if (__find_clean_monitor_handle(current_play_instance_id, &monitor_handle)) {
								cb_res = __do_callback(current_play_instance_id, monitor_handle, ASM_COMMAND_STOP, event_src);
								asm_warning("[ASM_Server] send stop callback for monitor handle of pid %d\n", current_play_instance_id);
								if (cb_res != ASM_CB_RES_NONE && cb_res != ASM_CB_RES_STOP) {
									asm_error_r("[ASM_Server] oops! not suspected callback result %d\n", cb_res);
								}
								__set_monitor_dirty(current_play_instance_id);

								/* If current is playing and input event is CALL/VIDEOCALL/ALARM, set to need resume */
								if ((sound_event == ASM_EVENT_CALL || sound_event == ASM_EVENT_VIDEOCALL || sound_event == ASM_EVENT_ALARM)
								   && (current_play_state == ASM_STATE_PLAYING)) {
									__asm_change_need_resume_list(current_play_instance_id, monitor_handle, ASM_NEED_RESUME);
								}
							}

							/* Execute callback function for worker handle */
							cb_res = __do_callback(current_play_instance_id,current_play_handle,ASM_COMMAND_STOP, event_src);
							if (cb_res != ASM_CB_RES_NONE && cb_res != ASM_CB_RES_STOP)
								asm_error_r("[ASM_Server] oops! not suspected result %d\n", cb_res);
							asm_warning("[ASM_Server]  __asm_compare_priority_matrix(1STOP_2PLAY) : __do_callback Complete : TID=%ld, handle=%d",
										current_play_instance_id,current_play_handle );

							/* If current is playing and input event is CALL/VIDEOCALL/ALARM, set to need resume */
							if ((sound_event == ASM_EVENT_CALL || sound_event == ASM_EVENT_VIDEOCALL || sound_event == ASM_EVENT_ALARM)
								&& (current_play_state == ASM_STATE_PLAYING)) {
								__asm_change_need_resume_list(current_play_instance_id, current_play_handle, ASM_NEED_RESUME);
							}

							/* Set state to NONE */
							__asm_change_state_list(current_play_instance_id, current_play_handle, ASM_STATE_NONE, ASM_RESOURCE_NONE);
						}

						/* Prepare msg to send */
						ASM_SND_MSG_SET(asm_snd_msg, handle, handle, ASM_COMMAND_PLAY, sound_state);

						if (!updatedflag) {
							if (request_id == ASM_REQUEST_REGISTER) {
								__asm_register_list(instance_id, handle, sound_event, sound_state, mm_resource);
							} else {
								__asm_change_state_list(instance_id, handle, sound_state, mm_resource);
							}
							updatedflag = 1;
						}
						break;
					}

					case ASM_CASE_1PAUSE_2PLAY:
					{
						ASM_resource_t	update_resource = current_using_resource;
						if (current_play_instance_id == instance_id)	{
							asm_info("[ASM_Server] Do not send Pause callback in same pid %ld\n", instance_id);
						} else {
							ASM_event_sources_t event_src;
							ASM_sound_commands_t command;

							unsigned short resource_status = current_using_resource & mm_resource;
							if (resource_status != ASM_RESOURCE_NONE) {
								asm_info("[ASM_Server] resource conflict found 0x%x\n", resource_status);
								event_src = ASM_EVENT_SOURCE_RESOURCE_CONFLICT;
								command = ASM_COMMAND_STOP;
							} else {
								switch (sound_event) {
								case ASM_EVENT_CALL:
								case ASM_EVENT_VIDEOCALL:
									event_src = ASM_EVENT_SOURCE_CALL_START;
									break;

								case ASM_EVENT_EARJACK_UNPLUG:
									event_src = ASM_EVENT_SOURCE_EARJACK_UNPLUG;
									break;

								case ASM_EVENT_ALARM:
									event_src = ASM_EVENT_SOURCE_ALARM_START;
									break;

								case ASM_EVENT_SHARE_MMPLAYER:
								case ASM_EVENT_EXCLUSIVE_MMPLAYER:
									if (	current_play_sound_event == ASM_EVENT_SHARE_MMPLAYER ||
										current_play_sound_event == ASM_EVENT_EXCLUSIVE_MMPLAYER ) {
										event_src = ASM_EVENT_SOURCE_OTHER_PLAYER_APP;
										break;
									}

								default:
									event_src = ASM_EVENT_SOURCE_OTHER_APP;
									break;
								}
								command = ASM_COMMAND_PAUSE;
							}

							/* Execute callback function for monitor handle */
							int monitor_handle = -1;
							if (__find_clean_monitor_handle(current_play_instance_id, &monitor_handle)) {
								cb_res = __do_callback(current_play_instance_id, monitor_handle, ASM_COMMAND_STOP, event_src);
								asm_warning("[ASM_Server] send stop callback for monitor handle of pid %d\n", current_play_instance_id);
								if (cb_res != ASM_CB_RES_NONE && cb_res != ASM_CB_RES_STOP) {
									asm_error_r("[ASM_Server] oops! not suspected callback result %d\n", cb_res);
								}
								__set_monitor_dirty(current_play_instance_id);

								/* If current is playing and input event is CALL/VIDEOCALL/ALARM, set to need resume */
								if ((sound_event == ASM_EVENT_CALL || sound_event == ASM_EVENT_VIDEOCALL || sound_event == ASM_EVENT_ALARM)
									&& (current_play_state == ASM_STATE_PLAYING)) {
									__asm_change_need_resume_list(current_play_instance_id, monitor_handle, ASM_NEED_RESUME);
								}
							}

							/* Execute callback function for worker handle */
							cb_res = __do_callback(current_play_instance_id,current_play_handle,command, event_src);
							asm_warning("[ASM_Server]  %s(1PAUSE_2PLAY) : Callback of %s: TID(%ld)\n",
										__func__,ASM_sound_command_str[command], current_play_instance_id);
							/*Change current sound' state when it is in 1Pause_2Play case */
							switch (cb_res) {
							case ASM_CB_RES_PAUSE:
								update_state = ASM_STATE_PAUSE;
								break;

							case ASM_CB_RES_NONE:
							case ASM_CB_RES_STOP:
								update_state = ASM_STATE_NONE;
								update_resource = ASM_RESOURCE_NONE;
								break;

							case ASM_CB_RES_IGNORE:
								update_state = ASM_STATE_IGNORE;
								break;

							default:
								asm_error_r("[ASM_Server] oops! not suspected result %d\n", cb_res);
								update_state = ASM_STATE_NONE;
								break;
							}

							/* If current is playing and input event is CALL/VIDEOCALL/ALARM, set to need resume */
							if ((sound_event == ASM_EVENT_CALL || sound_event == ASM_EVENT_VIDEOCALL || sound_event == ASM_EVENT_ALARM)
								&&(current_play_state == ASM_STATE_PLAYING)) {
								__asm_change_need_resume_list(current_play_instance_id, current_play_handle, ASM_NEED_RESUME);
							}

							__asm_change_state_list(current_play_instance_id, current_play_handle, update_state, update_resource);
						}

						/* Prepare msg to send */
						ASM_SND_MSG_SET(asm_snd_msg, handle, handle, ASM_COMMAND_PLAY, sound_state);

						if (!updatedflag) {
							if (request_id == ASM_REQUEST_REGISTER) {
								__asm_register_list(instance_id, handle, sound_event, sound_state, mm_resource);
							} else {
								__asm_change_state_list(instance_id, handle, sound_state, mm_resource);
							}
							updatedflag = 1;
						}
						break;
					}

					case ASM_CASE_1PLAY_2PLAY_MIX:
					{
						if (current_play_instance_id == instance_id) {
							asm_info("[ASM_Server] Do not send check resource conflict in same pid %ld\n", instance_id);
						} else {
							/* MIX but need to check resource conflict */
							asm_warning("[ASM_Server] >>>> __asm_compare_priority_matrix(1PLAY_2PLAY_MIX) :  !!!\n");
							ASM_resource_t update_resource = current_using_resource;
							unsigned short resource_status = current_using_resource & mm_resource;
							if (resource_status) { /* Resouce conflict */
								asm_warning("[ASM_Server] there is system resource conflict 0x%x\n", resource_status);
								__print_resource(resource_status);

								/* Execute callback function for monitor handle */
								int monitor_handle = -1;
								if (__find_clean_monitor_handle(current_play_instance_id, &monitor_handle)) {
									cb_res = __do_callback(current_play_instance_id, monitor_handle, ASM_COMMAND_STOP, ASM_EVENT_SOURCE_RESOURCE_CONFLICT);
									if (cb_res != ASM_CB_RES_NONE && cb_res != ASM_CB_RES_STOP) {
										asm_error_r("[ASM_Server] oops! not suspected callback result %d\n", cb_res);
									}
									asm_warning("[ASM_Server] send stop callback for monitor handle of pid %d\n", current_play_instance_id);
									__set_monitor_dirty(current_play_instance_id);

									/* If current is playing and input event is CALL/VIDEOCALL/ALARM, set to need resume */
									if ((sound_event == ASM_EVENT_CALL || sound_event == ASM_EVENT_VIDEOCALL || sound_event == ASM_EVENT_ALARM)
										&& (current_play_state == ASM_STATE_PLAYING) ) {
										__asm_change_need_resume_list(current_play_instance_id, monitor_handle, ASM_NEED_RESUME);
									}
								}

								/* Execute callback function for worker handle */
								/* Stop current resource holding instance */
								cb_res = __do_callback(current_play_instance_id, current_play_handle, ASM_COMMAND_STOP, ASM_EVENT_SOURCE_RESOURCE_CONFLICT);
								asm_warning("[ASM_Server]  __asm_compare_priority_matrix(1PLAY_2PLAY_MIX) : Resource Conflict : TID(%ld)\n",
											current_play_instance_id);

								/* Change current sound */
								switch (cb_res) {
								case ASM_CB_RES_NONE:
								case ASM_CB_RES_STOP:
									update_state = ASM_STATE_NONE;
									update_resource = ASM_RESOURCE_NONE;
									break;

								case ASM_CB_RES_IGNORE:
									update_state = ASM_STATE_IGNORE;
									break;

								default:
									asm_error_r("[ASM_Server] oops! not suspected result %d\n", cb_res);
									update_state = ASM_STATE_NONE;
									break;
								}

								__asm_change_state_list(current_play_instance_id, current_play_handle, update_state, update_resource);
							}
						}

						/* Prepare msg to send */
						ASM_SND_MSG_SET(asm_snd_msg, handle, handle, ASM_COMMAND_PLAY, sound_state);

						if (!updatedflag) {
							if (request_id == ASM_REQUEST_REGISTER) {
								__asm_register_list(instance_id, handle, sound_event, sound_state, mm_resource);
							} else {
								__asm_change_state_list(instance_id, handle, sound_state, mm_resource);
							}
							updatedflag = 1;
						}
						break;
					}

					default:
					{
						ASM_SND_MSG_SET(asm_snd_msg, handle, handle, ASM_COMMAND_NONE, sound_state);
						asm_warning("[ASM_Server] >>>> __asm_compare_priority_matrix : ASM_CASE_NONE [It should not be seen] !!!\n");
						break;
					}
					} /* switch (sound_case) */
				} else {
					/* Request was not PLAYING, this means no conflict, just do set */
					asm_info("[ASM_Server] __asm_compare_priority_matrix : No Conflict (Just Register or Set State) !!!\n");
					ASM_SND_MSG_SET(asm_snd_msg, handle, handle, ASM_COMMAND_NONE, sound_state);

					if (sound_state == ASM_STATE_NONE) {
						asm_info("[ASM_Server] __asm_compare_priority_matrix(1PLAY_2NONE) : No Conflict !!!\n");
					} else if (sound_state == ASM_STATE_WAITING) {
						asm_info("[ASM_Server] __asm_compare_priority_matrix(1PLAY_2WAIT) : No Conflict !!!\n");
					}

					if (!updatedflag) {
						if (request_id == ASM_REQUEST_REGISTER) {
							__asm_register_list(instance_id, handle, sound_event, sound_state, mm_resource);
						} else {
							__asm_change_state_list(instance_id, handle, sound_state, mm_resource);
						}
						updatedflag = 1;
					}
				}
			} /* if (temp_list->sound_state == ASM_STATE_PLAYING)  */

			temp_list = temp_list->next;

		} /* while (temp_list->next != tail_list) */

		/* Make all monitor handle dirty flag clean. */
		__set_all_monitor_clean();
	}

	/* Find if resource confilct exists in case of 1Pause 2Play or 1Stop 2Play */
	if (ASM_STATUS_NONE != g_sound_status_pause && mm_resource != ASM_RESOURCE_NONE &&
		(asm_snd_msg.data.result_sound_command == ASM_COMMAND_PLAY || no_conflict_flag)) {
		asm_instance_list_t *temp_list = head_list;
		int cb_res = 0;
		
		while (temp_list->next != tail_list) {
			/* Who is in PAUSE state? */
			if (temp_list->sound_state == ASM_STATE_PAUSE) {
				/* Found PAUSE state */
				asm_warning("[ASM_Server] Now list's state is pause. %d %d\n", instance_id, handle);
				ASM_sound_states_t current_play_state = temp_list->sound_state;
				ASM_sound_events_t current_play_sound_event = temp_list->sound_event;
				long int current_play_instance_id = temp_list->instance_id;
				int current_play_handle = temp_list->sound_handle;
				ASM_resource_t current_using_resource = temp_list->mm_resource;

				if ((current_play_instance_id == instance_id) && (current_play_handle == handle)) {
					if (request_id == ASM_REQUEST_SETSTATE) {
						asm_warning("[ASM_Server] Own handle. Pause state change to play. %d %d\n", instance_id, handle);
						__asm_change_state_list(instance_id, handle, sound_state, mm_resource);
					} else {
						asm_warning("[ASM_Server] This is my handle. skip %d %d\n", instance_id, handle);
					}
					temp_list = temp_list->next;
					continue;
				}

				if (sound_state == ASM_STATE_PLAYING) {
					ASM_sound_cases_t sound_case = ASM_sound_case[current_play_sound_event][sound_event];

					asm_critical("[ASM_Server] Conflict policy[%x][%x]: %s\n", current_play_sound_event, sound_event, ASM_sound_cases_str[sound_case]);
					switch (sound_case) {
					case ASM_CASE_1PAUSE_2PLAY:
					case ASM_CASE_1STOP_2PLAY:
					{
						if (current_play_instance_id == instance_id) {
							//PID is policy group.
							asm_info("[ASM_Server] Do not send Stop callback in same pid %ld\n", instance_id);
						} else {
							unsigned short resource_status = current_using_resource & mm_resource;

							/* Check conflict with paused instance */
							if (resource_status != ASM_RESOURCE_NONE) {
								asm_warning("[ASM_Server] there is system resource conflict with paused instance 0x%x\n",resource_status);
								__print_resource(resource_status);
							} else {
								asm_info("[ASM_Server] no resource conflict with paused instance\n");
								break;
							}

							/* Execute callback function for monitor handle */
							int monitor_handle = -1;
							if (__find_clean_monitor_handle(current_play_instance_id, &monitor_handle)) {
								cb_res = __do_callback(current_play_instance_id, monitor_handle, ASM_COMMAND_STOP, ASM_EVENT_SOURCE_RESOURCE_CONFLICT);
								if (cb_res != ASM_CB_RES_NONE && cb_res != ASM_CB_RES_STOP) {
									asm_error_r("[ASM_Server] oops! not suspected callback result %d\n", cb_res);
								}
								asm_warning("[ASM_Server] send stop callback for monitor handle of pid %d\n", current_play_instance_id);

								__set_monitor_dirty(current_play_instance_id);
							}

							/* Execute callback function for worker handle */
							cb_res = __do_callback(current_play_instance_id,current_play_handle, ASM_COMMAND_STOP, ASM_EVENT_SOURCE_RESOURCE_CONFLICT);
							if (cb_res != ASM_CB_RES_NONE && cb_res != ASM_CB_RES_STOP) {
								asm_error_r("[ASM_Server] oops! not suspected result %d\n", cb_res);
							}
							asm_warning("[ASM_Server]  __asm_compare_priority_matrix(1STOP_2PLAY) cause RESOURCE : __do_callback Complete : TID=%ld, handle=%d",current_play_instance_id,current_play_handle );

							__asm_change_state_list(current_play_instance_id, current_play_handle, ASM_STATE_NONE, ASM_RESOURCE_NONE);
						}

						asm_info("[ASM_Server] >>>> __asm_compare_priority_matrix(1STOP_2PLAY) cause RESOURCE : msg sent and  then received msg !!!\n");
						break;
					}

					default:
					{
						/* asm_warning("[ASM_Server] >>>> __asm_compare_priority_matrix : ASM_CASE_NONE [do not anything] !!!\n"); */
						break;
					}
					} /* switch (sound_case) */
				} else {
					/* asm_warning("[ASM_Server] >>>> __asm_compare_priority_matrix : ASM_CASE_NONE [do not anything] !!!\n"); */
				}
			} /* if (temp_list->sound_state == ASM_STATE_PAUSE) */

			temp_list = temp_list->next;
		} /* while (temp_list->next != tail_list) */
	}

	/* Finally, no conflict */
	if (no_conflict_flag) {
		if (request_id == ASM_REQUEST_REGISTER) {
			__asm_register_list(instance_id, handle, sound_event, sound_state, mm_resource);
		} else {
			__asm_change_state_list(instance_id, handle, sound_state, mm_resource);
		}
	}

	/* Send response to client */
	asm_snd_msg.instance_id = instance_id;
	__asm_snd_message();
}

void __asm_do_all_resume_callback(ASM_event_sources_t eventsrc)
{
	asm_instance_list_t *temp_list = head_list;
	int cb_res = 0;
	int avsys_err = 0;
	avsys_audio_playing_devcie_t dev = AVSYS_AUDIO_ROUTE_DEVICE_UNKNOWN;

	asm_info("[ASM_Server] __asm_do_all_resume_callback\n");

	if (AVSYS_FAIL(avsys_err = avsys_audio_get_playing_device_info(&dev))) {
		asm_error("[ASM_Server] Can not get playing device info\n");
	}

	while (temp_list->next != tail_list) {
		if (temp_list->need_resume == ASM_NEED_RESUME) {
			if ((avsys_err != AVSYS_STATE_SUCCESS) ||
					(dev == AVSYS_AUDIO_ROUTE_DEVICE_HANDSET && temp_list->device_when_interrupted != AVSYS_AUDIO_ROUTE_DEVICE_HANDSET)) {
				asm_warning("[ASM_Server] Skip send resume callback. Because sound path has changed to loud during interruption.\n");
			} else {
				cb_res = __do_callback(temp_list->instance_id, temp_list->sound_handle, ASM_COMMAND_RESUME, eventsrc);
				switch (cb_res) {
				case ASM_CB_RES_PLAYING:
					temp_list->sound_state = ASM_STATE_PLAYING;
					break;
				case ASM_CB_RES_IGNORE:
				case ASM_CB_RES_NONE:
				case ASM_CB_RES_STOP:
				case ASM_CB_RES_PAUSE:
				default:
					/* do nothing */
					break;
				}
			}
			temp_list->need_resume = ASM_NEED_NOT_RESUME;
		}
		temp_list = temp_list->next;
	}
}

#ifdef USE_SECURITY
gboolean __asm_check_check_privilege (unsigned char* cookie)
{
	int asm_gid = -1;
	int retval = 0;

	/* Get ASM server group id */
	asm_gid = security_server_get_gid("asm");
	asm_info ("[Security] asm server gid = [%d]\n", asm_gid);
	if (asm_gid < 0) {
		asm_error ("[Security] security_server_get_gid() failed. error=[%d]\n", asm_gid);
		return false;
	}

	/* Check privilege with valid group id */
	retval = security_server_check_privilege((char *)cookie, asm_gid);
	if (retval == SECURITY_SERVER_API_SUCCESS) {
		asm_info("[Security] security_server_check_privilege() returns [%d]\n", retval);
		return true;
	} else {
		asm_error("[Security] security_server_check_privilege() returns [%d]\n", retval);
		return false;
	}
}
#endif /* USE_SECURITY */

int main()
{
	int pid = 0, ret = 0;

	if (sysconf_set_mempolicy(OOM_IGNORE)) {
		fprintf(stderr, "set mem policy failed\n");
	}
	signal(SIGPIPE, SIG_IGN);
#if !defined(USE_SYSTEM_SERVER_PROCESS_MONITORING)
	while (1) {
		if (( pid = fork()) < 0) {
			asm_critical_r("[ASM_Server]Fork Failed!!!\n");
			return 0;
		} else if (pid == 0) {
			break;
		} else if (pid > 0) {
			wait(&ret);
			asm_critical_r("[ASM_Server]worker(child) dead!!!, try to refork()");
		}
	}
#endif
	__asm_create_message_queue();

	int temp_msgctl_id1 = msgctl(asm_snd_msgid, IPC_RMID, 0);
	int temp_msgctl_id2 = msgctl(asm_rcv_msgid, IPC_RMID, 0);
	int temp_msgctl_id3 = msgctl(asm_cb_msgid, IPC_RMID, 0);


	if (temp_msgctl_id1 == -1 || temp_msgctl_id2 == -1 || temp_msgctl_id3 == -1) {
		asm_info("[ASM_Server] msgctl failed with error: \n");
		exit(EXIT_FAILURE);
	}
	//-------------------------------------------------------------------
	/*
		This is unnessasry finaly, but nessasary during implement.
	*/

	__asm_create_message_queue();

	head_list = (asm_instance_list_t *)malloc(sizeof(asm_instance_list_t));
	tail_list = (asm_instance_list_t *)malloc(sizeof(asm_instance_list_t));
	head_list->next = tail_list;
	tail_list->next = tail_list;


	long int rcv_instance_id;
	ASM_requests_t rcv_request_id;
	ASM_sound_events_t rcv_sound_event;
	ASM_sound_states_t rcv_sound_state;
	ASM_resource_t rcv_resource;
	int rcv_sound_handle;

	/*
	 * Init Vconf
	 */
	if (vconf_set_int(SOUND_STATUS_KEY, 0)) {
		asm_critical("[ASM_Server] vconf_set_int fail\n");
		if (vconf_set_int(SOUND_STATUS_KEY, 0)) {
			asm_critical_r("[ASM_Server] vconf_set_int fail\n");
		}
	}
	if (vconf_set_int(HIBERNATION_CHECK_KEY, HIBERNATION_READY)) {
		asm_critical("[ASM_Server] Hibernation check vconf_set_int fail\n");
	}

#ifdef SUPPORT_GCF
	if (vconf_get_int(VCONFKEY_ADMIN_GCF_TEST, &is_gcf)) {
		asm_warning_r("[ASM_Server] vconf_get_int for VCONFKEY_ADMIN_GCF_TEST failed, set as default\n");
		is_gcf = GCF_DEFAULT;
	}
#endif

	while (true) {
		asm_info("[ASM_Server] asm_Server is waiting message(%d)!!!\n", asm_is_send_msg_to_cb);

		if (!asm_is_send_msg_to_cb) {
			__asm_rcv_message();

			rcv_instance_id = asm_rcv_msg.instance_id;
			rcv_sound_handle = asm_rcv_msg.data.handle;
			rcv_request_id = asm_rcv_msg.data.request_id;
			rcv_sound_event = asm_rcv_msg.data.sound_event;
			rcv_sound_state = asm_rcv_msg.data.sound_state;
			rcv_resource = asm_rcv_msg.data.system_resource;

			/*******************************************************************/
			asm_info("[ASM_Server] received msg (tid=%ld,handle=%d,req=%d,event=0x%x,state=0x%x,resource=0x%x)\n",
					rcv_instance_id, rcv_sound_handle, rcv_request_id, rcv_sound_event, rcv_sound_state, rcv_resource);
			if (rcv_request_id != ASM_REQUEST_EMERGENT_EXIT) {
				asm_warning("[ASM_Server]     request_id : %s\n", ASM_sound_request_str[rcv_request_id]);
				asm_warning("[ASM_Server]     sound_event : %s\n", ASM_sound_events_str[rcv_sound_event]);
				asm_warning("[ASM_Server]     sound_state : %s\n", ASM_sound_state_str[rcv_sound_state]);
				asm_warning("[ASM_Server]     resource : 0x%x\n", rcv_resource);
			}
			/*******************************************************************/

			switch (rcv_request_id) {
			case ASM_REQUEST_REGISTER:
#ifdef USE_SECURITY
				/* do security check */
				if (__asm_check_check_privilege(asm_rcv_msg.data.cookie) == 0) {
					asm_error ("[Security] __asm_check_check_privilege() failed....\n");
					asm_snd_msg.instance_id = rcv_instance_id;
					asm_snd_msg.data.alloc_handle = -1;
					asm_snd_msg.data.cmd_handle = -1;
					asm_snd_msg.data.check_privilege = 0;
					__asm_snd_message();
					break;
				}
				asm_info ("[Security] __asm_check_check_privilege() success\n");
				asm_snd_msg.data.check_privilege = 1;
#endif /* USE_SECURITY */
				__check_dead_process();

				__asm_get_empty_handle(rcv_instance_id, &rcv_sound_handle);
				if (rcv_sound_handle == -1) {
					asm_snd_msg.instance_id = rcv_instance_id;
					asm_snd_msg.data.alloc_handle = -1;
					asm_snd_msg.data.cmd_handle = -1;
					__asm_snd_message();
				} else {
					__asm_compare_priority_matrix(rcv_instance_id, rcv_sound_handle, rcv_request_id, rcv_sound_event, rcv_sound_state, rcv_resource);
				}

				break;

			case ASM_REQUEST_UNREGISTER:
				__asm_unregister_list(rcv_sound_handle);
				/* only support resuming at end of call & alarm interrupt */
				switch (rcv_sound_event) {
				case ASM_EVENT_CALL:
				case ASM_EVENT_VIDEOCALL:
					__asm_do_all_resume_callback(ASM_EVENT_SOURCE_CALL_END);
					break;
				case ASM_EVENT_ALARM:
					__asm_do_all_resume_callback(ASM_EVENT_SOURCE_ALARM_END);
					break;
				default:
					break;
				}

				break;

			case ASM_REQUEST_SETSTATE:
				__check_dead_process();
				if ( rcv_sound_state == ASM_STATE_PLAYING )	{
					if ( __isItPlayingNow(rcv_instance_id, rcv_sound_handle)) {
						__asm_change_state_list(rcv_instance_id, rcv_sound_handle, rcv_sound_state, rcv_resource);

						asm_snd_msg.data.cmd_handle = rcv_sound_handle;
						asm_snd_msg.data.result_sound_command = ASM_COMMAND_NONE;
						asm_snd_msg.data.result_sound_state = rcv_sound_state;
						asm_snd_msg.instance_id = rcv_instance_id;

						__asm_snd_message();
					} else {
						__asm_compare_priority_matrix(rcv_instance_id, rcv_sound_handle, rcv_request_id, rcv_sound_event, rcv_sound_state, rcv_resource);
					}
					__temp_print_list("Set State (Play)");
				} else {
					__asm_change_state_list(rcv_instance_id, rcv_sound_handle, rcv_sound_state, rcv_resource);
					__temp_print_list("Set State (Not Play)");
				}
				break;

			case ASM_REQUEST_GETSTATE:
				asm_snd_msg.instance_id = rcv_instance_id;
				asm_snd_msg.data.result_sound_state = __asm_find_list(rcv_request_id, rcv_sound_handle);
				__asm_snd_message();
				break;

			case ASM_REQUEST_GETMYSTATE:
				__check_dead_process();
				asm_snd_msg.instance_id = rcv_instance_id;
				asm_snd_msg.data.result_sound_state = __asm_find_process_status(rcv_instance_id);
				__asm_snd_message();
				break;

			case ASM_REQUEST_DUMP:
				__temp_print_list("DUMP");
				break;

			case ASM_REQUEST_EMERGENT_EXIT:
				emergent_exit(rcv_instance_id);
				break;

			default:
				break;
			}
		}
	}

	return 0;
}

