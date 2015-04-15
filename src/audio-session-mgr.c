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

GThread *g_asm_thread;
GMainLoop *g_asm_loop;

typedef struct
{
	int                asm_tid;
	int                handle;
	ASM_sound_events_t sound_event;
	ASM_sound_states_t sound_state;
	ASM_sound_cb_t     asm_callback;
	ASM_watch_cb_t     watch_callback;
	void               *user_data;
	int                option_flags;
	int                asm_fd;
	GSourceFuncs*      g_src_funcs;
	GPollFD*           g_poll_fd;
	GSource*           asm_src;
	bool               is_used;
	bool               is_for_watching;
	GMutex*            asm_lock;
} ASM_sound_info_t;

ASM_sound_info_t ASM_sound_handle[ASM_HANDLE_MAX];

static const char* ASM_sound_events_str[] =
{
	"MEDIA_MMPLAYER",
	"MEDIA_MMCAMCORDER",
	"MEDIA_MMSOUND",
	"MEDIA_OPENAL",
	"MEDIA_FMRADIO",
	"MEDIA_WEBKIT",
	"NOTIFY",
	"ALARM",
	"EARJACK_UNPLUG",
	"CALL",
	"VIDEOCALL",
	"VOIP",
	"MONITOR",
	"EMERGENCY",
	"EXCLUSIVE_RESOURCE",
	"VOICE_RECOGNITION",
	"MMCAMCORDER_AUDIO",
	"MMCAMCORDER_VIDEO"
};

static const char* ASM_sound_state_str[] =
{
	"STATE_NONE",
	"STATE_PLAYING",
	"STATE_WAITING",
	"STATE_STOP",
	"STATE_PAUSE"
};

unsigned int ASM_all_sound_status;

int __ASM_find_index_by_handle(int handle);

gpointer thread_func(gpointer data)
{
	debug_log(">>> thread func..ID of this thread(%u)\n", (unsigned int)pthread_self());
	g_main_loop_run(g_asm_loop);
	debug_log("<<< quit thread func..\n");
	return NULL;
}

bool __ASM_get_sound_state(unsigned int *all_sound_status, int *error_code)
{
	int value = 0;

	if(vconf_get_int(SOUND_STATUS_KEY, &value)) {
		debug_error("failed to vconf_get_int(SOUND_STATUS_KEY)");
		*error_code = ERR_ASM_VCONF_ERROR;
		return false;
	}
	debug_log("All status(%#X)", value);
	*all_sound_status = value;
	ASM_all_sound_status = value;

	return true;
}

gboolean __asm_fd_check(GSource * source)
{
	GSList *fd_list;
	GPollFD *temp;

	if (!source) {
		debug_error("GSource is null");
		return FALSE;
	}
	fd_list = source->poll_fds;
	if (!fd_list) {
		debug_error("fd_list is null");
		return FALSE;
	}
	do {
		temp = (GPollFD*)fd_list->data;
		if (!temp) {
			debug_error("fd_list->data is null");
			return FALSE;
		}
		if (temp->revents & (POLLIN | POLLPRI)) {
			return TRUE;
		}
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
	//debug_fenter();
	debug_log(">>> asm_callback_handler()..ID of this thread(%u)\n", (unsigned int)pthread_self());

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
		rcv_command = (ASM_sound_commands_t)((buf >> 16) & 0xff);
		event_src = (ASM_event_sources_t)((buf >> 24) & 0xff);

		asm_index = __ASM_find_index_by_handle(handle);
		if (asm_index == -1) {
			debug_error("Can not find index");
			return FALSE;
		}

		if (ASM_sound_handle[asm_index].asm_lock) {
			g_mutex_lock(ASM_sound_handle[asm_index].asm_lock);
		}

		tid = ASM_sound_handle[asm_index].asm_tid;

		if (rcv_command) {
			debug_msg("Got and start CB : TID(%d), handle(%d), command(%d,(PLAY(2)/STOP(3)/PAUSE(4)/RESUME(5)), event_src(%d)", tid, handle, rcv_command, event_src );
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
					break;
				}
				debug_msg("[CALLBACK(%p) START]",ASM_sound_handle[asm_index].asm_callback);
				cb_res = (ASM_sound_handle[asm_index].asm_callback)(handle, event_src, rcv_command, sound_status_value, ASM_sound_handle[asm_index].user_data);
				debug_msg("[CALLBACK END]");
				break;
			default:
				break;
			}
#ifdef CONFIG_ENABLE_RETCB

			/* If the command is not RESUME, send return */
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
					if (ASM_sound_handle[asm_index].asm_lock) {
						g_mutex_unlock(ASM_sound_handle[asm_index].asm_lock);
					}
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
	//debug_fleave();

	if (ASM_sound_handle[asm_index].asm_lock) {
		g_mutex_unlock(ASM_sound_handle[asm_index].asm_lock);
	}

	return TRUE;
}

gboolean watch_callback_handler( gpointer d)
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
		ASM_sound_events_t rcv_sound_event = ASM_EVENT_NONE;
		ASM_sound_states_t rcv_sound_state = ASM_STATE_NONE;
		int error_code = 0;

		unsigned int sound_status_value;

		ASM_cb_result_t cb_res = ASM_CB_RES_NONE;


		count = read(data->fd, &buf, sizeof(int));

		handle = (int)( buf & 0x0000ffff);
		rcv_sound_event = (ASM_sound_events_t)((buf >> 16) & 0xff);
		rcv_sound_state = (ASM_sound_states_t)((buf >> 24) & 0xff);

		asm_index = __ASM_find_index_by_handle(handle);
		if (asm_index == -1) {
			debug_error("Can not find index");
			return FALSE;
		}

		if (ASM_sound_handle[asm_index].asm_lock) {
			g_mutex_lock(ASM_sound_handle[asm_index].asm_lock);
		}

		tid = ASM_sound_handle[asm_index].asm_tid;

		debug_msg("Got and start CB : handle(%d) sound_event(%d) sound_state(%d)", handle, rcv_sound_event, rcv_sound_state );

		if (!__ASM_get_sound_state(&sound_status_value, &error_code)) {
			debug_error("failed to __ASM_get_sound_state(), error(%d)", error_code);
		}

		if (ASM_sound_handle[asm_index].watch_callback == NULL) {
			debug_msg("callback is null..");
			if (ASM_sound_handle[asm_index].asm_lock) {
				g_mutex_unlock(ASM_sound_handle[asm_index].asm_lock);
			}
			return FALSE;
		}
		debug_msg("[CALLBACK(%p) START]",ASM_sound_handle[asm_index].watch_callback);
		cb_res = (ASM_sound_handle[asm_index].watch_callback)(handle, rcv_sound_event, rcv_sound_state, ASM_sound_handle[asm_index].user_data);
		debug_msg("[CALLBACK END]");

#ifdef CONFIG_ENABLE_RETCB
		{
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
				if (ASM_sound_handle[asm_index].asm_lock) {
					g_mutex_unlock(ASM_sound_handle[asm_index].asm_lock);
				}
				return FALSE;
			}
			rett = write(tmpfd, &buf, sizeof(buf));
			close(tmpfd);
			g_free(filename2);
			debug_msg("[RETCB] tid(%d) finishing CB (write=%d)\n", tid, rett);
		}
#endif

	}
	debug_fleave();

	if (ASM_sound_handle[asm_index].asm_lock) {
		g_mutex_unlock(ASM_sound_handle[asm_index].asm_lock);
	}

	return TRUE;
}


bool __ASM_add_sound_callback(int index, int fd, gushort events, gLoopPollHandler_t p_gloop_poll_handler )
{
	GSource* g_src = NULL;
	GSourceFuncs *g_src_funcs = NULL;		/* handler function */
	guint gsource_handle;
	GPollFD *g_poll_fd = NULL;			/* file descriptor */

	ASM_sound_handle[index].asm_lock = g_mutex_new();
	if (!ASM_sound_handle[index].asm_lock) {
		debug_error("failed to g_mutex_new() for index(%d)", index);
		return false;
	}

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

	/* 2. add file description which used in g_loop() */
	g_poll_fd = (GPollFD *)g_malloc(sizeof(GPollFD));
	if (!g_poll_fd) {
		debug_error("g_malloc failed on g_poll_fd");
		return false;
	}
	g_poll_fd->fd = fd;
	g_poll_fd->events = events;
	ASM_sound_handle[index].g_poll_fd = g_poll_fd;

	/* 3. combine g_source object and file descriptor */
	g_source_add_poll(g_src, g_poll_fd);
	gsource_handle = g_source_attach(g_src, g_main_loop_get_context(g_asm_loop));
	if (!gsource_handle) {
		debug_error(" Failed to attach the source to context");
		return false;
	}
	g_source_unref(g_src);

	/* 4. set callback */
	g_source_set_callback(g_src, p_gloop_poll_handler,(gpointer)g_poll_fd, NULL);

	debug_log(" g_malloc:g_src_funcs(%#X),g_poll_fd(%#X)  g_source_add_poll:g_src_id(%d)  g_source_set_callback:errno(%d)",
				g_src_funcs, g_poll_fd, gsource_handle, errno);
	return true;
}


