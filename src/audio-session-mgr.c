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

#define CONFIG_ENABLE_MULTI_INSTANCE
#define CONFIG_ENABLE_ASM_SERVER_USING_GLIB
#define CONFIG_ENABLE_SIGNAL_HANDLER
#define CONFIG_ENABLE_RETCB
#define MAKE_HANDLE_FROM_SERVER

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <sys/poll.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <mm_debug.h>

#ifdef USE_SECURITY
#include <security-server.h>
#endif

#if defined(USE_VCONF)
#include <vconf.h>
#include <errno.h>
#else
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <phonestatus.h>

#endif
#include "../include/audio-session-manager.h"

#define asmgettid() (long int)getpid()
#define ASM_HANDLE_MAX 256

#define NO_EINTR(stmt) while ((stmt) == -1 && errno == EINTR);  /* sample code by THE LINUX PROGRAMMING INTERFACE */

int asm_register_instance_id;

int asm_snd_msgid;
int asm_rcv_msgid;
int asm_cb_msgid;

ASM_msg_lib_to_asm_t asm_snd_msg;
ASM_msg_asm_to_lib_t asm_rcv_msg;
ASM_msg_asm_to_cb_t asm_cb_msg;

ASM_sound_cb_t 	asm_callback;

unsigned char 		str_pass[] = "< OK >";
unsigned char 		str_fail[] = "<FAIL>";

typedef gboolean (*gLoopPollHandler_t)(gpointer d);

typedef struct
{
	ASM_sound_events_t	sound_event;
	GSourceFuncs*		g_src_funcs;
	GPollFD*		g_poll_fd;
	int			asm_fd;
	ASM_sound_cb_t 		asm_callback;
	void			*cb_data;
	int			asm_tid;
	int			handle;
	bool			is_used;
	GSource*		asm_src;
	guint			ASM_g_id;
} ASM_sound_ino_t;

ASM_sound_ino_t ASM_sound_handle[ASM_HANDLE_MAX];

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
	"MONITOR",
	"RICH_CALL",
	"EMERGENCY",
	"EXCLUSIVE_RESOURCE"
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
	"CASE_1PLAY_2PLAY_MIX",
	"CASE_RESOURCE_CHECK"
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

/*
 * function prototypes
 */


unsigned int ASM_all_sound_status;

void __asm_init_module(void);
void __asm_fini_module(void);
int __ASM_find_index(int handle);

bool __ASM_get_sound_state(unsigned int *all_sound_status, int *error_code)
{
	int value = 0;

	if(vconf_get_int(SOUND_STATUS_KEY, &value)) {
		debug_error("failed to vconf_get_int(SOUND_STATUS_KEY)");
		*error_code = ERR_ASM_VCONF_ERROR;
		return false;
	}
	debug_msg("All status(%#X)", value);
	*all_sound_status = value;
	ASM_all_sound_status = value;

	return true;
}

bool __ASM_set_sound_state(ASM_sound_events_t sound_event, ASM_sound_states_t sound_state, int *error_code)
{
	/* this function will deprecated */
	debug_msg("Event(%s), State(%s)", ASM_sound_events_str[sound_event], ASM_sound_state_str[sound_state]);

	return true;
}

gboolean __asm_fd_check(GSource * source)
{
	GSList *fd_list;
	if (!source) {
		debug_error("GSource is null");
		return FALSE;
	}
	fd_list = source->poll_fds;
	GPollFD *temp;

	do {
		temp = (GPollFD*)fd_list->data;
		if (temp->revents & (POLLIN | POLLPRI))
			return TRUE;
		fd_list = fd_list->next;
	} while (fd_list);

	return FALSE; /* there is no change in any fd state */
}


gboolean __asm_fd_prepare(GSource *source, gint *timeout)
{
	return FALSE;
}

gboolean __asm_fd_dispatch(GSource *source,	GSourceFunc callback, gpointer user_data)
{
	callback(user_data);
	return TRUE;
}

gboolean asm_callback_handler( gpointer d)
{
	GPollFD *data = (GPollFD*)d;
	unsigned int buf;
	int count;
	int tid = 0;
	int asm_index = 0;
	debug_fenter();

	if (!data) {
		debug_error("GPollFd is null");
		return FALSE;
	}
	if (data->revents & (POLLIN | POLLPRI)) {
		int handle;
		int error_code = 0;
		int event_src;
		unsigned int sound_status_value;
		ASM_sound_commands_t rcv_command;
		ASM_cb_result_t cb_res = ASM_CB_RES_NONE;


		count = read(data->fd, &buf, sizeof(int));

		handle = (int)( buf & 0x0000ffff);
		rcv_command = (ASM_sound_commands_t)((buf >> 16) & 0x0f);
		event_src = (ASM_event_sources_t)((buf >> 24) & 0x0f);

		asm_index = __ASM_find_index(handle);
		if (asm_index == -1) {
			debug_error("Can not find index");
			return FALSE;
		}

		tid = ASM_sound_handle[asm_index].asm_tid;
		
		if (rcv_command) {
			debug_msg("got and start CB : TID(%d), handle(%d) cmd(%d) event_src(%d)", tid, handle, rcv_command, event_src );
			if (!__ASM_get_sound_state(&sound_status_value, &error_code)) {
				debug_error("failed to __ASM_get_sound_state(), error(%d)", error_code);
			}
			switch (rcv_command) {
			case ASM_COMMAND_PLAY:
			case ASM_COMMAND_RESUME:
			case ASM_COMMAND_PAUSE:
			case ASM_COMMAND_STOP:
				if (ASM_sound_handle[asm_index].asm_callback == NULL) {
					debug_msg("callback is null..");
					return FALSE;
				}
				debug_msg("[CALLBACK(%p) START]",ASM_sound_handle[asm_index].asm_callback);
				cb_res = (ASM_sound_handle[asm_index].asm_callback)(handle, event_src, rcv_command, sound_status_value, ASM_sound_handle[asm_index].cb_data);
				debug_msg("[CALLBACK END]");
				break;
			default:
				break;
			}
#ifdef CONFIG_ENABLE_RETCB

			/* If command is other than RESUME, send return */
			if (rcv_command != ASM_COMMAND_RESUME) {
				int rett = 0;
				int buf = cb_res;
				int tmpfd = -1;
				char *filename2 = g_strdup_printf("/tmp/ASM.%d.%dr", ASM_sound_handle[asm_index].asm_tid, handle);
				tmpfd = open(filename2, O_WRONLY | O_NONBLOCK);
				if (tmpfd < 0) {
					char str_error[256];
					strerror_r(errno, str_error, sizeof(str_error));
					debug_error("[RETCB][Failed(May Server Close First)]tid(%d) fd(%d) %s errno=%d(%s)\n", tid, tmpfd, filename2, errno, str_error);
					g_free(filename2);
					return FALSE;
				}
				rett = write(tmpfd, &buf, sizeof(buf));
				close(tmpfd);
				g_free(filename2);
				debug_msg("[RETCB] tid(%d) finishing CB (write=%d)\n", tid, rett);
			} else {
				debug_msg("[RETCB] No need to send return for RESUME command\n");
			}
#endif
		}
	}
	debug_fleave();
	return TRUE;
}

