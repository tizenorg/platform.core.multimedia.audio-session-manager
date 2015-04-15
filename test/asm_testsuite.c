#include <stdio.h>
#include <glib.h>
#include <stdlib.h>

#include <audio-session-manager.h>


GMainLoop *g_loop;
GThreadPool* g_pool;
gint g_asm_handle = -1;
gint g_event_type = ASM_EVENT_NONE;
gint g_sub_event_type = ASM_SUB_EVENT_NONE;
gint g_sub_session_type = ASM_SUB_SESSION_TYPE_VOICE;
gint g_asm_state = ASM_STATE_NONE;
ASM_resource_t g_resource = ASM_RESOURCE_NONE;
gboolean thread_run;

ASM_cb_result_t
asm_callback (int handle, ASM_event_sources_t event_src, ASM_sound_commands_t command, unsigned int sound_status, void* cb_data)
{
	g_print ("\n[%s] handle = %d, event src = %d, command = %d, sound_state = %d, cb_data = %p\n\n",
						__func__, handle, event_src, command, sound_status, cb_data);
	return ASM_CB_RES_NONE;
}

ASM_cb_result_t
watch_callback (int handle, ASM_sound_events_t sound_event, ASM_sound_states_t sound_state, void* cb_data)
{
	g_print ("\n[%s] handle = %d, sound_event = %d, sound_state = %d, cb_data = %p\n\n",
						__func__, handle, sound_event, sound_state, cb_data);
	return ASM_CB_RES_NONE;
}

void print_menu_main(void)
{
	printf("===============ASM Testsuite==============\n");
	printf(" a. Register ASM handle\n");
	printf(" o. Set options\n");
	printf(" r. Set resource\n");
	printf(" s. Set state\t\t");
	printf(" x. Get state\n");
	printf(" d. Set sub-event\t");
	printf(" c. Get sub-event\n");
	printf(" f. Set sub-session\t");
	printf(" v. Get sub-session\n");
	printf(" z. Unregister ASM handle\n");
	printf(" j. Set watcher\n");
	printf(" m. Unset watcher\n");
	printf(" w. Reset resume tag\n");
	printf(" q. Quit\n");
	printf("==========================================\n");
	printf(">> ");
}

void menu_unregister(void)
{
	gint errorcode = 0;
	if(g_asm_handle == -1) {
		g_print("Register handle first..\n\n");
		return;
	}
	if( !ASM_unregister_sound(g_asm_handle, g_event_type, &errorcode) ) {
		g_print("ASM_unregister_sound() failed 0x%X\n\n", errorcode);
	} else {
		g_print("Unregister success..\n\n");
		g_asm_handle = -1;
	}
}