bool __ASM_remove_sound_callback(int index, gushort events)
{
	bool ret = true;

	if (ASM_sound_handle[index].asm_lock) {
		g_mutex_free(ASM_sound_handle[index].asm_lock);
		ASM_sound_handle[index].asm_lock = NULL;
	}

	GSourceFuncs *g_src_funcs = ASM_sound_handle[index].g_src_funcs;
	GPollFD *g_poll_fd = ASM_sound_handle[index].g_poll_fd;	/* store file descriptor */
	if (!g_poll_fd) {
		debug_error("g_poll_fd is null..");
		ret = false;
		goto init_handle;
	}
	g_poll_fd->fd = ASM_sound_handle[index].asm_fd;
	g_poll_fd->events = events;

	if (!ASM_sound_handle[index].asm_src) {
		debug_error("ASM_sound_handle[%d].asm_src is null..", index);
		goto init_handle;
	}
	debug_log(" g_source_remove_poll : fd(%d), event(%x), errno(%d)", g_poll_fd->fd, g_poll_fd->events, errno);
	g_source_remove_poll(ASM_sound_handle[index].asm_src, g_poll_fd);

init_handle:

	if (ASM_sound_handle[index].asm_src) {
		g_source_destroy(ASM_sound_handle[index].asm_src);
		if (!g_source_is_destroyed (ASM_sound_handle[index].asm_src)) {
			debug_warning(" failed to g_source_destroy(), asm_src(0x%p)", ASM_sound_handle[index].asm_src);
		}
	}
	debug_log(" g_free : g_src_funcs(%#X), g_poll_fd(%#X)", g_src_funcs, g_poll_fd);

	if (g_src_funcs) {
		g_free(g_src_funcs);
		g_src_funcs = NULL;
	}
	if (g_poll_fd) {
		g_free(g_poll_fd);
		g_poll_fd = NULL;
	}

	ASM_sound_handle[index].g_src_funcs = NULL;
	ASM_sound_handle[index].g_poll_fd = NULL;
	ASM_sound_handle[index].asm_src = NULL;
	ASM_sound_handle[index].asm_callback = NULL;
	ASM_sound_handle[index].watch_callback = NULL;

	return ret;
}


bool __ASM_is_existed_request_for_watching(ASM_sound_events_t interest_event, ASM_sound_states_t interest_state, int *index)
{
	int i = 0;
	for(i = 0; i< ASM_HANDLE_MAX; i++) {
		if (ASM_sound_handle[i].is_for_watching && ASM_sound_handle[i].sound_event == interest_event) {
			if (ASM_sound_handle[i].sound_state == interest_state) {
				debug_warning("already requested interest-session(%s, %s)",
						ASM_sound_events_str[interest_event], ASM_sound_state_str[interest_state]);
				*index = i;
				return true;
			}
		}
	}
	*index = 0;
	return false;
}


bool __ASM_is_supported_session_for_watching(ASM_sound_events_t interest_event, ASM_sound_states_t interest_state)
{
	bool ret = false;

	/* check sound_event */
	switch (interest_event) {
	case ASM_EVENT_MEDIA_MMPLAYER:
	case ASM_EVENT_MEDIA_MMCAMCORDER:
	case ASM_EVENT_MEDIA_MMSOUND:
	case ASM_EVENT_MEDIA_OPENAL:
	case ASM_EVENT_MEDIA_FMRADIO:
	case ASM_EVENT_MEDIA_WEBKIT:
	case ASM_EVENT_NOTIFY:
	case ASM_EVENT_ALARM:
	case ASM_EVENT_EARJACK_UNPLUG:
	case ASM_EVENT_CALL:
	case ASM_EVENT_VIDEOCALL:
	case ASM_EVENT_VOIP:
	case ASM_EVENT_MONITOR:
	case ASM_EVENT_EMERGENCY:
	case ASM_EVENT_EXCLUSIVE_RESOURCE:
	case ASM_EVENT_VOICE_RECOGNITION:
	case ASM_EVENT_MMCAMCORDER_AUDIO:
	case ASM_EVENT_MMCAMCORDER_VIDEO:
		ret = true;
		break;
	default:
		debug_error("not supported sound_event(%d)", interest_event);
		ret = false;
		return ret;
	}

	/* check sound_state */
	switch (interest_state) {
	case ASM_STATE_PLAYING:
	case ASM_STATE_STOP:
		ret = true;
		break;
	default:
		debug_error("not supported sound_state(%d)", interest_state);
		ret = false;
		return ret;
	}

	return ret;
}


int __ASM_find_index_by_handle(int handle)
{
	int i = 0;
	for(i = 0; i< ASM_HANDLE_MAX; i++) {
		if (handle == ASM_sound_handle[i].handle) {
			//debug_msg("found index(%d) for handle(%d)", i, handle);
			if (handle == ASM_HANDLE_INIT_VAL) {
				return -1;
			}
			return i;
		}
	}
	return -1;
}

int __ASM_find_index_by_event(ASM_sound_events_t sound_event, int pid)
{
	int i = 0;

	for(i = 0; i< ASM_HANDLE_MAX; i++) {
		if (sound_event == ASM_sound_handle[i].sound_event && pid == ASM_sound_handle[i].asm_tid) {
			debug_msg("found index(%d) for sound_event(%d)", i, sound_event);
			return i;
		}
	}
	return -1;
}