bool __ASM_add_sound_callback(int index, int fd, gushort events, gLoopPollHandler_t p_gloop_poll_handler )
{
	GSource* g_src = NULL;
	GSourceFuncs *g_src_funcs = NULL;		/* handler function */
	guint gsource_handle;
	GPollFD *g_poll_fd = NULL;			/* file descriptor */

	/* 1. make GSource Object */
	g_src_funcs = (GSourceFuncs *)g_malloc(sizeof(GSourceFuncs));
	if (!g_src_funcs) {
		debug_error("g_malloc failed on g_src_funcs");
		return false;
	}
	g_src_funcs->prepare = __asm_fd_prepare;
	g_src_funcs->check = __asm_fd_check;
	g_src_funcs->dispatch = __asm_fd_dispatch;
	g_src_funcs->finalize = NULL;
	g_src = g_source_new(g_src_funcs, sizeof(GSource));
	if (!g_src) {
		debug_error("g_malloc failed on m_readfd");
		return false;
 	}
	ASM_sound_handle[index].asm_src = g_src;
	ASM_sound_handle[index].g_src_funcs = g_src_funcs;
	debug_msg(" g_malloc : g_src_funcs(%#X)", g_src_funcs);

	/* 2. add file description which used in g_loop() */
	g_poll_fd = (GPollFD *)g_malloc(sizeof(GPollFD));
	if (!g_poll_fd) {
		debug_error("g_malloc failed on g_poll_fd");
		return false;
	}
	g_poll_fd->fd = fd;
	g_poll_fd->events = events;
	ASM_sound_handle[index].g_poll_fd = g_poll_fd;
	debug_msg(" g_malloc : g_poll_fd(%#X)", g_poll_fd);

	/* 3. combine g_source object and file descriptor */
	g_source_add_poll(g_src, g_poll_fd);
	gsource_handle = g_source_attach(g_src, NULL);
 	if (!gsource_handle) {
		debug_error(" Failed to attach the source to context");
		return false;
 	}

	debug_msg(" g_source_add_poll : g_src_id(%d)", gsource_handle);
 	ASM_sound_handle[index].ASM_g_id = gsource_handle;

	/* 4. set callback */
	g_source_set_callback(g_src, p_gloop_poll_handler,(gpointer)g_poll_fd, NULL);

	debug_msg(" g_source_set_callback : %d", errno);
	return true;
}


bool __ASM_remove_sound_callback(int index, gushort events)
{
	bool ret = true;
	gboolean gret = TRUE;

	GSourceFunc *g_src_funcs = ASM_sound_handle[index].g_src_funcs;
	GPollFD *g_poll_fd = ASM_sound_handle[index].g_poll_fd;	/* store file descriptor */
	if (!g_poll_fd) {
		debug_error("g_poll_fd is null..");
		ret = false;
		goto init_handle;
	}
	g_poll_fd->fd = ASM_sound_handle[index].asm_fd;
	g_poll_fd->events = events;

	debug_msg(" g_source_remove_poll : fd(%d), event(%x)", g_poll_fd->fd, g_poll_fd->events);
	g_source_remove_poll(ASM_sound_handle[index].asm_src, g_poll_fd);
	debug_msg(" g_source_remove_poll : %d", errno);

	gret = g_source_remove(ASM_sound_handle[index].ASM_g_id);
	debug_msg(" g_source_remove : gret(%d)", gret);
	if (!gret) {
		debug_error("failed to g_source_remove(). ASM_g_id(%d)", ASM_sound_handle[index].ASM_g_id);
		ret = false;
		goto init_handle;
	}

init_handle:

	if (g_src_funcs) {
		debug_msg(" g_free : g_src_funcs(%#X)", g_src_funcs);
		g_free(g_src_funcs);

	}
	if (g_poll_fd) {
		debug_msg(" g_free : g_poll_fd(%#X)", g_poll_fd);
		g_free(g_poll_fd);
	}

	ASM_sound_handle[index].g_src_funcs = NULL;
	ASM_sound_handle[index].g_poll_fd = NULL;
	ASM_sound_handle[index].ASM_g_id = 0;
	ASM_sound_handle[index].asm_src = NULL;
	ASM_sound_handle[index].asm_callback = NULL;

	return ret;
}

int __ASM_find_index(int handle)
{
	int i = 0;
	for(i = 0; i< ASM_HANDLE_MAX; i++) {
		if (handle == ASM_sound_handle[i].handle) {
			debug_msg("found index(%d) for handle(%d)", i, handle);
			return i;
		}
	}
	return -1;
}

void __ASM_add_callback(int index)
{
	if (!__ASM_add_sound_callback(index, ASM_sound_handle[index].asm_fd, (gushort)POLLIN | POLLPRI, asm_callback_handler)) {
		debug_error("failed to __ASM_add_sound_callback()");
		//return false;
	}
}


void __ASM_remove_callback(int index)
{
	if (!__ASM_remove_sound_callback(index, (gushort)POLLIN | POLLPRI)) {
		debug_error("failed to __ASM_remove_sound_callback()");
		//return false;
	}
}


