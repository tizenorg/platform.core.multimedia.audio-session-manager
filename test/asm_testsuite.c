#include <stdio.h>
#include <glib.h>
#include <stdlib.h>

#include <audio-session-manager.h>


GMainLoop *g_loop;
GThreadPool* g_pool;
gint asm_handle = -1;
gint event_type = ASM_EVENT_NONE;
gint asm_state = ASM_STATE_NONE;
ASM_resource_t g_resource = ASM_RESOURCE_NONE;
gboolean thread_run;

ASM_cb_result_t
asm_callback (int handle, ASM_event_sources_t event_src, ASM_sound_commands_t command, unsigned int sound_status, void* cb_data)
{
	g_print ("\n[%s][%d] handle = %d, event src = %d, command = %d, sound_state = %d\n\n", __func__, __LINE__, handle ,event_src, command, sound_status);
	return ASM_CB_RES_IGNORE;
}

void print_menu_main(void)
{
	printf("========ASM Testsuite======\n");
	printf(" r. Register ASM\n");
	printf(" s. Set state\n");
	printf(" u. Unregister ASM\n");
	printf(" q. Quit\n");
	printf("============================\n");
	printf(">> ");
}

void menu_unregister(void)
{
	gint errorcode = 0;
	if(asm_handle == -1) {
		g_print("Register sound first..\n\n");
		return;
	}
	if( !ASM_unregister_sound(asm_handle, event_type, &errorcode) ) {
		g_print("Unregister sound failed 0x%X\n\n", errorcode);
	} else {
		g_print("Unregister success..\n\n");
		asm_handle = -1;
	}
}