void __ASM_add_callback(int index, bool is_for_watching)
{
	if (!is_for_watching) {
		if (!__ASM_add_sound_callback(index, ASM_sound_handle[index].asm_fd, (gushort)POLLIN | POLLPRI, asm_callback_handler)) {
			debug_error("failed to __ASM_add_sound_callback(asm_callback_handler)");
			//return false;
		}
	} else {
		if (!__ASM_add_sound_callback(index, ASM_sound_handle[index].asm_fd, (gushort)POLLIN | POLLPRI, watch_callback_handler)) {
			debug_error("failed to __ASM_add_sound_callback(watch_callback_handler)");
			//return false;
		}
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

	char *filename = g_strdup_printf("/tmp/ASM.%d.%d", ASM_sound_handle[index].asm_tid, ASM_sound_handle[index].handle);
	pre_mask = umask(0);
	if (mknod(filename, S_IFIFO|0666, 0)) {
		debug_error("mknod() failure, errno(%d)", errno);
	}
	umask(pre_mask);
	ASM_sound_handle[index].asm_fd = open( filename, O_RDWR|O_NONBLOCK);
	if (ASM_sound_handle[index].asm_fd == -1) {
		debug_error("%s : index(%d), file open error(%d)", str_fail, index, errno);
	} else {
		debug_log("%s : index(%d), filename(%s), fd(%d)", str_pass, index, filename, ASM_sound_handle[index].asm_fd);
	}
	g_free(filename);
	filename = NULL;

#ifdef CONFIG_ENABLE_RETCB
	char *filename2 = g_strdup_printf("/tmp/ASM.%d.%dr", ASM_sound_handle[index].asm_tid,  ASM_sound_handle[index].handle);
	pre_mask = umask(0);
	if (mknod(filename2, S_IFIFO | 0666, 0)) {
		debug_error("mknod() failure, errno(%d)", errno);
	}
	umask(pre_mask);
	g_free(filename2);
	filename2 = NULL;
#endif

}


void __ASM_close_callback(int index)
{
	if (ASM_sound_handle[index].asm_fd < 0) {
		debug_error("%s : fd error.", str_fail);
	} else {
		char *filename = g_strdup_printf("/tmp/ASM.%d.%d", ASM_sound_handle[index].asm_tid, ASM_sound_handle[index].handle);
		close(ASM_sound_handle[index].asm_fd);
		if (remove(filename)) {
			debug_error("remove() failure, filename(%s), errno(%d)", filename, errno);
		}
		debug_log("%s : index(%d), filename(%s), fd(%d)", str_pass, index, filename, ASM_sound_handle[index].asm_fd);
		g_free(filename);
		filename = NULL;
	}

#ifdef CONFIG_ENABLE_RETCB
	char *filename2 = g_strdup_printf("/tmp/ASM.%d.%dr", ASM_sound_handle[index].asm_tid, ASM_sound_handle[index].handle);

	/* Defensive code - wait until callback timeout although callback is removed */
	int buf = ASM_CB_RES_STOP;
	int tmpfd = -1;
	int ret;
	tmpfd = open(filename2, O_WRONLY | O_NONBLOCK);
	if (tmpfd < 0) {
		char str_error[256];
		strerror_r(errno, str_error, sizeof(str_error));
		debug_warning("could not open file(%s) (may server close it first), tid(%d) fd(%d) %s errno=%d(%s)",
			filename2, ASM_sound_handle[index].asm_tid, tmpfd, filename2, errno, str_error);
	} else {
		ret = write(tmpfd, &buf, sizeof(buf));
		close(tmpfd);
		debug_msg("write ASM_CB_RES_STOP(tid:%d) for waiting server , error code(%d)", ASM_sound_handle[index].asm_tid, ret);
	}

	if (remove(filename2)) {
		debug_error("remove() failure, filename(%s), errno(%d)", filename2, errno);
	}
	g_free(filename2);
	filename2 = NULL;
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

	debug_msg("tid=%ld,handle=%d,req=%d,evt=%d,state=%d,resource=%d,instance_id=%ld", asm_snd_msg.instance_id, asm_snd_msg.data.handle,
				asm_snd_msg.data.request_id, asm_snd_msg.data.sound_event, asm_snd_msg.data.sound_state, asm_snd_msg.data.system_resource, asm_snd_msg.instance_id);

	return true;
}


bool __ASM_init_msg(int *error_code)
{
	int i = 0;
	asm_snd_msgid = msgget((key_t)2014, 0666);
	asm_rcv_msgid = msgget((key_t)4102, 0666);
	asm_cb_msgid = msgget((key_t)4103, 0666);

	debug_msg("snd_msqid(%#X), rcv_msqid(%#X), cb_msqid(%#X)\n", asm_snd_msgid, asm_rcv_msgid, asm_cb_msgid);

	if (asm_snd_msgid == -1 || asm_rcv_msgid == -1 || asm_cb_msgid == -1 ) {
		if (errno == EACCES) {
			debug_warning("Require ROOT permission.\n");
		} else if (errno == ENOMEM) {
			debug_warning("System memory is empty.\n");
		} else if(errno == ENOSPC) {
			debug_warning("Resource is empty.\n");
		}
		/* try again in 50ms later by 10 times */
		for (i=0;i<10;i++) {
			usleep(50000);
			asm_snd_msgid = msgget((key_t)2014, 0666);
			asm_rcv_msgid = msgget((key_t)4102, 0666);
			asm_cb_msgid = msgget((key_t)4103, 0666);
			if (asm_snd_msgid == -1 || asm_rcv_msgid == -1 || asm_cb_msgid == -1) {
				debug_error("Fail to GET msgid by retrying %d times\n", i+1);
			} else {
				break;
			}
		}

		if (asm_snd_msgid == -1 || asm_rcv_msgid == -1 || asm_cb_msgid == -1) {
			*error_code = ERR_ASM_MSG_QUEUE_MSGID_GET_FAILED;
			debug_error("failed to msgget with error(%d)",*error_code);
			return false;
		}
	}

	return true;
}

void __ASM_init_callback(int index, bool is_for_watching)
{
	debug_fenter();
	__ASM_open_callback(index);
	__ASM_add_callback(index, is_for_watching);
	debug_fleave();
}


void __ASM_destroy_callback(int index)
{
	debug_fenter();
	__ASM_remove_callback(index);
	__ASM_close_callback(index);
	debug_fleave();
}

EXPORT_API
bool ASM_register_sound_ex (const int application_pid, int *asm_handle, ASM_sound_events_t sound_event, ASM_sound_states_t sound_state,
						ASM_sound_cb_t callback, void *user_data, ASM_resource_t mm_resource, int *error_code, int (*func)(void*,void*))
{
	unsigned int sound_status_value;
	int handle = 0;
	int asm_pid = 0;
	int index = 0;
	int ret = 0;

	debug_fenter();

	if (error_code==NULL) {
		debug_error ("invalid parameter. error code is null");
		return false;
	}
	*error_code = ERR_ASM_ERROR_NONE;

	if (sound_event < ASM_EVENT_MEDIA_MMPLAYER || sound_event >= ASM_EVENT_MAX) {
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
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_FULL;
		debug_error ("local sound event is full(MAX)");
		return false;
	}

	if (!g_asm_thread) {
		GMainContext* asm_context = g_main_context_new ();
		g_asm_loop = g_main_loop_new (asm_context, FALSE);
		g_main_context_unref(asm_context);
		g_asm_thread = g_thread_create(thread_func, NULL, TRUE, NULL);
		if (g_asm_thread == NULL) {
			debug_error ("could not create thread..");
			g_main_loop_unref(g_asm_loop);
			return false;
		}
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

	if (!__ASM_init_msg(error_code)) {
		debug_error("failed to __ASM_init_msg(), error(%d)", *error_code);
		return false;
	}

	if (!__ASM_get_sound_state(&sound_status_value, error_code)) {
		debug_error("failed to __ASM_get_sound_state(), error(%d)", *error_code);
		return false;
	}

	debug_msg(" <<<< Event(%s), Tid(%d), Index(%d), State(%s)", ASM_sound_events_str[sound_event], ASM_sound_handle[index].asm_tid, index, ASM_sound_state_str[sound_state]);

	handle = -1; /* for register & get handle from server */

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

		NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[index].asm_tid, 0));
		if (ret == -1) {
			*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
			debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
			return false;
		}

		if (asm_rcv_msg.data.source_request_id != ASM_REQUEST_REGISTER) {
			*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
			debug_error("received msg is not valid, source_request_id(%d)", asm_rcv_msg.data.source_request_id);
			return false;
		}
	}
	/* Construct msg to send -> send msg -> recv msg : end */

	handle = asm_rcv_msg.data.alloc_handle; /* get handle from server */
	if (handle == -1) {
		debug_error("failed to create handle from server");
		*error_code = ERR_ASM_SERVER_HANDLE_IS_FULL;
		return false;
	}

	ASM_sound_handle[index].handle = handle;

	__ASM_init_callback(index, ASM_sound_handle[index].is_for_watching);

/********************************************************************************************************/
	switch (asm_rcv_msg.data.result_sound_command) {
	case ASM_COMMAND_PAUSE:
	case ASM_COMMAND_STOP:
		debug_msg( " <<<<<<<<<<<<<<<< Received command : %d (STOP(3)/PAUSE(4)) >>>>>>>>>>>>>>>>>>>>\n", asm_rcv_msg.data.result_sound_command);
		if (handle == asm_rcv_msg.data.cmd_handle) {

			__ASM_destroy_callback(index);

			ASM_sound_handle[index].asm_fd = 0;
			ASM_sound_handle[index].asm_tid = 0;
			ASM_sound_handle[index].sound_event = ASM_EVENT_NONE;
			ASM_sound_handle[index].is_used = false;
			if (asm_rcv_msg.data.result_sound_command == ASM_COMMAND_PAUSE) {
				ASM_sound_handle[index].sound_state = ASM_STATE_PAUSE;
			} else if (asm_rcv_msg.data.result_sound_command == ASM_COMMAND_STOP) {
				ASM_sound_handle[index].sound_state = ASM_STATE_STOP;
			}

			if (asm_rcv_msg.data.error_code){
				*error_code = asm_rcv_msg.data.error_code;
				debug_warning("error code: %x",*error_code);
			}
			return false;

		} else {
			int action_index = 0;
			unsigned int rcv_sound_status_value = 0;

			if (!__ASM_get_sound_state(&rcv_sound_status_value, error_code)) {
				debug_error("failed to __ASM_get_sound_state(), error(%d)", *error_code);
			}

			debug_msg("Callback : TID(%ld), handle(%d), command(%d)", asm_rcv_msg.instance_id, asm_rcv_msg.data.cmd_handle, asm_rcv_msg.data.result_sound_command);
			action_index = __ASM_find_index_by_handle(asm_rcv_msg.data.cmd_handle);
			if (action_index == -1) {
				debug_error("Can not find index of instance %ld, handle %d", asm_rcv_msg.instance_id, asm_rcv_msg.data.cmd_handle);
			} else {
				if (ASM_sound_handle[action_index].asm_callback!=NULL) {
					ASM_sound_handle[action_index].asm_callback(asm_rcv_msg.data.cmd_handle, ASM_sound_handle[action_index].sound_event, asm_rcv_msg.data.result_sound_command, rcv_sound_status_value, ASM_sound_handle[action_index].user_data);
				} else {
					debug_msg("null callback");
				}
			}
		}
		break;

	case ASM_COMMAND_PLAY:
	case ASM_COMMAND_NONE:
	case ASM_COMMAND_RESUME:
		ASM_sound_handle[index].sound_state = sound_state;
		break;
	default:
		break;
	}