void __ASM_open_callback(int index)
{
	mode_t pre_mask;

	debug_msg("index (%d)", index);
	char *filename = g_strdup_printf("/tmp/ASM.%d.%d", ASM_sound_handle[index].asm_tid, ASM_sound_handle[index].handle);
	pre_mask = umask(0);
	if (mknod(filename, S_IFIFO|0666, 0)) {
		debug_error("mknod() failure, errno(%d)", errno);
	}
	umask(pre_mask);
	ASM_sound_handle[index].asm_fd = open( filename, O_RDWR|O_NONBLOCK);
	if (ASM_sound_handle[index].asm_fd == -1) {
		debug_error("%s : file open error(%d)", str_fail, errno);
	} else {
		debug_msg("%s : filename(%s), fd(%d)", str_pass, filename, ASM_sound_handle[index].asm_fd);
	}
	g_free(filename);

#ifdef CONFIG_ENABLE_RETCB
	char *filename2 = g_strdup_printf("/tmp/ASM.%d.%dr", ASM_sound_handle[index].asm_tid,  ASM_sound_handle[index].handle);
	pre_mask = umask(0);
	if (mknod(filename2, S_IFIFO | 0666, 0)) {
		debug_error("mknod() failure, errno(%d)", errno);
	}
	umask(pre_mask);
	g_free(filename2);
#endif

}


void __ASM_close_callback(int index)
{
	debug_msg("index (%d)", index);
	if (ASM_sound_handle[index].asm_fd < 0) {
		debug_error("%s : fd error.", str_fail);
	} else {
		char *filename = g_strdup_printf("/tmp/ASM.%d.%d", ASM_sound_handle[index].asm_tid, ASM_sound_handle[index].handle);
		close(ASM_sound_handle[index].asm_fd);
		if (remove(filename)) {
			debug_error("remove() failure, filename(%s), errno(%d)", filename, errno);
		}
		debug_msg("%s : filename(%s), fd(%d)", str_pass, filename, ASM_sound_handle[index].asm_fd);
		g_free(filename);
	}

#ifdef CONFIG_ENABLE_RETCB
	char *filename2 = g_strdup_printf("/tmp/ASM.%d.%dr", ASM_sound_handle[index].asm_tid, ASM_sound_handle[index].handle);

	/* Defensive code - wait until callback timeout although callback is removed */
	int buf = ASM_CB_RES_STOP;
	int tmpfd = -1;

	tmpfd = open(filename2, O_WRONLY | O_NONBLOCK);
	if (tmpfd < 0) {
		char str_error[256];
		strerror_r(errno, str_error, sizeof(str_error));
		debug_error("failed to open file(%s) (may server close first), tid(%d) fd(%d) %s errno=%d(%s)",
			filename2, ASM_sound_handle[index].asm_tid, tmpfd, filename2, errno, str_error);
	} else {
		debug_msg("write ASM_CB_RES_STOP(tid:%d) for waiting server", ASM_sound_handle[index].asm_tid);
		write(tmpfd, &buf, sizeof(buf));
		close(tmpfd);
	}

	if (remove(filename2)) {
		debug_error("remove() failure, filename(%s), errno(%d)", filename2, errno);
	}
	g_free(filename2);
#endif

}

bool __asm_construct_snd_msg(int asm_pid, int handle, ASM_sound_events_t sound_event,
					ASM_requests_t request_id, ASM_sound_states_t sound_state, ASM_resource_t resource, int *error_code)
{
	asm_snd_msg.instance_id = asm_pid;

	asm_snd_msg.data.handle = handle;
	asm_snd_msg.data.request_id = request_id;
	asm_snd_msg.data.sound_event = sound_event;
	asm_snd_msg.data.sound_state = sound_state;
	asm_snd_msg.data.system_resource = resource;

	debug_msg("tid=%ld,handle=%d,req=%d,evt=%d,state=%d,resource=%d", asm_snd_msg.instance_id, asm_snd_msg.data.handle,
				asm_snd_msg.data.request_id, asm_snd_msg.data.sound_event, asm_snd_msg.data.sound_state, asm_snd_msg.data.system_resource);
	debug_msg("     instance_id : %ld\n", asm_snd_msg.instance_id);
	debug_msg("     handle : %d\n", asm_snd_msg.data.handle);

	return true;
}


bool __ASM_init_msg(int *error_code)
{
	asm_snd_msgid = msgget((key_t)2014, 0666);
	asm_rcv_msgid = msgget((key_t)4102, 0666);
	asm_cb_msgid = msgget((key_t)4103, 0666);

	debug_msg("snd_msqid(%#X), rcv_msqid(%#X), cb_msqid(%#X)\n", asm_snd_msgid, asm_rcv_msgid, asm_cb_msgid);

	if (asm_snd_msgid == -1 || asm_rcv_msgid == -1 || asm_cb_msgid == -1 ) {
		*error_code = ERR_ASM_MSG_QUEUE_MSGID_GET_FAILED;
		debug_error("failed to msgget with error(%d)",error_code);
		return false;
	}

	return true;
}

void __ASM_init_callback(int index)
{
	debug_fenter();
	__ASM_open_callback(index);
	__ASM_add_callback(index);
	debug_fleave();
}


void __ASM_destroy_callback(int index)
{
	debug_fenter();
	__ASM_remove_callback(index);
	__ASM_close_callback(index);
	debug_fleave();
}

#ifdef USE_SECURITY
bool __ASM_set_cookie (unsigned char* cookie)
{
	int retval = -1;
	int cookie_size = 0;

	cookie_size = security_server_get_cookie_size();
	if (cookie_size != COOKIE_SIZE) {
		debug_error ("[Security] security_server_get_cookie_size(%d)  != COOKIE_SIZE(%d)\n", cookie_size, COOKIE_SIZE);
		return false;
	}

	retval = security_server_request_cookie (cookie, COOKIE_SIZE);
	if (retval == SECURITY_SERVER_API_SUCCESS) {
		debug_msg ("[Security] security_server_request_cookie() returns [%d]\n", retval);
		return true;
	} else {
		debug_error ("[Security] security_server_request_cookie() returns [%d]\n", retval);
		return false;
	}
}
#endif