void menu_register(void)
{
	char key = 0;
	int input = 0;
	gint errorcode = 0;
	gint pid = -1;

	while(1) {
		printf("==========select ASM event=============\n");
		printf(" 0. ASM_EVENT_MEDIA_MMPLAYER \n");
		printf(" 1. ASM_EVENT_MEDIA_MMCAMCORDER \n");
		printf(" 2. ASM_EVENT_MEDIA_MMSOUND\n");
		printf(" 3. ASM_EVENT_MEDIA_OPENAL\n");
		printf(" 4. ASM_EVENT_MEDIA_FMRADIO\n");
		printf(" 5. ASM_EVENT_MEDIA_WEBKIT\n");
		printf(" 6. ASM_EVENT_NOTIFY\n");
		printf(" 7. ASM_EVENT_ALARM\n");
		printf(" 8. ASM_EVENT_EARJACK_UNPLUG\n");
		printf(" 9. ASM_EVENT_CALL\n");
		printf(" a. ASM_EVENT_VIDEOCALL\n");
		printf(" b. ASM_EVENT_VOIP\n");
		printf(" c. ASM_EVENT_MONITOR\n");
		printf(" d. ASM_EVENT_EMERGENCY\n");
		printf(" e. ASM_EVENT_EXCLUSIVE_RESOURCE\n");
		printf(" f. ASM_EVENT_VOICE_RECOGNITION\n");
		printf(" g. ASM_EVENT_MMCAMCORDER_AUDIO\n");
		printf(" h. ASM_EVENT_MMCAMCORDER_VIDEO\n");
		printf(" q. Back to main menu\n");
		printf("=======================================\n");
		printf(">> ");


		while( (input = getchar())!= '\n' && input != EOF) {
			key = (char)input;
		}
		switch (key) {
			case '0':
				g_event_type = ASM_EVENT_MEDIA_MMPLAYER;
				break;
			case '1':
				g_event_type = ASM_EVENT_MEDIA_MMCAMCORDER;
				break;
			case '2':
				g_event_type = ASM_EVENT_MEDIA_MMSOUND;
				break;
			case '3':
				g_event_type = ASM_EVENT_MEDIA_OPENAL;
				break;
			case '4':
				g_event_type = ASM_EVENT_MEDIA_FMRADIO;
				break;
			case '5':
				g_event_type = ASM_EVENT_MEDIA_WEBKIT;
				break;
			case '6':
				g_event_type = ASM_EVENT_NOTIFY;
				break;
			case '7':
				g_event_type = ASM_EVENT_ALARM;
				break;
			case '8':
				g_event_type = ASM_EVENT_EARJACK_UNPLUG;
				break;
			case '9':
				g_event_type = ASM_EVENT_CALL;
				break;
			case 'a':
				g_event_type = ASM_EVENT_VIDEOCALL;
				break;
			case 'b':
				g_event_type = ASM_EVENT_VOIP;
				break;
			case 'c':
				g_event_type = ASM_EVENT_MONITOR;
				break;
			case 'd':
				g_event_type = ASM_EVENT_EMERGENCY;
				break;
			case 'e':
				g_event_type = ASM_EVENT_EXCLUSIVE_RESOURCE;
				break;
			case 'f':
				g_event_type = ASM_EVENT_VOICE_RECOGNITION;
				break;
			case 'g':
				g_event_type = ASM_EVENT_MMCAMCORDER_AUDIO;
				break;
			case 'h':
				g_event_type = ASM_EVENT_MMCAMCORDER_VIDEO;
				break;
			case 'q':
				return;
			default :
				g_print("select event again...\n");
				g_event_type = -1;
				break;
		}
		if (g_event_type == -1) {
			continue;
		}
		if (g_asm_handle != -1) {
			g_print("Unregister handle first..\n\n");
			break;
		}
		if( ! ASM_register_sound(pid, &g_asm_handle, g_event_type, ASM_STATE_NONE, asm_callback, NULL, g_resource, &errorcode)) {
			g_print("ASM_register_sound() failed, error = %x\n\n", errorcode);
			break;
		} else {
			g_print("ASM_register_sound() success, ASM handle=%d, ASM_EVENT=%d, ASM_RESOURCE=%d, ASM_STATE_NONE.\n\n", g_asm_handle, g_event_type, g_resource);
			break;
		}
	}
}

void menu_set_options(void)
{
	char key = 0;
	int input = 0;
	int options = 0;
	gint errorcode = 0;

	if( ! ASM_get_session_option(g_asm_handle, &options, &errorcode)) {
		g_print("ASM_get_session_option() failed, error = %x\n\n", errorcode);
	}
	while(1) {
		printf("===========ASM SESSION OPTION===========\n");
		printf(">> current(0x%x)\n", options);
		printf(" 0. ASM_SESSION_OPTION_PAUSE_OTHERS\n");
		printf(" 1. ASM_SESSION_OPTION_UNINTERRUTIBLE\n");
		printf(" 2. ASM_SESSION_OPTION_RESUME_BY_MEDIA_PAUSED\n");
		printf(" r. Reset ASM SESSION OPTION\n");
		printf(" q. Back to main menu\n");
		printf("========================================\n");
		printf(">> ");
		while( (input = getchar())!= '\n' && input != EOF) {
			key = (char)input;
		}

		switch (key) {
			case '0':
				options |= ASM_SESSION_OPTION_PAUSE_OTHERS;
				break;
			case '1':
				options |= ASM_SESSION_OPTION_UNINTERRUPTIBLE;
				break;
			case '2':
				options |= ASM_SESSION_OPTION_RESUME_BY_MEDIA_PAUSED;
				break;
			case 'r':
				options = 0;
				break;
			case 'q':
				return;
			default :
				g_print("select ASM session option again...\n");
				options = -1;
				break;
		}
		if (options == -1) {
			continue;
		}
		if( ! ASM_set_session_option(g_asm_handle, options, &errorcode)) {
			g_print("ASM_get_session_option() failed, error = %x\n\n", errorcode);
		}
	}
}