/********************************************************************************************************/


	ASM_sound_handle[index].asm_callback = callback;
	ASM_sound_handle[index].user_data = user_data;
	ASM_sound_handle[index].is_used = true;

	debug_msg(" >>>> Event(%s), Handle(%d), CBFuncPtr(%p)", ASM_sound_events_str[sound_event], handle, callback);
	/* Add [out] param, asm_handle */
	*asm_handle = handle;

	debug_fleave();

	return true;

}

EXPORT_API
bool ASM_register_sound (const int application_pid, int *asm_handle, ASM_sound_events_t sound_event, ASM_sound_states_t sound_state,
						ASM_sound_cb_t callback, void *user_data, ASM_resource_t mm_resource, int *error_code)
{
	return ASM_register_sound_ex (application_pid, asm_handle, sound_event, sound_state, callback, user_data, mm_resource, error_code, NULL);
}


EXPORT_API
bool ASM_change_callback(const int asm_handle, ASM_sound_events_t sound_event, ASM_sound_cb_t callback, void *user_data, int *error_code)
{
	int handle=0;

	if (error_code==NULL) {
		debug_error ("invalid parameter. error code is null");
		return false;
	}
	*error_code = ERR_ASM_ERROR_NONE;

	if (sound_event < ASM_EVENT_MEDIA_MMPLAYER || sound_event >= ASM_EVENT_MAX) {
		*error_code = ERR_ASM_EVENT_IS_INVALID;
		debug_error ("invalid sound event(%d)",sound_event);
		return false;
	}

	int asm_index = -1;

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
		debug_error("invalid handle(%d). callback is not registered", asm_handle);
		return false;
	}

	handle = asm_handle;

	asm_index = __ASM_find_index_by_handle(handle);
	if (asm_index == -1) {
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
		debug_error("Can not find index for handle %d", handle);
		return false;
	}

	debug_msg("callback function has changed to %p", callback);
	ASM_sound_handle[asm_index].asm_callback = callback;
	ASM_sound_handle[asm_index].user_data = user_data;

	return true;
}

EXPORT_API
bool ASM_unregister_sound_ex(const int asm_handle, ASM_sound_events_t sound_event, int *error_code, int (*func)(void*,void*))
{
	int handle=0;
	int asm_index = -1;
	int ret = 0;

	debug_fenter();

	if (error_code == NULL) {
		debug_error ("invalid parameter. error code is null");
		return false;
	}
	*error_code = ERR_ASM_ERROR_NONE;

	if (sound_event < ASM_EVENT_MEDIA_MMPLAYER || sound_event >= ASM_EVENT_MAX) {
		*error_code = ERR_ASM_EVENT_IS_INVALID;
		debug_error ("invalid sound event(%d)",sound_event);
		return false;
	}

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
		debug_error("invalid handle(%d). callback is not registered", asm_handle);
		return false;
	}

	handle = asm_handle;
	asm_index = __ASM_find_index_by_handle(handle);
	if (asm_index == -1) {
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
		debug_error("Can not find index for handle(%d)", handle);
		return false;
	}
	debug_msg("<<<< Event(%s), Tid(%d), Handle(%d) Index(%d)",
				ASM_sound_events_str[sound_event], ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle, asm_index);

	if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, handle, sound_event, ASM_REQUEST_UNREGISTER, ASM_STATE_NONE, ASM_RESOURCE_NONE, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}

	if (ASM_sound_handle[asm_index].asm_lock) {
		if (!g_mutex_trylock(ASM_sound_handle[asm_index].asm_lock)) {
			debug_warning("maybe asm_callback is being called, try one more time..");
			usleep(2500000); // 2.5 sec
			if (g_mutex_trylock(ASM_sound_handle[asm_index].asm_lock)) {
				debug_msg("finally got asm_lock");
			}
		}
	}

	if (func) {
		func(&asm_snd_msg, &asm_rcv_msg);
	} else  {
		NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
		if (ret == -1) {
			*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
			debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
			if (ASM_sound_handle[asm_index].asm_lock) {
				g_mutex_unlock(ASM_sound_handle[asm_index].asm_lock);
			}
			return false;
		}
	}

	if (ASM_sound_handle[asm_index].asm_lock) {
		g_mutex_unlock(ASM_sound_handle[asm_index].asm_lock);
	}

	__ASM_destroy_callback(asm_index);

	ASM_sound_handle[asm_index].asm_fd = 0;
	ASM_sound_handle[asm_index].asm_tid = 0;
	ASM_sound_handle[asm_index].handle = 0;
	ASM_sound_handle[asm_index].sound_event = ASM_EVENT_NONE;
	ASM_sound_handle[asm_index].sound_state = ASM_STATE_NONE;
	ASM_sound_handle[asm_index].is_used = false;

	debug_fleave();

	return true;
}

EXPORT_API
bool ASM_unregister_sound(const int asm_handle, ASM_sound_events_t sound_event, int *error_code)
{
	return ASM_unregister_sound_ex (asm_handle, sound_event, error_code, NULL);
}

EXPORT_API
bool ASM_set_watch_session (const int application_pid,	ASM_sound_events_t interest_sound_event,
                            ASM_sound_states_t interest_sound_state, ASM_watch_cb_t callback, void *user_data, int *error_code)
{
	unsigned int sound_status_value;
	int handle = 0;
	int asm_pid = 0;
	int index = 0;
	int ret = 0;

	debug_fenter();

	if (error_code==NULL) {
		debug_error ("invalid parameter. error code is null");
		return false;
	}
	*error_code = ERR_ASM_ERROR_NONE;

	if (interest_sound_event < ASM_EVENT_MEDIA_MMPLAYER || interest_sound_event >= ASM_EVENT_MAX) {
		*error_code = ERR_ASM_EVENT_IS_INVALID;
		debug_error ("invalid sound event(%d)", interest_sound_event);
		return false;
	}

	if (!__ASM_is_supported_session_for_watching(interest_sound_event, interest_sound_state)) {
		debug_error("not supported sound_event(%d) or sound_state(%d)", interest_sound_event, interest_sound_state);
		*error_code = ERR_ASM_WATCH_NOT_SUPPORTED;
		return false;
	}

	if (__ASM_is_existed_request_for_watching(interest_sound_event, interest_sound_state, &index))
	{
		debug_warning("already requested interest-session, do not send request message");
		*error_code = ERR_ASM_WATCH_ALREADY_REQUESTED;
		return false;
	}

	for (index = 0; index < ASM_HANDLE_MAX; index++) {
		if (ASM_sound_handle[index].is_used == false) {
			break;
		}
	}

	if (index == ASM_HANDLE_MAX) {
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_FULL;
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

	ASM_sound_handle[index].asm_tid = asm_pid;
	ASM_sound_handle[index].sound_event = interest_sound_event;
	ASM_sound_handle[index].sound_state = interest_sound_state;

	debug_msg(" <<<< Interest event(%s), state(%s), Tid(%d), Index(%d)",
				ASM_sound_events_str[interest_sound_event], ASM_sound_state_str[interest_sound_state], ASM_sound_handle[index].asm_tid, index);

	if (!__ASM_init_msg(error_code)) {
		debug_error("failed to __ASM_init_msg(), error(%d)", *error_code);
		return false;
	}

	if (!__ASM_get_sound_state(&sound_status_value, error_code)) {
		debug_error("failed to __ASM_get_sound_state(), error(%d)", *error_code);
		return false;
	}

	handle = -1; /* for register & get handle from server */

	/* Construct msg to send -> send msg -> recv msg */
	if (!__asm_construct_snd_msg(asm_pid, handle, interest_sound_event, ASM_REQUEST_REGISTER_WATCHER, interest_sound_state, ASM_RESOURCE_NONE, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}

	NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
		debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
		return false;
	}

	NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[index].asm_tid, 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
		return false;
	}
	if (asm_rcv_msg.data.source_request_id != ASM_REQUEST_REGISTER_WATCHER) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("received msg is not valid, source_request_id(%d)", asm_rcv_msg.data.source_request_id);
		return false;
	}
	/* Construct msg to send -> send msg -> recv msg : end */

	handle = asm_rcv_msg.data.alloc_handle; /* get handle from server */
	if (handle == -1) {
		debug_error("failed to create handle from server");
		*error_code = ERR_ASM_SERVER_HANDLE_IS_FULL;
		return false;
	}

	ASM_sound_handle[index].handle = handle;
	ASM_sound_handle[index].watch_callback = callback;
	ASM_sound_handle[index].user_data = user_data;
	ASM_sound_handle[index].is_used = true;
	ASM_sound_handle[index].is_for_watching = true;

	__ASM_init_callback(index, ASM_sound_handle[index].is_for_watching);

	/********************************************************************************************************/
	switch (asm_rcv_msg.data.result_sound_command) {
	case ASM_COMMAND_PLAY:
		debug_msg(" >>>> added to watch list successfully");
		break;

	default:
		debug_error("received message is abnormal..result_sound_command(%d) from ASM server", asm_rcv_msg.data.result_sound_command);
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		return false;
	}
	/********************************************************************************************************/

	debug_fleave();

	return true;
}