EXPORT_API
bool ASM_register_sound_ex (const int application_pid, int *asm_handle, ASM_sound_events_t sound_event, ASM_sound_states_t sound_state,
						ASM_sound_cb_t callback, void *cb_data, ASM_resource_t mm_resource, int *error_code, int (*func)(void*,void*))
{
	int error = 0;
	unsigned int sound_status_value;
	int handle = 0;
	int asm_pid = 0;
	int index = 0;
	int ret = 0;
	unsigned int rcv_sound_status_value;

	debug_fenter();

	if (error_code==NULL) {
		debug_error ("invalid parameter. error code is null");
		return false;
	}

	if (sound_event< ASM_EVENT_SHARE_MMPLAYER || sound_event >= ASM_EVENT_MAX) {
		*error_code = ERR_ASM_EVENT_IS_INVALID;
		debug_error ("invalid sound event(%d)",sound_event);
		return false;
	}

	for (index = 0; index < ASM_HANDLE_MAX; index++) {
		if (ASM_sound_handle[index].is_used == false) {
			break;
		}
	}

	if (index == ASM_HANDLE_MAX) {
		*error_code = ERR_ASM_EVENT_IS_FULL;
		debug_error ("local sound event is full(MAX)");
		return false;
	}

	if (application_pid == -1) {
		asm_pid = asmgettid();
	} else if (application_pid > 2) {
		asm_pid = application_pid;
	} else {
		*error_code = ERR_ASM_INVALID_PARAMETER;
		debug_error ("invalid pid %d", application_pid);
		return false;
	}

	ASM_sound_handle[index].sound_event = sound_event;
	ASM_sound_handle[index].asm_tid = asm_pid;

	debug_msg(" <<<< Event(%s), Tid(%d), Index(%d)", ASM_sound_events_str[sound_event], ASM_sound_handle[index].asm_tid, index);

	if (!__ASM_init_msg(&error)) {
		debug_error("failed to __ASM_init_msg(), error(%d)", error);
	}

	if (!__ASM_get_sound_state(&sound_status_value, error_code)) {
		debug_error("failed to __ASM_get_sound_state(), error(%d)", *error_code);
	}

	/* If state is PLAYING, do set state immediately */
	if (sound_state == ASM_STATE_PLAYING) {
		debug_msg(" <<<< Event(%s), Tid(%d), State(PLAYING)", ASM_sound_events_str[sound_event], ASM_sound_handle[index].asm_tid);
		if (!__ASM_set_sound_state(sound_event, ASM_STATE_PLAYING, error_code)) {
			debug_error("failed to __ASM_set_sound_state(PLAYING), error(%d)", *error_code);
			return false;
		}
	} else {
		debug_msg(" <<<< Event(%s), Tid(%ld), State(WAITING)", ASM_sound_events_str[sound_event], asmgettid());
	}


	handle = -1; /* for register & get handle from server */

#ifdef USE_SECURITY
	/* get cookie from security server */
	if (__ASM_set_cookie (asm_snd_msg.data.cookie) == false) {
		debug_error("failed to ASM_set_cookie()");
		return false;
	}
#endif

	/* Construct msg to send -> send msg -> recv msg */
	if (!__asm_construct_snd_msg(asm_pid, handle, sound_event, ASM_REQUEST_REGISTER, sound_state, mm_resource, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}

	if (func) {
		func ((void*)&asm_snd_msg, (void*)&asm_rcv_msg);
	} else	{
		NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
		if (ret == -1) {
			*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
			debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
			return false;
		}
		NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), asm_pid, 0));
		if (ret == -1) {
			*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
			debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
			return false;
		}
	}
	/* Construct msg to send -> send msg -> recv msg : end */

#ifdef USE_SECURITY
	/* Check privilege first */
	if (asm_rcv_msg.data.check_privilege == 0) {
		debug_error("[Security] Check privilege from server Failed!!!");
		*error_code = ERR_ASM_CHECK_PRIVILEGE_FAILED;
		return false;
	} else {
		debug_msg ("[Security] Check privilege from server Success");
	}
#endif

	handle = asm_rcv_msg.data.alloc_handle; /* get handle from server */
	if (handle == -1) {
		debug_error("failed to create handle from server");
		*error_code = ERR_ASM_HANDLE_IS_FULL;
		return false;
	}

	ASM_sound_handle[index].handle = handle;

	__ASM_init_callback(index);

/********************************************************************************************************/
	switch (asm_rcv_msg.data.result_sound_command) {
	case ASM_COMMAND_PAUSE:
	case ASM_COMMAND_STOP:
		if (handle == asm_rcv_msg.data.cmd_handle) {

			__ASM_destroy_callback(index);

			ASM_sound_handle[index].asm_fd = 0;
			ASM_sound_handle[index].asm_tid = 0;
			ASM_sound_handle[index].sound_event = ASM_EVENT_NONE;
			ASM_sound_handle[index].is_used = false;

			*error_code = ERR_ASM_POLICY_CANNOT_PLAY;
			return false;
		} else {
			int action_index = 0;

			if (!__ASM_get_sound_state(&rcv_sound_status_value, error_code)) {
				debug_error("failed to __ASM_get_sound_state(), error(%d)", *error_code);
			}

			debug_msg("Callback : TID(%ld), handle(%d), command(%d)", asm_rcv_msg.instance_id, asm_rcv_msg.data.cmd_handle, asm_rcv_msg.data.result_sound_command);
			action_index = __ASM_find_index(asm_rcv_msg.data.cmd_handle);
			if (action_index == -1) {
				debug_error("Can not find index of instance %ld, handle %d", asm_rcv_msg.instance_id, asm_rcv_msg.data.cmd_handle);
			} else {
				if (ASM_sound_handle[action_index].asm_callback!=NULL) {
					ASM_sound_handle[action_index].asm_callback(asm_rcv_msg.data.cmd_handle, ASM_sound_handle[action_index].sound_event, asm_rcv_msg.data.result_sound_command, rcv_sound_status_value, ASM_sound_handle[action_index].cb_data);
				} else {
					debug_msg("null callback");
				}
			}
		}
		break;

	case ASM_COMMAND_PLAY:
	case ASM_COMMAND_NONE:
	case ASM_COMMAND_RESUME:
	default:
		break;
	}