void menu_set_resource(void)
{
	int ret = 0;
	char key = 0;
	int input = 0;
	while(1) {
		printf("==========ASM RESOURCE==========\n");
		printf(">> current(0x%x)\n", g_resource);
		printf(" 0. ASM_RESOURCE_CAMERA\n");
		printf(" 1. ASM_RESOURCE_VIDEO_OVERLAY\n");
		printf(" 2. ASM_RESOURCE_STREAMING\n");
		printf(" 3. ASM_RESOURCE_HW_DECODER\n");
		printf(" 4. ASM_RESOURCE_HW_ENCODER\n");
		printf(" 5. ASM_RESOURCE_RADIO_TUNNER\n");
		printf(" 6. ASM_RESOURCE_TV_TUNNER\n");
		printf(" 9. ASM_RESOURCE_VOICECONTROL\n");
		printf(" r. Reset ASM RESOURCE\n");
		printf(" q. Back to main menu\n");
		printf("================================\n");
		printf(">> ");
		while( (input = getchar())!= '\n' && input != EOF) {
			key = (char)input;
		}

		switch (key) {
			case '0':
				g_resource |= ASM_RESOURCE_CAMERA;
				break;
			case '1':
				g_resource |= ASM_RESOURCE_VIDEO_OVERLAY;
				break;
			case '2':
				g_resource |= ASM_RESOURCE_STREAMING;
				break;
			case '3':
				g_resource |= ASM_RESOURCE_HW_DECODER;
				break;
			case '4':
				g_resource |= ASM_RESOURCE_HW_ENCODER;
				break;
			case '5':
				g_resource |= ASM_RESOURCE_RADIO_TUNNER;
				break;
			case '6':
				g_resource |= ASM_RESOURCE_TV_TUNNER;
				break;
			case '9':
				g_resource |= ASM_RESOURCE_VOICECONTROL;
				break;
			case 'r':
				g_resource = ASM_RESOURCE_NONE;
				return;
			case 'q':
				return;
			default :
				g_print("select ASM resource again...\n");
				g_resource = -1;
				break;
		}
		if (g_resource == -1) {
			continue;
		}
	}
}

void menu_set_state(void)
{
	int ret = 0;
	char key = 0;
	int input = 0;
	while(1) {
		printf("==========ASM state==========\n");
		printf(" 0. ASM_STATE_NONE\n");
		printf(" 1. ASM_STATE_PLAYING\n");
		printf(" 2. ASM_STATE_WAITING\n");
		printf(" 3. ASM_STATE_STOP\n");
		printf(" 4. ASM_STATE_PAUSE\n");
		printf(" q. Back to main menu\n");
		printf("=============================\n");
		printf(">> ");
		while( (input = getchar())!= '\n' && input != EOF) {
			key = (char)input;
		}

		switch (key) {
			case '0':
				g_asm_state = ASM_STATE_NONE;
				break;
			case '1':
				g_asm_state = ASM_STATE_PLAYING;
				break;
			case '2':
				g_asm_state = ASM_STATE_WAITING;
				break;
			case '3':
				g_asm_state = ASM_STATE_STOP;
				break;
			case '4':
				g_asm_state = ASM_STATE_PAUSE;
				break;
			case 'q':
				return;
			default :
				g_print("select ASM state again...\n");
				g_asm_state = 9;
				break;
		}
		if (g_asm_state == 9) {
			continue;
		}
		/* set ASM sound state */
		if( ! ASM_set_sound_state( g_asm_handle, g_event_type, g_asm_state, g_resource, &ret) ) {
			g_print("ASM_set_sound_state() failed, Set state to [%d] failed 0x%X\n\n", g_asm_state, ret);
			break;
		} else {
			g_print("ASM_set_sound_state() success, ASM handle=%d, ASM_EVENT=%d, ASM_RESOURCE=%d, ASM_STATE=%d\n\n", g_asm_handle, g_event_type, g_resource, g_asm_state);
			break;
		}
	}
}