EXPORT_API
bool ASM_unset_watch_session (ASM_sound_events_t interest_sound_event, ASM_sound_states_t interest_sound_state, int *error_code)
{
	unsigned int sound_status_value;
	int handle = 0;
	int asm_pid = 0;
	int index = 0;
	int ret = 0;

	debug_fenter();

	if (error_code==NULL) {
		debug_error ("invalid parameter. error code is null");
		return false;
	}
	*error_code = ERR_ASM_ERROR_NONE;

	if (interest_sound_event < ASM_EVENT_MEDIA_MMPLAYER || interest_sound_event >= ASM_EVENT_MAX) {
		*error_code = ERR_ASM_EVENT_IS_INVALID;
		debug_error ("invalid sound event(%d)",interest_sound_event);
		return false;
	}

	if (!__ASM_is_supported_session_for_watching(interest_sound_event, interest_sound_state)) {
		debug_error("not supported sound_event(%d) or sound_state(%d)", interest_sound_event, interest_sound_state);
		*error_code = ERR_ASM_WATCH_NOT_SUPPORTED;
		return false;
	}

	if (!__ASM_is_existed_request_for_watching(interest_sound_event, interest_sound_state, &index))
	{
		debug_warning("already unrequested interest-session or have not been requested it before, do not send request message");
		*error_code = ERR_ASM_WATCH_ALREADY_UNREQUESTED;
		return false;
	}

	debug_msg(" <<<< Unregister interest event(%s), state(%s), Tid(%d), Index(%d)",
				ASM_sound_events_str[ASM_sound_handle[index].sound_event], ASM_sound_state_str[ASM_sound_handle[index].sound_state], ASM_sound_handle[index].asm_tid, index);

	if (!__ASM_init_msg(error_code)) {
		debug_error("failed to __ASM_init_msg(), error(%d)", *error_code);
		return false;
	}

	if (!__ASM_get_sound_state(&sound_status_value, error_code)) {
		debug_error("failed to __ASM_get_sound_state(), error(%d)", *error_code);
		return false;
	}

	handle = ASM_sound_handle[index].handle;
	asm_pid = ASM_sound_handle[index].asm_tid;

	/* Construct msg to send -> send msg */
	if (!__asm_construct_snd_msg(asm_pid, handle, interest_sound_event, ASM_REQUEST_UNREGISTER_WATCHER, interest_sound_state, ASM_RESOURCE_NONE, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}

	if (ASM_sound_handle[index].asm_lock) {
		g_mutex_lock(ASM_sound_handle[index].asm_lock);
	}

	NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
		debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
		if (ASM_sound_handle[index].asm_lock) {
			g_mutex_unlock(ASM_sound_handle[index].asm_lock);
		}
		return false;
	}
	/* Construct msg to send -> send msg : end */

	if (ASM_sound_handle[index].asm_lock) {
		g_mutex_unlock(ASM_sound_handle[index].asm_lock);
	}

	__ASM_destroy_callback(index);

	ASM_sound_handle[index].asm_tid = 0;
	ASM_sound_handle[index].handle = 0;
	ASM_sound_handle[index].sound_event = ASM_EVENT_NONE;
	ASM_sound_handle[index].sound_state = ASM_STATE_NONE;
	ASM_sound_handle[index].is_used = false;
	ASM_sound_handle[index].is_for_watching = false;

	debug_msg(" >>>> send requesting message successfully");

	debug_fleave();

	return true;
}

EXPORT_API
bool ASM_get_sound_status(unsigned int *all_sound_status, int *error_code)
{
	if (all_sound_status == NULL || error_code == NULL) {
		if (error_code)
			*error_code = ERR_ASM_INVALID_PARAMETER;
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
			*error_code = ERR_ASM_INVALID_PARAMETER;
		debug_error("invalid parameter");
		return false;
	}

	handle = asm_handle;
	asm_index = __ASM_find_index_by_handle(handle);
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

	NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[asm_index].asm_tid, 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
		return false;
	}

	if (asm_rcv_msg.data.source_request_id != ASM_REQUEST_GETMYSTATE) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("received msg is not valid, source_request_id(%d)", asm_rcv_msg.data.source_request_id);
		return false;
	}

	*sound_state = asm_rcv_msg.data.result_sound_state;

	debug_msg(">>>> Pid(%d), State(%s)", ASM_sound_handle[asm_index].asm_tid, ASM_sound_state_str[*sound_state]);


	return true;
}

