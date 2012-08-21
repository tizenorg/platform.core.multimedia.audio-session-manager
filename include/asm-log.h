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

#ifndef __ASM_LOG_H__
#define __ASM_LOG_H__

#ifdef __DEBUG_MODE__
#include <stdio.h>
#ifdef __USE_LOGMANAGER__
#include <mm_log.h>
#endif
#endif

#ifdef __DEBUG_MODE__

#ifdef __USE_LOGMANAGER__
#define asm_info_r(msg, args...) log_print_rel((LOG_SESSIONMGR), (LOG_CLASS_INFO), (msg), ##args)
#define asm_warning_r(msg, args...) log_print_rel((LOG_SESSIONMGR), (LOG_CLASS_WARNING), (msg), ##args)
#define asm_error_r(msg, args...) log_print_rel((LOG_SESSIONMGR), (LOG_CLASS_ERR), (msg), ##args)
#define asm_critical_r(msg, args...) log_print_rel((LOG_SESSIONMGR), (LOG_CLASS_CRITICAL), (msg), ##args)
#define asm_assert_r(condition)  log_assert_rel((condition))

#define asm_info(msg, args...) log_print_dbg((LOG_SESSIONMGR), LOG_CLASS_INFO, (msg), ##args)
#define asm_warning(msg, args...) log_print_dbg((LOG_SESSIONMGR), LOG_CLASS_WARNING, (msg), ##args)
#define asm_error(msg, args...) log_print_dbg((LOG_SESSIONMGR), LOG_CLASS_ERR, (msg), ##args)
#define asm_critical(msg, args...) log_print_dbg((LOG_SESSIONMGR), LOG_CLASS_CRITICAL, (msg), ##args)
#define asm_assert(condition)  log_assert_dbg((condition))

#else	//__USE_LOGMANAGER__

#define asm_info_r(msg, args...) fprintf(stderr, msg, ##args)
#define asm_warning_r(msg, args...) fprintf(stderr, msg, ##args)
#define asm_error_r(msg, args...) fprintf(stderr, msg, ##args)
#define asm_critical_r(msg, args...) fprintf(stderr, msg, ##args)
#define asm_assert_r(condition)		(condition)

#define asm_info(msg, args...) fprintf(stderr, msg, ##args)
#define asm_warning(msg, args...) fprintf(stderr, msg, ##args)
#define asm_error(msg, args...) fprintf(stderr, msg, ##args)
#define asm_critical(msg, args...) fprintf(stderr, msg, ##args)
#define asm_assert(condition)			(condition)

#endif	//__USE_LOGMANAGER__

#else	//__DEBUG_MODE__

#define asm_info_r(msg, args...)
#define asm_warning_r(msg, args...)
#define asm_error_r(msg, args...)
#define asm_critical_r(msg, args...)
#define asm_assert_r(condition)	(condition)

#define asm_info(msg, args...)
#define asm_warning(msg, args...)
#define asm_error(msg, args...)
#define asm_critical(msg, args...)
#define asm_assert(condition)		(condition)

#endif  // __DEBUG_MODE__

#endif	/* __ASM_LOG_H__ */
