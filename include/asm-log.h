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
#include <dlog.h>

#define LOG_TAG	"MMFW_SESSIONMGR"
#define asm_info(fmt, arg...) SLOG(LOG_VERBOSE, LOG_TAG, "<DEBUG> [%s:%d] "fmt"", __FUNCTION__,__LINE__,##arg)
#define asm_warning(fmt, arg...) SLOG(LOG_WARN, LOG_TAG, "<WARNI> [%s:%d] "fmt"", __FUNCTION__,__LINE__,##arg)
#define asm_error(fmt, arg...) SLOG(LOG_ERROR, LOG_TAG, "<ERROR> [%s:%d] "fmt"", __FUNCTION__,__LINE__,##arg)
#define asm_critical(fmt, arg...) SLOG(LOG_ERROR, LOG_TAG, "<FATAL> [%s:%d] "fmt"", __FUNCTION__,__LINE__,##arg)
#define asm_fenter()	SLOG(LOG_VERBOSE, LOG_TAG, "<ENTER> [%s:%d]", __FUNCTION__,__LINE__)
#define asm_fleave()	SLOG(LOG_VERBOSE, LOG_TAG, "<LEAVE> [%s:%d]", __FUNCTION__,__LINE__)

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
#define asm_fenter()
#define asm_fleave()

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
#define asm_fenter()
#define asm_fleave()

#endif  // __DEBUG_MODE__

#endif	/* __ASM_LOG_H__ */