void menu_register(void)
{
	char key = 0;
	int input = 0;
	gint errorcode = 0;
	gint pid = -1;
	g_resource = ASM_RESOURCE_NONE;

	while(1) {
		printf("==========select ASM event=============\n");
		printf(" 0. ASM_EVENT_SHARE_MMPLAYER \n");
		printf(" 1. ASM_EVENT_SHARE_MMCAMCORDER\n");
		printf(" 2. ASM_EVENT_SHARE_MMSOUND\n");
		printf(" 3. ASM_EVENT_SHARE_OPENAL\n");
		printf(" 4. ASM_EVENT_SHARE_AVSYSTEM\n");
		printf(" 5. ASM_EVENT_SHARE_FMRADIO\n");
		printf(" 6. ASM_EVENT_EXCLUSIVE_MMPLAYER\n");
		printf(" 7. ASM_EVENT_EXCLUSIVE_MMCAMCORDER\n");
		printf(" 8. ASM_EVENT_EXCLUSIVE_MMSOUND\n");
		printf(" 9. ASM_EVENT_EXCLUSIVE_OPENAL\n");
		printf(" a. ASM_EVENT_EXCLUSIVE_AVSYSTEM\n");
		printf(" b. ASM_EVENT_EXCLUSIVE_FMRADIO\n");
		printf(" c. ASM_EVENT_NOTIFY\n");
		printf(" d. ASM_EVENT_CALL\n");
		printf(" e. ASM_EVENT_EARJACK_UNPLUG\n");
		printf(" f. ASM_EVENT_ALARM\n");
		printf(" g. ASM_EVENT_VIDEOCALL\n");
		printf(" h. ASM_EVENT_MONITOR\n");
		printf(" i. ASM_EVENT_RICHCALL\n");
		printf(" j. ASM_EVENT_EMERGENCY\n");
		printf(" k. ASM_EVENT_EXCLUSIVE_RESOURCE\n");
		printf(" q. Back to main menu\n");
		printf("=======================================\n");
		printf(">> ");


		while( (input = getchar())!= '\n' && input != EOF) {
			key = (char)input;
		}
		switch (key) {
			case '0':
				event_type = ASM_EVENT_SHARE_MMPLAYER;
				break;
			case '1':
				event_type = ASM_EVENT_SHARE_MMCAMCORDER;
				g_resource = ASM_RESOURCE_CAMERA;
				break;
			case '2':
				event_type = ASM_EVENT_SHARE_MMSOUND;
				break;
			case '3':
				event_type = ASM_EVENT_SHARE_OPENAL;
				break;
			case '4':
				event_type = ASM_EVENT_SHARE_AVSYSTEM;
				break;
			case '5':
				event_type = ASM_EVENT_SHARE_FMRADIO;
				break;
			case '6':
				event_type = ASM_EVENT_EXCLUSIVE_MMPLAYER;
				break;
			case '7':
				event_type = ASM_EVENT_EXCLUSIVE_MMCAMCORDER;
				g_resource = ASM_RESOURCE_CAMERA;
				break;
			case '8':
				event_type = ASM_EVENT_EXCLUSIVE_MMSOUND;
				break;
			case '9':
				event_type = ASM_EVENT_EXCLUSIVE_OPENAL;
				break;
			case 'a':
				event_type = ASM_EVENT_EXCLUSIVE_AVSYSTEM;
				break;
			case 'b':
				event_type = ASM_EVENT_EXCLUSIVE_FMRADIO;
				break;
			case 'c':
				event_type = ASM_EVENT_NOTIFY;
				break;
			case 'd':
				event_type = ASM_EVENT_CALL;
				break;
			case 'e':
				event_type = ASM_EVENT_EARJACK_UNPLUG;
				break;
			case 'f':
				event_type = ASM_EVENT_ALARM;
				break;
			case 'g':
				event_type = ASM_EVENT_VIDEOCALL;
				g_resource = ASM_RESOURCE_CAMERA;
				break;
			case 'h':
				event_type = ASM_EVENT_MONITOR;
				break;
			case 'i':
				event_type = ASM_EVENT_RICH_CALL;
				g_resource = ASM_RESOURCE_CAMERA;
				break;
			case 'j':
				event_type = ASM_EVENT_EMERGENCY;
				break;
			case 'k':
				event_type = ASM_EVENT_EXCLUSIVE_RESOURCE;
				/* temporarily set it ASM_RESOURCE_CAMERA */
				g_resource = ASM_RESOURCE_CAMERA;
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
		if( ! ASM_register_sound(pid, &asm_handle, event_type, ASM_STATE_NONE, asm_callback, NULL, g_resource, &errorcode)) {
			g_print("ASM_register_sound() failed, error = %x\n\n", errorcode);
			break;
		} else {
			g_print("ASM_register_sound() success, ASM handle=%d, ASM_EVENT=%d, ASM_RESOURCE=%d, ASM_STATE_NONE.\n\n", asm_handle, event_type, g_resource);
			break;
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
		printf(" 0. ASM_STATE_IGNORE\n");
		printf(" 1. ASM_STATE_NONE\n");
		printf(" 2. ASM_STATE_PLAYING\n");
		printf(" 3. ASM_STATE_WAITING\n");
		printf(" 4. ASM_STATE_STOP\n");
		printf(" 5. ASM_STATE_PAUSE\n");
		printf(" 6. ASM_STATE_PAUSE_BY_APP\n");
		printf(" q. Back to main menu\n");
		printf("=============================\n");
		printf(">> ");
		while( (input = getchar())!= '\n' && input != EOF) {
			key = (char)input;
		}

		switch (key) {
			case '0':
				asm_state = ASM_STATE_IGNORE;
				break;
			case '1':
				asm_state = ASM_STATE_NONE;
				break;
			case '2':
				asm_state = ASM_STATE_PLAYING;
				break;
			case '3':
				asm_state = ASM_STATE_WAITING;
				break;
			case '4':
				asm_state = ASM_STATE_STOP;
				break;
			case '5':
				asm_state = ASM_STATE_PAUSE;
				break;
			case '6':
				asm_state = ASM_STATE_PAUSE_BY_APP;
				break;
			case 'q':
				return;
			default :
				g_print("select ASM state again...\n");
				asm_state = 9;
		}
		if (asm_state == 9) {
			continue;
		}
		/* set ASM sound state */
		if( ! ASM_set_sound_state( asm_handle, event_type, asm_state, g_resource, &ret) ) {
			g_print("ASM_set_sound_state() failed, Set state to [%d] failed 0x%X\n\n", asm_state, ret);
			break;
		} else {
			g_print("ASM_set_sound_state() success, ASM handle=%d, ASM_EVENT=%d, ASM_RESOURCE=%d, ASM_STATE=%d\n\n", asm_handle, event_type, g_resource, asm_state);
			break;
		}
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
			case 'r':
				menu_register();
				break;
			case 's':
				menu_set_state();
				break;
			case 'u':
				menu_unregister();
				break;
			case 'q':
				if(asm_handle != -1) {
					menu_unregister();
				}
				g_main_loop_quit(g_loop);
				break;
			default :
				g_print("wrong input, select again...\n\n");
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