/********************************************************************************************************/


	ASM_sound_handle[index].asm_callback = callback;
	ASM_sound_handle[index].cb_data = cb_data;
	ASM_sound_handle[index].is_used = true;

	debug_msg(" >>>> Event(%s), Handle(%d), CBFuncPtr(%p)", ASM_sound_events_str[sound_event], handle, callback);
	/* Add [out] param, asm_handle */
	*asm_handle = handle;

	debug_fleave();

	return true;

}

EXPORT_API
bool ASM_register_sound (const int application_pid, int *asm_handle, ASM_sound_events_t sound_event, ASM_sound_states_t sound_state,
						ASM_sound_cb_t callback, void *cb_data, ASM_resource_t mm_resource, int *error_code)
{
	return ASM_register_sound_ex (application_pid, asm_handle, sound_event, sound_state, callback, cb_data, mm_resource, error_code, NULL);
}


EXPORT_API
bool ASM_change_callback(const int asm_handle, ASM_sound_events_t sound_event, ASM_sound_cb_t callback, void *cb_data, int *error_code)
{
	int handle=0;

	if (error_code==NULL) {
		debug_error ("invalid parameter. error code is null");
		return false;
	}

	if (sound_event < ASM_EVENT_SHARE_MMPLAYER || sound_event >= ASM_EVENT_MAX) {
		*error_code = ERR_ASM_EVENT_IS_INVALID;
		debug_error ("invalid sound event(%d)",sound_event);
		return false;
	}

	int asm_index = -1;

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_POLICY_INVALID_HANDLE;
		debug_error("invalid handle(%d). callback is not registered", asm_handle);
		return false;
	}

	handle = asm_handle;

	asm_index = __ASM_find_index(handle);
	if (asm_index == -1) {
		*error_code = ERR_ASM_POLICY_INVALID_HANDLE;
		debug_error("Can not find index for handle %d", handle);
		return false;
	}

	debug_msg("callback function has changed to %p", callback);
	ASM_sound_handle[asm_index].asm_callback = callback;
	ASM_sound_handle[asm_index].cb_data = cb_data;

	return true;
}

EXPORT_API
bool ASM_unregister_sound_ex(const int asm_handle, ASM_sound_events_t sound_event, int *error_code, int (*func)(void*,void*))
{
	int handle=0;
	int asm_index = -1;
	int ret = 0;

	debug_fenter();

	if (error_code==NULL) {
		debug_error ("invalid parameter. error code is null");
		return false;
	}

	if (sound_event<ASM_EVENT_SHARE_MMPLAYER || sound_event >= ASM_EVENT_MAX) {
		*error_code = ERR_ASM_EVENT_IS_INVALID;
		debug_error ("invalid sound event(%d)",sound_event);
		return false;
	}

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_POLICY_INVALID_HANDLE;
		debug_error("invalid handle(%d). callback is not registered", asm_handle);
		return false;
	}

	handle = asm_handle;
	asm_index = __ASM_find_index(handle);
	if (asm_index == -1) {
		*error_code = ERR_ASM_POLICY_INVALID_HANDLE;
		debug_error("Can not find index for handle(%d)", handle);
		return false;
	}
	debug_msg("<<<< Event(%s), Tid(%d), Handle(%d) Index(%d)",
				ASM_sound_events_str[sound_event], ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle, asm_index);

	if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, handle, sound_event, ASM_REQUEST_UNREGISTER, ASM_STATE_NONE, ASM_RESOURCE_NONE, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}

	if (func) {
		func(&asm_snd_msg, &asm_rcv_msg);
	} else  {
		NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
		if (ret == -1) {
			*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
			debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
			return false;
		}
	}

	__ASM_destroy_callback(asm_index);

	ASM_sound_handle[asm_index].asm_fd = 0;
	ASM_sound_handle[asm_index].asm_tid = 0;
	ASM_sound_handle[asm_index].sound_event = ASM_EVENT_NONE;
	ASM_sound_handle[asm_index].is_used = false;

	debug_msg(">>>> Event(%s)", ASM_sound_events_str[sound_event]);

	debug_fleave();

	return true;
}

EXPORT_API
bool ASM_unregister_sound(const int asm_handle, ASM_sound_events_t sound_event, int *error_code)
{
	return ASM_unregister_sound_ex (asm_handle, sound_event, error_code, NULL);
}

EXPORT_API
bool ASM_get_sound_status(unsigned int *all_sound_status, int *error_code)
{
	if (all_sound_status == NULL || error_code == NULL) {
		if (error_code)
			*error_code = ERR_ASM_UNKNOWN_ERROR;
		debug_error("invalid parameter");
		return false;
	}

	debug_msg("Tid(%ld)", asmgettid());

	if (!__ASM_get_sound_state(all_sound_status, error_code)) {
		debug_error("failed to __ASM_get_sound_state(), error(%d)", *error_code);
		return false;
	}

	return true;
}

EXPORT_API
bool ASM_get_process_session_state(const int asm_handle, ASM_sound_states_t *sound_state, int *error_code)
{
	int handle = 0;
	int asm_index = 0;
	int ret = 0;

	if (sound_state == NULL || error_code == NULL) {
		if (error_code)
			*error_code = ERR_ASM_UNKNOWN_ERROR;
		debug_error("invalid parameter");
		return false;
	}

	handle = asm_handle;
	asm_index = __ASM_find_index(handle);
	if (asm_index == -1) {
		debug_error("Can not find index of %d", handle);
		return false;
	}


	debug_msg("Pid(%d)", ASM_sound_handle[asm_index].asm_tid);

	if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, handle, ASM_EVENT_MONITOR, ASM_REQUEST_GETMYSTATE, ASM_STATE_NONE, ASM_RESOURCE_NONE, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}


	NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
		debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
		return false;
	}

	NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), asm_snd_msg.instance_id, 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
		return false;
	}

	*sound_state = asm_rcv_msg.data.result_sound_state;

	debug_msg(">>>> Pid(%d), State(%s)", ASM_sound_handle[asm_index].asm_tid, ASM_sound_state_str[*sound_state]);


	return true;
}

