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

#ifndef __ASM_ERROR_H__
#define __ASM_ERROR_H__

#ifdef __cplusplus
	extern "C" {
#endif

/**
  * ASM CLASS
  */
#define ASM_STATE_SUCCESS                 (0x00000000)                    /**< No Error */
#define ASM_STATE_ERROR                   (0x80000000)                    /**< Error Class */
#define ASM_STATE_WARING                  (0x70000000)                    /**< Waring Class */

/*
 * Detail enumeration
 */
enum {
    ASM_IN_UNKNOWN = 0,
    ASM_IN_PARAMETER,
    ASM_IN_HANDLE,
    ASM_IN_MEMORY,
};

/*
 * ASM_WARING
 */
#define ASM_STATE_WAR_INVALID_PARAMETER   (ASM_STATE_WARING | ASM_IN_PARAMETER)
#define ASM_STATE_WAR_INVALID_HANDLE      (ASM_STATE_WARING | ASM_IN_HANDLE)
#define ASM_STATE_WAR_INVALID_MEMORY       (ASM_STATE_WARING | ASM_IN_MEMORY)


/**
 *  ASM_ERROR
 */
#define ASM_STATE_ERR_INVALID_PARAMETER   (ASM_STATE_ERROR | ASM_IN_PARAMETER)
#define ASM_STATE_ERR_INVALID_HANDLE      (ASM_STATE_ERROR | ASM_IN_HANDLE)
#define ASM_STATE_ERR_INVALID_MEMORY       (ASM_STATE_ERROR | ASM_IN_MEMORY)


#define ASM_FAIL(_A_)                     (ASM_STATE_ERROR & (_A_))
#define ASM_SUCCESS(_A_)                  (!ASM_FAIL(_A_))
#define ASM_WARING(_A_)                   (ASM_STATE_WARING & (_A_))
#define ASM_ERROR(_A_)                    (ASM_STATE_ERROR & (_A_))

#ifdef __cplusplus
	}
#endif

#endif	/* __ASM_ERROR_H__ */