void menu_set_sub_event(void)
{
	int ret = 0;
	char key = 0;
	int input = 0;
	gint error = 0;

	while(1) {
		printf("==========ASM sub-event==========\n");
		printf(" 0. ASM_SUB_EVENT_NONE\n");
		printf(" 1. ASM_SUB_EVENT_SHARE\n");
		printf(" 2. ASM_SUB_EVENT_EXCLUSIVE\n");
		printf(" q. Back to main menu\n");
		printf("=============================\n");
		printf(">> ");
		while( (input = getchar())!= '\n' && input != EOF) {
			key = (char)input;
		}

		switch (key) {
			case '0':
				g_sub_event_type = ASM_SUB_EVENT_NONE;
				break;
			case '1':
				g_sub_event_type = ASM_SUB_EVENT_SHARE;
				break;
			case '2':
				g_sub_event_type = ASM_SUB_EVENT_EXCLUSIVE;
				break;
			case 'q':
				return;
			default :
				g_print("select ASM sub event again...\n");
				g_sub_event_type = 9;
				break;
		}
		if (g_sub_event_type == 9) {
			continue;
		}
		/* set ASM set subevent */
		ret = ASM_set_subevent (g_asm_handle, g_sub_event_type, &error);
		if (!ret) {
			g_print("ASM_set_subevent() failed, Set state to [%d] failed 0x%X\n\n", g_sub_event_type, error);
		}
	}
}

void menu_set_sub_session(void)
{
	int ret = 0;
	char key = 0;
	int input = 0;
	gint error = 0;
	ASM_resource_t resource = ASM_RESOURCE_NONE;

	while(1) {
		printf("===============ASM sub-session==========================\n");
		printf(" 0. ASM_SUB_SESSION_TYPE_VOICE\n");
		printf(" 1. ASM_SUB_SESSION_TYPE_RINGTONE\n");
		printf(" 2. ASM_SUB_SESSION_TYPE_MEDIA\n");
		printf(" 3. ASM_SUB_SESSION_TYPE_INIT\n");
		printf(" 4. ASM_SUB_SESSION_TYPE_VR_NORMAL\n");
		printf(" 5. ASM_SUB_SESSION_TYPE_VR_DRIVE\n");
		printf(" 6. ASM_SUB_SESSION_TYPE_RECORD_STEREO\n");
		printf(" 7. ASM_SUB_SESSION_TYPE_RECORD_MONO\n");
		printf(" q. Back to main menu\n");
		printf("========================================================\n");
		printf(">> ");
		while( (input = getchar())!= '\n' && input != EOF) {
			key = (char)input;
		}

		switch (key) {
			case '0':
				g_sub_session_type = ASM_SUB_SESSION_TYPE_VOICE;
				break;
			case '1':
				g_sub_session_type = ASM_SUB_SESSION_TYPE_RINGTONE;
				break;
			case '2':
				g_sub_session_type = ASM_SUB_SESSION_TYPE_MEDIA;
				break;
			case '3':
				g_sub_session_type = ASM_SUB_SESSION_TYPE_INIT;
				break;
			case '4':
				g_sub_session_type = ASM_SUB_SESSION_TYPE_VR_NORMAL;
				break;
			case '5':
				g_sub_session_type = ASM_SUB_SESSION_TYPE_VR_DRIVE;
				break;
			case '6':
				g_sub_session_type = ASM_SUB_SESSION_TYPE_RECORD_STEREO;
				break;
			case '7':
				g_sub_session_type = ASM_SUB_SESSION_TYPE_RECORD_MONO;
				break;
			case 'q':
				return;
			default :
				g_print("select ASM sub session again...\n");
				g_sub_session_type = -1;
				break;
		}
		if (g_sub_session_type == -1) {
			continue;
		}
		/* set ASM set subsession */
		ret = ASM_set_subsession (g_asm_handle, g_sub_session_type, resource, &error);
		if (!ret) {
			g_print("ASM_set_subsession() failed, sub_session_type[%d], resource[%d], 0x%X\n\n", g_sub_session_type, resource, error);
		}
	}
}