EXPORT_API
bool ASM_get_sound_state(const int asm_handle, ASM_sound_events_t sound_event, ASM_sound_states_t *sound_state, int *error_code)
{
	int handle = 0;
	int asm_index = 0;
	int ret = 0;

	if (sound_state == NULL || error_code == NULL) {
		if (error_code)
			*error_code = ERR_ASM_UNKNOWN_ERROR;
		debug_error("invalid parameter");
		return false;
	}
	if (sound_event < ASM_EVENT_SHARE_MMPLAYER || sound_event >= ASM_EVENT_MAX) {
		*error_code = ERR_ASM_EVENT_IS_INVALID;
		debug_error("invalid sound event(%d)",sound_event);
		return false;
	}
	handle = asm_handle;

	asm_index = __ASM_find_index(handle);
	if (asm_index == -1) {
		debug_error("Can not find index of %d", handle);
		return false;
	}
	debug_msg("<<<< Event(%s), Tid(%d), handle(%d)",
			ASM_sound_events_str[sound_event], ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle);

	if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, handle, sound_event, ASM_REQUEST_GETSTATE, ASM_STATE_NONE, ASM_RESOURCE_NONE, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}


	NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
		debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
		return false;
	}

	NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), asm_snd_msg.instance_id, 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
		return false;
	}

	*sound_state = asm_rcv_msg.data.result_sound_state;

	debug_msg(">>>> Event(%s), State(%s)", ASM_sound_events_str[sound_event], ASM_sound_state_str[*sound_state]);


	return true;
}

EXPORT_API
bool ASM_set_sound_state_ex (const int asm_handle, ASM_sound_events_t sound_event, ASM_sound_states_t sound_state, ASM_resource_t mm_resource, int *error_code, int (*func)(void*,void*))
{
	int handle = 0;
	int asm_index = 0;
	int ret = 0;
	unsigned int rcv_sound_status_value;

	debug_fenter();

	if (error_code == NULL) {
		debug_error("error_code is null");
		return false;
	}

	if (sound_event < 0 || sound_event > ASM_PRIORITY_MATRIX_MIN) {
		debug_error("invalid sound event(%d)",sound_event);
		*error_code = ERR_ASM_EVENT_IS_INVALID;
		return false;
	}

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_POLICY_INVALID_HANDLE;
		debug_error("Invalid handle %d", asm_handle);
		return false;
	}

	handle = asm_handle;

	asm_index = __ASM_find_index(handle);
	if (asm_index == -1) {
		debug_error("Can not find index of %d", handle);
		return false;
	}

	debug_msg("<<<< Event(%s), State(%s), Tid(%d), handle(%d)",
				ASM_sound_events_str[sound_event], ASM_sound_state_str[sound_state], ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle);

	if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle, sound_event, ASM_REQUEST_SETSTATE, sound_state, mm_resource, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}

	if (func) {
		debug_msg( "[func(%p) START]", func);
		func (&asm_snd_msg, &asm_rcv_msg);
		debug_msg( "[func END]");
	} else {
		NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
		if (ret == -1) {
			*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
			debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
			return false;
		}
	}

	if (sound_state == ASM_STATE_PLAYING ) {
		debug_msg( "sound_state is PLAYING");
		if (func == NULL) {
			NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[handle].asm_tid, 0));
			if (ret == -1) {
				*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
				debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
				return false;
			} else {

			}
		}

		debug_msg( " <<<<<<<<<<<<<<<< [BEFORE] Callback : Main Context >>>>>>>>>>>>>>>>>>>> \n");
	/********************************************************************************************************/
		switch (asm_rcv_msg.data.result_sound_command) {
		case ASM_COMMAND_PAUSE:
		case ASM_COMMAND_STOP:
			if (handle == asm_rcv_msg.data.cmd_handle) {

				debug_msg("handle(%d) is same as asm_rcv_msg.data.cmd_handle", handle);

				asm_index = __ASM_find_index(asm_rcv_msg.data.cmd_handle);
				if (asm_index == -1) {
					*error_code = ERR_ASM_POLICY_INVALID_HANDLE;
					debug_error( "Can not find index from instance_id %ld, handle %d",	asm_rcv_msg.instance_id, asm_rcv_msg.data.cmd_handle);
					return false;
				}

				/* check former sound event */
				switch (asm_rcv_msg.data.former_sound_event) {
				case ASM_EVENT_ALARM:
					debug_msg("blocked by ALARM");
					*error_code = ERR_ASM_POLICY_CANNOT_PLAY_BY_ALARM;
					break;
				case ASM_EVENT_CALL:
				case ASM_EVENT_VIDEOCALL:
				case ASM_EVENT_RICH_CALL:
					debug_msg("blocked by CALL/VIDEOCALL/RICH_CALL");
					*error_code = ERR_ASM_POLICY_CANNOT_PLAY_BY_CALL;
					break;
				default:
					debug_msg("blocked by Other(sound_event num:%d)", asm_rcv_msg.data.former_sound_event);
					*error_code = ERR_ASM_POLICY_CANNOT_PLAY;
					break;
				}
				return false;
			} else {

				if (!__ASM_get_sound_state(&rcv_sound_status_value, error_code)) {
					debug_error("failed to __ASM_get_sound_state(), error(%d)", *error_code);
				}

				debug_msg("[ASM_CB] Callback : TID(%ld), handle(%d)", asm_rcv_msg.instance_id, asm_rcv_msg.data.cmd_handle);

				asm_index = __ASM_find_index(asm_rcv_msg.data.cmd_handle);
				if (asm_index == -1) {
					*error_code = ERR_ASM_POLICY_INVALID_HANDLE;
					debug_error("Can not find index from instance_id %ld, handle %d", asm_rcv_msg.instance_id, asm_rcv_msg.data.cmd_handle);
					return false;
				}

				if (ASM_sound_handle[asm_index].asm_callback!=NULL) {
					debug_msg( "[ASM_CB(%p) START]", ASM_sound_handle[asm_index].asm_callback);
					ASM_sound_handle[asm_index].asm_callback(asm_rcv_msg.data.cmd_handle, ASM_sound_handle[asm_index].sound_event, asm_rcv_msg.data.result_sound_command, rcv_sound_status_value, ASM_sound_handle[asm_index].cb_data);
					debug_msg( "[ASM_CB END]");
				} else {
					debug_msg("asm callback is null");
				}
			}
			break;

		case ASM_COMMAND_PLAY:
		case ASM_COMMAND_NONE:
		case ASM_COMMAND_RESUME:
		default:
			break;
		}
	/********************************************************************************************************/
		debug_msg(" <<<<<<<<<<<<<<<< [AFTER]  Callback : Main Context >>>>>>>>>>>>>>>>>>>> \n");

	}


	debug_msg(">>>> Event(%s), State(%s)", ASM_sound_events_str[sound_event],ASM_sound_state_str[sound_state]);

	debug_fleave();

	return true;
}