EXPORT_API
bool ASM_attach_callback(ASM_sound_events_t sound_event, ASM_sound_cb_t callback, void *user_data, int *error_code)
{
	int asm_index = 0;

	if (callback == NULL || error_code == NULL) {
		if (error_code)
			*error_code = ERR_ASM_INVALID_PARAMETER;
		debug_error("invalid parameter");
		return false;
	}

	asm_index = __ASM_find_index_by_event(sound_event, asmgettid());
	if (asm_index == -1) {
		debug_error("Could not find index of the event(%d)", sound_event);
		return false;
	}

	if (!ASM_sound_handle[asm_index].asm_callback) {
		ASM_sound_handle[asm_index].asm_callback = callback;
		ASM_sound_handle[asm_index].user_data = user_data;
	} else {
		if (error_code)
			*error_code = ERR_ASM_ALREADY_REGISTERED;
		debug_error("asm_callback was already registered(0x%x)", ASM_sound_handle[asm_index].asm_callback);
		return false;
	}

	debug_msg(">>>> Pid(%d), Handle(%d), Event(%s), Callback(0x%x)", ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle,
		ASM_sound_events_str[ASM_sound_handle[asm_index].sound_event], ASM_sound_handle[asm_index].asm_callback);

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
	if (sound_event < ASM_EVENT_MEDIA_MMPLAYER || sound_event >= ASM_EVENT_MAX) {
		*error_code = ERR_ASM_EVENT_IS_INVALID;
		debug_error("invalid sound event(%d)",sound_event);
		return false;
	}
	handle = asm_handle;

	asm_index = __ASM_find_index_by_handle(handle);
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

	NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[asm_index].asm_tid, 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
		return false;
	}

	if (asm_rcv_msg.data.source_request_id != ASM_REQUEST_GETSTATE) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("received msg is not valid, source_request_id(%d)", asm_rcv_msg.data.source_request_id);
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
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
		debug_error("Invalid handle %d", asm_handle);
		return false;
	}

	handle = asm_handle;

	asm_index = __ASM_find_index_by_handle(handle);
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
		debug_log( "sound_state is PLAYING, func(0x%x)", func);
		if (func == NULL) {
			NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[asm_index].asm_tid, 0));
			if (ret == -1) {
				*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
				debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
				return false;
			}

			if (asm_rcv_msg.data.source_request_id != ASM_REQUEST_SETSTATE) {
				*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
				debug_error("received msg is not valid, source_request_id(%d)", asm_rcv_msg.data.source_request_id);
				return false;
			}
		}


		switch (asm_rcv_msg.data.result_sound_command) {
		case ASM_COMMAND_PAUSE:
		case ASM_COMMAND_STOP:
			debug_msg( " <<<<<<<<<<<<<<<< Received command : %d (STOP(3)/PAUSE(4)) >>>>>>>>>>>>>>>>>>>>\n", asm_rcv_msg.data.result_sound_command);
			if (handle == asm_rcv_msg.data.cmd_handle) {

				debug_msg("handle(%d) is same as asm_rcv_msg.data.cmd_handle", handle);

				asm_index = __ASM_find_index_by_handle(asm_rcv_msg.data.cmd_handle);
				if (asm_index == -1) {
					*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
					debug_error( "Can not find index from instance_id %ld, handle %d", asm_rcv_msg.instance_id, asm_rcv_msg.data.cmd_handle);
					return false;
				}

				if (asm_rcv_msg.data.result_sound_command == ASM_COMMAND_PAUSE) {
					ASM_sound_handle[asm_index].sound_state = ASM_STATE_PAUSE;
				} else if (asm_rcv_msg.data.result_sound_command == ASM_COMMAND_STOP) {
					ASM_sound_handle[asm_index].sound_state = ASM_STATE_STOP;
				}

				if (asm_rcv_msg.data.error_code){
					*error_code = asm_rcv_msg.data.error_code;
					debug_warning("error code: %x",*error_code);
				}
				return false;

			} else {
				unsigned int rcv_sound_status_value = 0;
				if (!__ASM_get_sound_state(&rcv_sound_status_value, error_code)) {
					debug_error("failed to __ASM_get_sound_state(), error(%d)", *error_code);
				}

				debug_msg("[ASM_CB] Callback : TID(%ld), handle(%d)", asm_rcv_msg.instance_id, asm_rcv_msg.data.cmd_handle);

				asm_index = __ASM_find_index_by_handle(asm_rcv_msg.data.cmd_handle);
				if (asm_index == -1) {
					*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
					debug_error("Can not find index from instance_id %ld, handle %d", asm_rcv_msg.instance_id, asm_rcv_msg.data.cmd_handle);
					return false;
				}

				if (ASM_sound_handle[asm_index].asm_callback!=NULL) {
					debug_msg( "[ASM_CB(%p) START]", ASM_sound_handle[asm_index].asm_callback);
					ASM_sound_handle[asm_index].asm_callback(asm_rcv_msg.data.cmd_handle, ASM_sound_handle[asm_index].sound_event, asm_rcv_msg.data.result_sound_command, rcv_sound_status_value, ASM_sound_handle[asm_index].user_data);
					debug_msg( "[ASM_CB END]");
				} else {
					debug_msg("asm callback is null");
				}
			}
			break;
		case ASM_COMMAND_PLAY:
		case ASM_COMMAND_NONE:
		case ASM_COMMAND_RESUME:
			ASM_sound_handle[asm_index].sound_state = sound_state;
			break;
		default:
			break;
		}

	}

	debug_fleave();

	return true;
}

EXPORT_API
bool ASM_set_sound_state (const int asm_handle, ASM_sound_events_t sound_event, ASM_sound_states_t sound_state, ASM_resource_t mm_resource, int *error_code)
{
	return ASM_set_sound_state_ex (asm_handle, sound_event, sound_state, mm_resource, error_code, NULL);
}

EXPORT_API
bool ASM_set_subsession (const int asm_handle, ASM_sound_sub_sessions_t subsession, int resource, int *error_code)
{
	int handle = 0;
	int asm_index = 0;
	int ret = 0;

	debug_fenter();

	if (error_code == NULL) {
		debug_error("error_code is null");
		return false;
	}

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
		debug_error("Invalid handle(%d)", asm_handle);
		return false;
	}

	if (subsession < ASM_SUB_SESSION_TYPE_VOICE || subsession >= ASM_SUB_SESSION_TYPE_MAX) {
		*error_code = ERR_ASM_INVALID_PARAMETER;
		debug_error("Invalid sub session type(%d)", subsession);
		return false;
	}

	handle = asm_handle;

	asm_index = __ASM_find_index_by_handle(handle);
	if (asm_index == -1) {
		debug_error("Can not find index of %d", handle);
		return false;
	}

	if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle, subsession, ASM_REQUEST_SET_SUBSESSION, ASM_sound_handle[asm_index].sound_state, resource, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}


	NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
		debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
		return false;
	}

	NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[asm_index].asm_tid, 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
		return false;
	}

	if (asm_rcv_msg.data.source_request_id != ASM_REQUEST_SET_SUBSESSION) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("received msg is not valid, source_request_id(%d)", asm_rcv_msg.data.source_request_id);
		return false;
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
bool ASM_get_subsession (const int asm_handle, ASM_sound_sub_sessions_t *subsession, int *error_code)
{
	int handle = 0;
	int asm_index = 0;
	int ret = 0;

	debug_fenter();

	if (error_code == NULL) {
		debug_error("error_code is null");
		return false;
	}

	if (subsession == NULL) {
		debug_error("subsession is null");
		*error_code = ERR_ASM_INVALID_PARAMETER;
		return false;
	}

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
		debug_error("Invalid handle %d \n", asm_handle);
		return false;
	}

	handle = asm_handle;

	asm_index = __ASM_find_index_by_handle(handle);
	if (asm_index == -1) {
		debug_error("Can not find index of %d", handle);
		return false;
	}

	if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle, 0, ASM_REQUEST_GET_SUBSESSION, 0, 0, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}


	NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
		debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
		return false;
	}

	NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[asm_index].asm_tid, 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
		return false;
	}

	if (asm_rcv_msg.data.source_request_id != ASM_REQUEST_GET_SUBSESSION) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("received msg is not valid, source_request_id(%d)", asm_rcv_msg.data.source_request_id);
		return false;
	}

	*subsession = asm_rcv_msg.data.result_sound_command;

	debug_msg(">>>> ASM_get_subsession with subsession value [%d]\n", *subsession);
	debug_fleave();

	return true;
}