void menu_set_watch_state(gint pid, gint event_type)
{
	int ret = 0;
	char key = 0;
	int input = 0;
	gint asm_state = 0;
	while(1) {
		printf("======ASM state to be watched======\n");
		printf(" 1. ASM_STATE_NONE\n");
		printf(" 2. ASM_STATE_PLAYING\n");
		printf(" 3. ASM_STATE_STOP\n");
		printf(" 4. ASM_STATE_PAUSE\n");
		printf(" q. Back to main menu\n");
		printf("===================================\n");
		printf(">> ");
		while( (input = getchar())!= '\n' && input != EOF) {
			key = (char)input;
		}

		switch (key) {
			case '1':
				asm_state = ASM_STATE_NONE;
				break;
			case '2':
				asm_state = ASM_STATE_PLAYING;
				break;
			case '3':
				asm_state = ASM_STATE_STOP;
				break;
			case '4':
				asm_state = ASM_STATE_PAUSE;
				break;
			case 'q':
				return;
			default :
				g_print("select ASM state again...\n");
				asm_state = 9;
				break;
		}
		if (asm_state == 9) {
			continue;
		}
		/* set ASM sound state */
		if( ! ASM_set_watch_session( pid, event_type, asm_state, watch_callback, NULL, &ret) ) {
			g_print("ASM_set_watch_session() failed, 0x%X\n\n", ret);
			break;
		} else {
			g_print("ASM_set_watch_session() success");
			break;
		}
	}
}

void menu_set_watch(void)
{
	char key = 0;
	int input = 0;
	gint pid = -1;
	gint event_type = 0;
	g_resource = ASM_RESOURCE_NONE;

	while(1) {
		printf("======select ASM event to be watched======\n");
		printf(" 0. ASM_EVENT_MEDIA_MMPLAYER \n");
		printf(" 1. ASM_EVENT_MEDIA_MMPLAYER \n");
		printf(" 2. ASM_EVENT_MEDIA_MMSOUND\n");
		printf(" 3. ASM_EVENT_MEDIA_OPENAL\n");
		printf(" 4. ASM_EVENT_MEDIA_FMRADIO\n");
		printf(" 5. ASM_EVENT_MEDIA_WEBKIT\n");
		printf(" 6. ASM_EVENT_NOTIFY\n");
		printf(" 7. ASM_EVENT_ALARM\n");
		printf(" 8. ASM_EVENT_EARJACK_UNPLUG\n");
		printf(" 9. ASM_EVENT_CALL\n");
		printf(" a. ASM_EVENT_VIDEOCALL\n");
		printf(" b. ASM_EVENT_VOIP\n");
		printf(" c. ASM_EVENT_MONITOR\n");
		printf(" d. ASM_EVENT_EMERGENCY\n");
		printf(" q. Back to main menu\n");
		printf("==========================================\n");
		printf(">> ");


		while( (input = getchar())!= '\n' && input != EOF) {
			key = (char)input;
		}
		switch (key) {
		case '0':
			event_type = ASM_EVENT_MEDIA_MMPLAYER;
			break;
		case '1':
			event_type = ASM_EVENT_MEDIA_MMCAMCORDER;
			break;
		case '2':
			event_type = ASM_EVENT_MEDIA_MMSOUND;
			break;
		case '3':
			event_type = ASM_EVENT_MEDIA_OPENAL;
			break;
		case '4':
			event_type = ASM_EVENT_MEDIA_FMRADIO;
			break;
		case '5':
			event_type = ASM_EVENT_MEDIA_WEBKIT;
			break;
		case '6':
			event_type = ASM_EVENT_NOTIFY;
			break;
		case '7':
			event_type = ASM_EVENT_ALARM;
			break;
		case '8':
			event_type = ASM_EVENT_EARJACK_UNPLUG;
			break;
		case '9':
			event_type = ASM_EVENT_CALL;
			break;
		case 'a':
			event_type = ASM_EVENT_VIDEOCALL;
			break;
		case 'b':
			event_type = ASM_EVENT_VOIP;
			break;
		case 'c':
			event_type = ASM_EVENT_MONITOR;
			break;
		case 'd':
			event_type = ASM_EVENT_EMERGENCY;
			break;
		case 'q':
			return;
		default :
			g_print("select event again...\n");
			event_type = -1;
			break;
		}
		if (event_type == -1) {
			continue;
		}

		menu_set_watch_state(pid, event_type);
		break;
	}
}