EXPORT_API
bool ASM_set_sound_state (const int asm_handle, ASM_sound_events_t sound_event, ASM_sound_states_t sound_state, ASM_resource_t mm_resource, int *error_code)
{
	return ASM_set_sound_state_ex (asm_handle, sound_event, sound_state, mm_resource, error_code, NULL);
}

EXPORT_API
bool ASM_set_subsession (const int asm_handle, int subsession, int *error_code, int (*func)(void*,void*))
{
	int handle = 0;
	int asm_index = 0;
	int ret = 0;
	unsigned int rcv_sound_status_value;

	debug_fenter();

	if (error_code == NULL) {
		debug_error("error_code is null");
		return false;
	}

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_POLICY_INVALID_HANDLE;
		debug_error("Invalid handle(%d)", asm_handle);
		return false;
	}

	handle = asm_handle;

	asm_index = __ASM_find_index(handle);
	if (asm_index == -1) {
		debug_error("Can not find index of %d", handle);
		return false;
	}

	if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle, subsession, ASM_REQUEST_SET_SUBSESSION, 0, 0, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}


	if (func) {
		func (&asm_snd_msg, &asm_rcv_msg);
	} else {
		NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
		if (ret == -1) {
			*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
			debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
			return false;
		}

		NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[handle].asm_tid, 0));
		if (ret == -1) {
			*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
			debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
			return false;
		}
	}

	/* TODO: Should check msg returned.....*/
#if 0
	{
		debug_msg( " <<<<<<<<<<<<<<<< [BEFORE] Callback : Main Context >>>>>>>>>>>>>>>>>>>> \n");
		/********************************************************************************************************/
		switch (asm_rcv_msg.data.result_sound_command) {
		case ASM_COMMAND_PAUSE:
		case ASM_COMMAND_STOP:
		case ASM_COMMAND_PLAY:
		case ASM_COMMAND_NONE:
		case ASM_COMMAND_RESUME:
		default:
			break;
		}
		/********************************************************************************************************/
		debug_msg(" <<<<<<<<<<<<<<<< [AFTER]  Callback : Main Context >>>>>>>>>>>>>>>>>>>> \n");

	}
#endif

	debug_fleave();

	return true;
}

EXPORT_API
bool ASM_get_subsession (const int asm_handle, int *subsession_value, int *error_code, int (*func)(void*,void*))
{
	int handle = 0;
	int asm_index = 0;
	int ret = 0;
	unsigned int rcv_sound_status_value;

	debug_fenter();

	if (error_code == NULL) {
		debug_error("error_code is null");
		return false;
	}

	/* TODO : Error Handling */
#if 0
	if (sound_event < 0 || sound_event > ASM_PRIORITY_MATRIX_MIN) {
		debug_error("invalid sound event(%d)",sound_event);
		*error_code = ERR_ASM_EVENT_IS_INVALID;
		return false;
	}
#endif

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_POLICY_INVALID_HANDLE;
		debug_error("Invalid handle %d \n", asm_handle);
		return false;
	}

	handle = asm_handle;

	asm_index = __ASM_find_index(handle);
	if (asm_index == -1) {
		debug_error("Can not find index of %d", handle);
		return false;
	}

	if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle, 0, ASM_REQUEST_GET_SUBSESSION, 0, 0, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}


	if (func) {
		func (&asm_snd_msg, &asm_rcv_msg);
	} else {
		NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
		if (ret == -1) {
			*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
			debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
			return false;
		}

		NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[handle].asm_tid, 0));
		if (ret == -1) {
			*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
			debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
			return false;
		}
	}

	*subsession_value = asm_rcv_msg.data.result_sound_command;

	debug_msg(">>>> ASM_get_subsession with subsession value [%d]\n", *subsession_value);
	debug_fleave();

	return true;
}

EXPORT_API
void ASM_dump_sound_state()
{
	int error = 0;
	int ret = 0;

	if (!__ASM_init_msg(&error) ) {
		debug_error("failed to __ASM_init_msg(), error(%d)", error);
	}
	if (!__asm_construct_snd_msg(getpid(), 0, 0, ASM_REQUEST_DUMP, ASM_STATE_NONE, ASM_RESOURCE_NONE, &error)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", error);
		return;
	}
	NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
	if (ret == -1) {
		debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
		return;
	}
}


void ASM_ask_sound_policy(ASM_sound_events_t playing_sound, ASM_sound_events_t request_sound,
				ASM_sound_cases_t *sound_policy)
{
}


#if defined(CONFIG_ENABLE_SIGNAL_HANDLER)
struct sigaction ASM_int_old_action;
struct sigaction ASM_abrt_old_action;
struct sigaction ASM_segv_old_action;
struct sigaction ASM_term_old_action;
struct sigaction ASM_sys_old_action;
struct sigaction ASM_xcpu_old_action;