EXPORT_API
bool ASM_set_subevent (const int asm_handle, ASM_sound_sub_events_t subevent, int *error_code)
{
	int handle = 0;
	int asm_index = 0;
	int ret = 0;
	ASM_sound_states_t sound_state = ASM_STATE_NONE;

	debug_fenter();

	if (error_code == NULL) {
		debug_error("error_code is null");
		return false;
	}

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
		debug_error("Invalid handle(%d)", asm_handle);
		return false;
	}

	if (subevent < ASM_SUB_EVENT_NONE || subevent >= ASM_SUB_EVENT_MAX) {
		*error_code = ERR_ASM_INVALID_PARAMETER;
		debug_error("Invalid sub event(%d)", subevent);
		return false;
	}

	handle = asm_handle;

	asm_index = __ASM_find_index_by_handle(handle);
	if (asm_index == -1) {
		debug_error("Can not find index of %d", handle);
		return false;
	}

	if (subevent == ASM_SUB_EVENT_NONE) {
		sound_state = ASM_STATE_STOP;
		if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle, subevent, ASM_REQUEST_SET_SUBEVENT, sound_state, 0, error_code)) {
			debug_error("failed to __asm_construct_snd_msg() for ASM_SUB_EVENT_NONE, error(%d)", *error_code);
			return false;
		}
	} else if (subevent < ASM_SUB_EVENT_MAX) {
		sound_state = ASM_STATE_PLAYING;
		if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle, subevent, ASM_REQUEST_SET_SUBEVENT, sound_state, 0, error_code)) {
			debug_error("failed to __asm_construct_snd_msg() for ASM_SUB_EVENT(%d), error(%d)", subevent, *error_code);
			return false;
		}
	}


	NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
		debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
		return false;
	}

	if (subevent == ASM_SUB_EVENT_NONE) {
		ASM_sound_handle[asm_index].sound_state = sound_state;
		debug_msg("Sent SUB_EVENT_NONE, do not wait a returned message");
		debug_fleave();
		return true;
	}

	NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[asm_index].asm_tid, 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
		return false;
	}

	if (asm_rcv_msg.data.source_request_id != ASM_REQUEST_SET_SUBEVENT) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("received msg is not valid, source_request_id(%d)", asm_rcv_msg.data.source_request_id);
		return false;
	}

	switch (asm_rcv_msg.data.result_sound_command) {
	case ASM_COMMAND_PAUSE:
	case ASM_COMMAND_STOP:
		debug_msg( " <<<<<<<<<<<<<<<< Received command : %d (STOP(3)/PAUSE(4)) >>>>>>>>>>>>>>>>>>>>\n", asm_rcv_msg.data.result_sound_command);
		if (handle == asm_rcv_msg.data.cmd_handle) {

			debug_msg("handle(%d) is same as asm_rcv_msg.data.cmd_handle", handle);

			asm_index = __ASM_find_index_by_handle(asm_rcv_msg.data.cmd_handle);
			if (asm_index == -1) {
				*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
				debug_error( "Can not find index from instance_id %ld, handle %d", asm_rcv_msg.instance_id, asm_rcv_msg.data.cmd_handle);
				return false;
			}

			if (asm_rcv_msg.data.result_sound_command == ASM_COMMAND_PAUSE) {
				ASM_sound_handle[asm_index].sound_state = ASM_STATE_PAUSE;
			} else if (asm_rcv_msg.data.result_sound_command == ASM_COMMAND_STOP) {
				ASM_sound_handle[asm_index].sound_state = ASM_STATE_STOP;
			}

			if (asm_rcv_msg.data.error_code){
				*error_code = asm_rcv_msg.data.error_code;
				debug_warning("error code: %x",*error_code);
			}
			return false;

		} else {
			unsigned int rcv_sound_status_value = 0;
			if (!__ASM_get_sound_state(&rcv_sound_status_value, error_code)) {
				debug_error("failed to __ASM_get_sound_state(), error(%d)", *error_code);
			}
			asm_index = __ASM_find_index_by_handle(asm_rcv_msg.data.cmd_handle);
			if (asm_index == -1) {
				*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
				debug_error("Can not find index from instance_id %ld, handle %d", asm_rcv_msg.instance_id, asm_rcv_msg.data.cmd_handle);
				return false;
			}

			if (ASM_sound_handle[asm_index].asm_callback!=NULL) {
				debug_msg( "[ASM_CB(%p) START]", ASM_sound_handle[asm_index].asm_callback);
				ASM_sound_handle[asm_index].asm_callback(asm_rcv_msg.data.cmd_handle, ASM_sound_handle[asm_index].sound_event, asm_rcv_msg.data.result_sound_command, rcv_sound_status_value, ASM_sound_handle[asm_index].user_data);
				debug_msg( "[ASM_CB END]");
			} else {
				debug_msg("asm callback is null");
			}
		}
		break;
	case ASM_COMMAND_PLAY:
	case ASM_COMMAND_NONE:
	case ASM_COMMAND_RESUME:
		ASM_sound_handle[asm_index].sound_state = sound_state;
		break;
	default:
		break;
	}

	debug_fleave();

	return true;
}

EXPORT_API
bool ASM_get_subevent (const int asm_handle, ASM_sound_sub_events_t *subevent, int *error_code)
{
	int handle = 0;
	int asm_index = 0;
	int ret = 0;

	debug_fenter();

	if (error_code == NULL) {
		debug_error("error_code is null");
		return false;
	}

	if (subevent == NULL) {
		debug_error("subevent is null");
		*error_code = ERR_ASM_INVALID_PARAMETER;
		return false;
	}

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
		debug_error("Invalid handle %d \n", asm_handle);
		return false;
	}

	handle = asm_handle;

	asm_index = __ASM_find_index_by_handle(handle);
	if (asm_index == -1) {
		debug_error("Can not find index of %d", handle);
		return false;
	}

	if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle, 0, ASM_REQUEST_GET_SUBEVENT, 0, 0, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}


	NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
		debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
		return false;
	}

	NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[asm_index].asm_tid, 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
		return false;
	}

	if (asm_rcv_msg.data.source_request_id != ASM_REQUEST_GET_SUBEVENT) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("received msg is not valid, source_request_id(%d)", asm_rcv_msg.data.source_request_id);
		return false;
	}

	*subevent = asm_rcv_msg.data.result_sound_command;

	debug_msg(">>>> ASM_get_subevent with subevent value [%d]\n", *subevent);
	debug_fleave();

	return true;
}


EXPORT_API
bool ASM_set_session_option (const int asm_handle, int option_flags, int *error_code)
{
	int handle = 0;
	int asm_index = 0;
	int ret = 0;
	ASM_sound_states_t sound_state = ASM_STATE_NONE;

	debug_fenter();

	if (error_code == NULL) {
		debug_error("error_code is null");
		return false;
	}

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
		debug_error("Invalid handle(%d)", asm_handle);
		return false;
	}

	if (option_flags < 0) {
		*error_code = ERR_ASM_INVALID_PARAMETER;
		debug_error("Invalid option_flags(%x)", option_flags);
		return false;
	}

	handle = asm_handle;

	asm_index = __ASM_find_index_by_handle(handle);
	if (asm_index == -1) {
		debug_error("Can not find index of %d", handle);
		return false;
	}

	if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle, option_flags, ASM_REQUEST_SET_SESSION_OPTIONS, sound_state, 0, error_code)) {
		debug_error("failed to __asm_construct_snd_msg() for option_flags, error(%d)", *error_code);
		return false;
	}

	NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
		debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
		return false;
	}

	NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[asm_index].asm_tid, 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
		return false;
	}

	if (asm_rcv_msg.data.source_request_id != ASM_REQUEST_SET_SESSION_OPTIONS) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("received msg is not valid, source_request_id(%d)", asm_rcv_msg.data.source_request_id);
		return false;
	}

	switch (asm_rcv_msg.data.result_sound_command) {
	case ASM_COMMAND_PAUSE:
	case ASM_COMMAND_STOP:
		debug_msg( " <<<<<<<<<<<<<<<< Received command : %d (STOP(3)/PAUSE(4)) >>>>>>>>>>>>>>>>>>>>\n", asm_rcv_msg.data.result_sound_command);
		if (handle == asm_rcv_msg.data.cmd_handle) {

			debug_msg("handle(%d) is same as asm_rcv_msg.data.cmd_handle", handle);

			asm_index = __ASM_find_index_by_handle(asm_rcv_msg.data.cmd_handle);
			if (asm_index == -1) {
				*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
				debug_error( "Can not find index from instance_id %ld, handle %d", asm_rcv_msg.instance_id, asm_rcv_msg.data.cmd_handle);
				return false;
			}

			if (asm_rcv_msg.data.result_sound_command == ASM_COMMAND_PAUSE) {
				ASM_sound_handle[asm_index].sound_state = ASM_STATE_PAUSE;
			} else if (asm_rcv_msg.data.result_sound_command == ASM_COMMAND_STOP) {
				ASM_sound_handle[asm_index].sound_state = ASM_STATE_STOP;
			}

			if (asm_rcv_msg.data.error_code){
				*error_code = asm_rcv_msg.data.error_code;
				debug_warning("error code: %x",*error_code);
			}
			return false;

		}
		break;
	case ASM_COMMAND_PLAY:
		ASM_sound_handle[asm_index].option_flags = option_flags;
		break;
	default:
		break;
	}

	debug_fleave();

	return true;
}