void menu_unset_watch_state(gint pid, gint event_type)
{
	int ret = 0;
	char key = 0;
	int input = 0;
	gint asm_state = 0;
	while(1) {
		printf("======ASM state to be watched======\n");
		printf(" 1. ASM_STATE_NONE\n");
		printf(" 2. ASM_STATE_PLAYING\n");
		printf(" 3. ASM_STATE_STOP\n");
		printf(" 4. ASM_STATE_PAUSE\n");
		printf(" q. Back to main menu\n");
		printf("===================================\n");
		printf(">> ");
		while( (input = getchar())!= '\n' && input != EOF) {
			key = (char)input;
		}

		switch (key) {
			case '1':
				asm_state = ASM_STATE_NONE;
				break;
			case '2':
				asm_state = ASM_STATE_PLAYING;
				break;
			case '3':
				asm_state = ASM_STATE_STOP;
				break;
			case '4':
				asm_state = ASM_STATE_PAUSE;
				break;
			case 'q':
				return;
			default :
				g_print("select ASM state again...\n");
				asm_state = 9;
				break;
		}
		if (asm_state == 9) {
			continue;
		}
		/* set ASM sound state */
		if( ! ASM_unset_watch_session( event_type, asm_state, &ret) ) {
			g_print("ASM_unset_watch_session() failed, 0x%X\n\n", ret);
			break;
		} else {
			g_print("ASM_unset_watch_session() success");
			break;
		}
	}
}

void menu_unset_watch(void)
{
	char key = 0;
	int input = 0;
	gint pid = -1;
	gint event_type = 0;
	g_resource = ASM_RESOURCE_NONE;

	while(1) {
		printf("======select ASM event to be un-watched======\n");
		printf(" 0. ASM_EVENT_MEDIA_MMPLAYER \n");
		printf(" 1. ASM_EVENT_MEDIA_MMPLAYER \n");
		printf(" 2. ASM_EVENT_MEDIA_MMSOUND\n");
		printf(" 3. ASM_EVENT_MEDIA_OPENAL\n");
		printf(" 4. ASM_EVENT_MEDIA_FMRADIO\n");
		printf(" 5. ASM_EVENT_MEDIA_WEBKIT\n");
		printf(" 6. ASM_EVENT_NOTIFY\n");
		printf(" 7. ASM_EVENT_ALARM\n");
		printf(" 8. ASM_EVENT_EARJACK_UNPLUG\n");
		printf(" 9. ASM_EVENT_CALL\n");
		printf(" a. ASM_EVENT_VIDEOCALL\n");
		printf(" b. ASM_EVENT_VOIP\n");
		printf(" c. ASM_EVENT_MONITOR\n");
		printf(" d. ASM_EVENT_EMERGENCY\n");
		printf(" q. Back to main menu\n");
		printf("==========================================\n");
		printf(">> ");


		while( (input = getchar())!= '\n' && input != EOF) {
			key = (char)input;
		}
		switch (key) {
		case '0':
			event_type = ASM_EVENT_MEDIA_MMPLAYER;
			break;
		case '1':
			event_type = ASM_EVENT_MEDIA_MMCAMCORDER;
			break;
		case '2':
			event_type = ASM_EVENT_MEDIA_MMSOUND;
			break;
		case '3':
			event_type = ASM_EVENT_MEDIA_OPENAL;
			break;
		case '4':
			event_type = ASM_EVENT_MEDIA_FMRADIO;
			break;
		case '5':
			event_type = ASM_EVENT_MEDIA_WEBKIT;
			break;
		case '6':
			event_type = ASM_EVENT_NOTIFY;
			break;
		case '7':
			event_type = ASM_EVENT_ALARM;
			break;
		case '8':
			event_type = ASM_EVENT_EARJACK_UNPLUG;
			break;
		case '9':
			event_type = ASM_EVENT_CALL;
			break;
		case 'a':
			event_type = ASM_EVENT_VIDEOCALL;
			break;
		case 'b':
			event_type = ASM_EVENT_VOIP;
			break;
		case 'c':
			event_type = ASM_EVENT_MONITOR;
			break;
		case 'd':
			event_type = ASM_EVENT_EMERGENCY;
			break;
		case 'q':
			return;
		default :
			g_print("select event again...\n");
			event_type = -1;
			break;
		}
		if (event_type == -1) {
			continue;
		}

		menu_unset_watch_state(pid, event_type);
		break;
	}
}