void __ASM_unregister_sound(int index)
{
	int error_code = 0;
	int ret = 0;
	unsigned int sound_status_value;

	debug_fenter();

	debug_msg(" <<<< Event(%s), Index(%d)", ASM_sound_events_str[ASM_sound_handle[index].sound_event], index);

	if (!__ASM_get_sound_state(&sound_status_value, &error_code)) {
		debug_error("failed to __ASM_get_sound_state()", error_code);
	}
	if (!__ASM_set_sound_state(ASM_sound_handle[index].sound_event, ASM_STATE_NONE, &error_code)) {
		debug_error("failed to __ASM_set_sound_state(NONE)", error_code);
	}

	if (!__asm_construct_snd_msg(ASM_sound_handle[index].asm_tid, ASM_sound_handle[index].handle, ASM_sound_handle[index].sound_event, ASM_REQUEST_UNREGISTER, ASM_STATE_NONE, ASM_RESOURCE_NONE, &error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", error_code);
	}

	NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
	if (ret == -1) {
		debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
	}

	__ASM_destroy_callback(index);

	debug_msg(" >>>> Event(%s)", ASM_sound_events_str[ASM_sound_handle[index].sound_event]);

	ASM_sound_handle[index].asm_fd = 0;
	ASM_sound_handle[index].asm_tid = 0;
	ASM_sound_handle[index].sound_event = ASM_EVENT_NONE;
	ASM_sound_handle[index].is_used = false;

	debug_fleave();
}


void __ASM_signal_handler(int signo)
{
	int exit_pid = 0;
	int asm_index = 0;
	int run_emergency_exit = 0;

	for (asm_index=0 ;asm_index < ASM_HANDLE_MAX; asm_index++) {
		if (ASM_sound_handle[asm_index].is_used == true) {
			exit_pid = ASM_sound_handle[asm_index].asm_tid;
			if (exit_pid == asmgettid()) {
				run_emergency_exit = 1;
				break;
			}
		}
	}

	if (run_emergency_exit) {
		asm_snd_msg.instance_id = exit_pid;
		asm_snd_msg.data.handle = 0;
		asm_snd_msg.data.request_id = ASM_REQUEST_EMERGENT_EXIT;
		asm_snd_msg.data.sound_event = 0;
		asm_snd_msg.data.sound_state = 0;
		/* signal block -------------- */
		sigset_t old_mask, all_mask;
		sigfillset(&all_mask);
		sigprocmask(SIG_BLOCK, &all_mask, &old_mask);

		if (msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0) < 0) {
			debug_error("msgsnd() failed, tid=%ld, reqid=%d, handle=0x%x, state=0x%x event=%d size=%d",asm_snd_msg.instance_id,
					asm_snd_msg.data.request_id, asm_snd_msg.data.handle, asm_snd_msg.data.sound_state, asm_snd_msg.data.sound_event, sizeof(asm_snd_msg.data) );
			int tmpid = msgget((key_t)2014, 0666);
			if (msgsnd(tmpid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0) > 0) {
				debug_msg("msgsnd() succeed");
			} else {
				debug_error("msgsnd() retry also failed");
			}
		}

		sigprocmask(SIG_SETMASK, &old_mask, NULL);
		/* signal unblock ------------ */
	}

	switch (signo) {
	case SIGINT:
		sigaction(SIGINT, &ASM_int_old_action, NULL);
		raise( signo);
		break;
	case SIGABRT:
		sigaction(SIGABRT, &ASM_abrt_old_action, NULL);
		raise( signo);
		break;
	case SIGSEGV:
		sigaction(SIGSEGV, &ASM_segv_old_action, NULL);
		raise( signo);
		break;
	case SIGTERM:
		sigaction(SIGTERM, &ASM_term_old_action, NULL);
		raise( signo);
		break;
	case SIGSYS:
		sigaction(SIGSYS, &ASM_sys_old_action, NULL);
		raise( signo);
		break;
	case SIGXCPU:
		sigaction(SIGXCPU, &ASM_xcpu_old_action, NULL);
		raise( signo);
		break;
	default:
		break;
	}
}

#endif
void __attribute__((constructor)) __ASM_init_module(void)
{
#if defined(CONFIG_ENABLE_SIGNAL_HANDLER)
	struct sigaction ASM_action;
	ASM_action.sa_handler = __ASM_signal_handler;
	ASM_action.sa_flags = SA_NOCLDSTOP;

	debug_fenter();

	sigemptyset(&ASM_action.sa_mask);

	sigaction(SIGINT, &ASM_action, &ASM_int_old_action);
	sigaction(SIGABRT, &ASM_action, &ASM_abrt_old_action);
	sigaction(SIGSEGV, &ASM_action, &ASM_segv_old_action);
	sigaction(SIGTERM, &ASM_action, &ASM_term_old_action);
	sigaction(SIGSYS, &ASM_action, &ASM_sys_old_action);
	sigaction(SIGXCPU, &ASM_action, &ASM_xcpu_old_action);

	debug_fleave();
#endif
}


void __attribute__((destructor)) __ASM_fini_module(void)
{
	debug_fenter();

#if defined(CONFIG_ENABLE_SIGNAL_HANDLER)

	int exit_pid = 0;
	int asm_index = 0;
	int run_emergency_exit = 0;

	for (asm_index = 0; asm_index < ASM_HANDLE_MAX; asm_index++) {
		if (ASM_sound_handle[asm_index].is_used == true) {
			exit_pid = ASM_sound_handle[asm_index].asm_tid;
			if (exit_pid == asmgettid()) {
				run_emergency_exit = 1;
				break;
			}
		}
	}

	if (run_emergency_exit) {
		asm_snd_msg.instance_id = exit_pid;
		asm_snd_msg.data.handle = 0; /* dummy */
		asm_snd_msg.data.request_id = ASM_REQUEST_EMERGENT_EXIT;
		asm_snd_msg.data.sound_event = 0;
		asm_snd_msg.data.sound_state = 0;

		if (msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0) < 0 ) {
			debug_msg( "msgsnd() failed, tid=%ld, reqid=%d, handle=0x%x, state=0x%x event=%d size=%d",asm_snd_msg.instance_id,
					asm_snd_msg.data.request_id, asm_snd_msg.data.handle, asm_snd_msg.data.sound_state, asm_snd_msg.data.sound_event, sizeof(asm_snd_msg.data) );
			int tmpid = msgget((key_t)2014, 0666);
			if (msgsnd(tmpid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0) > 0) {
				debug_msg("msgsnd() succeed");
			} else {
				debug_error("msgsnd() retry also failed");
			}
		}
	}
#endif
	sigaction(SIGINT, &ASM_int_old_action, NULL);
	sigaction(SIGABRT, &ASM_abrt_old_action, NULL);
	sigaction(SIGSEGV, &ASM_segv_old_action, NULL);
	sigaction(SIGTERM, &ASM_term_old_action, NULL);
	sigaction(SIGSYS, &ASM_sys_old_action, NULL);
	sigaction(SIGXCPU, &ASM_xcpu_old_action, NULL);

	debug_fleave();
}