EXPORT_API
bool ASM_get_session_option (const int asm_handle, int *option_flags, int *error_code)
{
	int handle = 0;
	int asm_index = 0;
	int ret = 0;

	debug_fenter();

	if (error_code == NULL) {
		debug_error("error_code is null");
		return false;
	}

	if (option_flags == NULL) {
		debug_error("option_flags is null");
		*error_code = ERR_ASM_INVALID_PARAMETER;
		return false;
	}

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
		debug_error("Invalid handle %d \n", asm_handle);
		return false;
	}

	handle = asm_handle;

	asm_index = __ASM_find_index_by_handle(handle);
	if (asm_index == -1) {
		debug_error("Can not find index of %d", handle);
		return false;
	}

	if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle, 0, ASM_REQUEST_GET_SESSION_OPTIONS, 0, 0, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}

	NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
		debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
		return false;
	}

	NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[asm_index].asm_tid, 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
		return false;
	}

	if (asm_rcv_msg.data.source_request_id != ASM_REQUEST_GET_SESSION_OPTIONS) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("received msg is not valid, source_request_id(%d)", asm_rcv_msg.data.source_request_id);
		return false;
	}

	if (asm_rcv_msg.data.result_sound_command == ASM_COMMAND_STOP) {
		*error_code = asm_rcv_msg.data.error_code;
		debug_error("received handle is not valid");
		return false;
	}

	*option_flags = asm_rcv_msg.data.option_flags;
	if (ASM_sound_handle[asm_index].option_flags != *option_flags) {
		debug_error("received flag(%x) from server is not same as local's(%x)", *option_flags, ASM_sound_handle[asm_index].option_flags);
		return false;
	}

	debug_msg(">>>> option flags [%x]\n", *option_flags);

	debug_fleave();

	return true;
}

EXPORT_API
bool ASM_reset_resumption_info(const int asm_handle, int *error_code)
{
	int handle = 0;
	int asm_index = 0;
	int ret = 0;

	debug_fenter();

	if (error_code == NULL) {
		debug_error("error_code is null");
		return false;
	}

	if (asm_handle < 0 || asm_handle >= ASM_SERVER_HANDLE_MAX) {
		*error_code = ERR_ASM_LOCAL_HANDLE_IS_INVALID;
		debug_error("Invalid handle %d \n", asm_handle);
		return false;
	}

	handle = asm_handle;

	asm_index = __ASM_find_index_by_handle(handle);
	if (asm_index == -1) {
		debug_error("Can not find index of %d", handle);
		return false;
	}

	if (!__asm_construct_snd_msg(ASM_sound_handle[asm_index].asm_tid, ASM_sound_handle[asm_index].handle, 0, ASM_REQUEST_RESET_RESUME_TAG, 0, 0, error_code)) {
		debug_error("failed to __asm_construct_snd_msg(), error(%d)", *error_code);
		return false;
	}

	NO_EINTR(ret = msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_SND_ERROR;
		debug_error("failed to msgsnd(%d,%s)", errno, strerror(errno));
		return false;
	}

	NO_EINTR(ret = msgrcv(asm_rcv_msgid, (void *)&asm_rcv_msg, sizeof(asm_rcv_msg.data), ASM_sound_handle[asm_index].asm_tid, 0));
	if (ret == -1) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("failed to msgrcv(%d,%s)", errno, strerror(errno));
		return false;
	}

	if (asm_rcv_msg.data.source_request_id != ASM_REQUEST_RESET_RESUME_TAG) {
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		debug_error("received msg is not valid, source_request_id(%d)", asm_rcv_msg.data.source_request_id);
		return false;
	}

	switch (asm_rcv_msg.data.result_sound_command) {
	case ASM_COMMAND_PLAY:
		debug_msg(" >>>> reset information of resumption successfully");
		break;
	default:
		debug_error("received message is abnormal..result_sound_command(%d) from ASM server", asm_rcv_msg.data.result_sound_command);
		*error_code = ERR_ASM_MSG_QUEUE_RCV_ERROR;
		return false;
	}

	debug_leave();

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


#if defined(CONFIG_ENABLE_SIGNAL_HANDLER)
struct sigaction ASM_int_old_action;
struct sigaction ASM_abrt_old_action;
struct sigaction ASM_segv_old_action;
struct sigaction ASM_term_old_action;
struct sigaction ASM_sys_old_action;
struct sigaction ASM_xcpu_old_action;

void __ASM_signal_handler(int signo)
{
	int exit_pid = 0;
	int asm_index = 0;

	debug_warning("ENTER, sig.num(%d)",signo);

	/* signal block -------------- */
	sigset_t old_mask, all_mask;
	sigfillset(&all_mask);
	sigprocmask(SIG_BLOCK, &all_mask, &old_mask);

	for (asm_index=0 ;asm_index < ASM_HANDLE_MAX; asm_index++) {
		if (ASM_sound_handle[asm_index].is_used == true &&
				ASM_sound_handle[asm_index].is_for_watching == false) {
			exit_pid = ASM_sound_handle[asm_index].asm_tid;
			if (exit_pid == asmgettid()) {
				asm_snd_msg.instance_id = exit_pid;
				asm_snd_msg.data.handle = ASM_sound_handle[asm_index].handle;
				asm_snd_msg.data.request_id = ASM_REQUEST_EMERGENT_EXIT;
				asm_snd_msg.data.sound_event = ASM_sound_handle[asm_index].sound_event;
				asm_snd_msg.data.sound_state = ASM_sound_handle[asm_index].sound_state;

				if (msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0) < 0 ) {
					debug_msg( "msgsnd() failed, tid=%ld, reqid=%d, handle=0x%x, state=0x%x event=%d size=%d",asm_snd_msg.instance_id,
							asm_snd_msg.data.request_id, asm_snd_msg.data.handle, asm_snd_msg.data.sound_state, asm_snd_msg.data.sound_event, sizeof(asm_snd_msg.data) );
					int tmpid = msgget((key_t)2014, 0666);
					if (msgsnd(tmpid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0) > 0) {
						debug_warning("msgsnd() succeed");
					} else {
						debug_error("msgsnd() retry also failed");
					}
				}
			}
		}
	}

	sigprocmask(SIG_SETMASK, &old_mask, NULL);
	/* signal unblock ------------ */

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

	debug_warning("LEAVE");
}

#endif
void __attribute__((constructor)) __ASM_init_module(void)
{
#if defined(CONFIG_ENABLE_SIGNAL_HANDLER)
	struct sigaction ASM_action;
	ASM_action.sa_handler = __ASM_signal_handler;
	ASM_action.sa_flags = SA_NOCLDSTOP;
	int asm_index = 0;

	debug_fenter();

	for (asm_index = 0; asm_index < ASM_HANDLE_MAX; asm_index++) {
		ASM_sound_handle[asm_index].handle = ASM_HANDLE_INIT_VAL;
	}

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

	for (asm_index = 0; asm_index < ASM_HANDLE_MAX; asm_index++) {
		if (ASM_sound_handle[asm_index].is_used == true &&
				ASM_sound_handle[asm_index].is_for_watching == false) {
			exit_pid = ASM_sound_handle[asm_index].asm_tid;
			if (exit_pid == asmgettid()) {
				asm_snd_msg.instance_id = exit_pid;
				asm_snd_msg.data.handle = ASM_sound_handle[asm_index].handle;
				asm_snd_msg.data.request_id = ASM_REQUEST_EMERGENT_EXIT;
				asm_snd_msg.data.sound_event = ASM_sound_handle[asm_index].sound_event;
				asm_snd_msg.data.sound_state = ASM_sound_handle[asm_index].sound_state;

				if (msgsnd(asm_snd_msgid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0) < 0 ) {
					debug_msg( "msgsnd() failed, tid=%ld, reqid=%d, handle=0x%x, state=0x%x event=%d size=%d",asm_snd_msg.instance_id,
							asm_snd_msg.data.request_id, asm_snd_msg.data.handle, asm_snd_msg.data.sound_state, asm_snd_msg.data.sound_event, sizeof(asm_snd_msg.data) );
					int tmpid = msgget((key_t)2014, 0666);
					if (msgsnd(tmpid, (void *)&asm_snd_msg, sizeof(asm_snd_msg.data), 0) > 0) {
						debug_warning("msgsnd() succeed");
					} else {
						debug_error("msgsnd() retry also failed");
					}
				}
			}
		}
	}

#endif

	if (g_asm_thread) {
		g_main_loop_quit(g_asm_loop);
		g_thread_join(g_asm_thread);
		debug_log("after thread join");
		g_main_loop_unref(g_asm_loop);
		g_asm_thread = NULL;
	}
	sigaction(SIGINT, &ASM_int_old_action, NULL);
	sigaction(SIGABRT, &ASM_abrt_old_action, NULL);
	sigaction(SIGSEGV, &ASM_segv_old_action, NULL);
	sigaction(SIGTERM, &ASM_term_old_action, NULL);
	sigaction(SIGSYS, &ASM_sys_old_action, NULL);
	sigaction(SIGXCPU, &ASM_xcpu_old_action, NULL);

	debug_fleave();
}