gpointer keythread(gpointer data)
{
	int input = 0;
	char key = 0;

	while (thread_run) {
		print_menu_main();
		while( (input = getchar())!= '\n' && input != EOF) {
			key = (char)input;
		}

		switch (key) {
			case 'a':
				menu_register();
				break;
			case 'o':
				menu_set_options();
				break;
			case 'r':
				menu_set_resource();
				break;
			case 's':
				menu_set_state();
				break;
			case 'x':
			{
				int ret = 0;
				ASM_sound_states_t asm_state = ASM_STATE_NONE;
				if( ! ASM_get_sound_state( g_asm_handle, g_event_type, &asm_state, &ret) ) {
					g_print("ASM_get_sound_state() failed, asm_handle[%d], asm_event[%d], 0x%X\n\n", g_asm_handle, g_event_type, ret);
				} else {
					g_print("ASM_get_sound_state() success, ASM handle=%d, ASM_EVENT=%d, ASM_STATE=%d\n\n", g_asm_handle, g_event_type, asm_state);
				}
				break;
			}
			case 'd':
				menu_set_sub_event();
				break;
			case 'c':
			{
				int ret = 0;
				ASM_sound_sub_events_t asm_sub_event = ASM_SUB_EVENT_NONE;
				if ( ! ASM_get_subevent (g_asm_handle, &asm_sub_event, &ret) ) {
					g_print("ASM_get_subevent() failed, asm_handle[%d], 0x%X\n\n", g_asm_handle, ret);
				} else {
					g_print("ASM_get_sound_state() success, ASM handle=%d, ASM_SUB_EVENT=%d\n\n", g_asm_handle, asm_sub_event);
				}
				break;
			}
			case 'f':
				menu_set_sub_session();
				break;
			case 'v':
			{
				int ret = 0;
				ASM_sound_sub_sessions_t asm_sub_session = ASM_SUB_SESSION_TYPE_VOICE;
				if ( ! ASM_get_subsession (g_asm_handle, &asm_sub_session, &ret)) {
					g_print("ASM_get_subsession() failed, asm_handle[%d], 0x%X\n\n", g_asm_handle, ret);
				} else {
					g_print("ASM_get_subsession() success, ASM handle=%d, ASM_SUB_SESSION=%d\n\n", g_asm_handle, asm_sub_session);
				}
				break;
			}
			case 'z':
				menu_unregister();
				break;
			case 'j':
				menu_set_watch();
				break;
			case 'm':
				menu_unset_watch();
				break;
			case 'w':
				if (g_asm_handle == -1) {
					g_print("Register ASM handle first...\n");
				} else {
					int error = 0;
					if (!ASM_reset_resumption_info(g_asm_handle, &error)) {
						g_print("ASM_reset_resumption_info() failed 0x%X\n", error);
					}
				}
				break;
			case 'q':
				if(g_asm_handle != -1) {
					menu_unregister();
				}
				g_main_loop_quit(g_loop);
				break;
			default :
				g_print("wrong input, select again...\n\n");
				break;
		} /* switch (key) */
	} /* while () */
	return NULL;
}

int main ()
{
	g_thread_init (NULL);
	thread_run = TRUE;

	g_loop = g_main_loop_new (NULL, 0);
	GThread * command_thread = g_thread_create (keythread, NULL, FALSE, NULL);
	if (!command_thread) {
		g_print ("key thread creation failure\n");
		return 0;
	}

	g_main_loop_run (g_loop);
	g_print ("loop finished !!\n");
	thread_run = FALSE;
	if (command_thread) {
		g_thread_join (command_thread);
		command_thread = NULL;
	}

	return 0;
}
