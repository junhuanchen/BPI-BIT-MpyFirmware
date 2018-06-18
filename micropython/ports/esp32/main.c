/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"    
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_task.h"
#include "soc/cpu.h"
#include "esp_log.h"

#include "py/stackctrl.h"
#include "py/nlr.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "lib/mp-readline/readline.h"
#include "lib/utils/pyexec.h"
#include "uart.h"
#include "modmachine.h"
#include "modnetwork.h"
#include "mpthreadport.h"

#include "lib/oofatfs/ff.h"
#include "extmod/vfs_fat.h"

int ff_cre_syncobj(FATFS *fatfs, _SYNC_t* sobj)
{
    vSemaphoreCreateBinary((*sobj));
    return (int)((*sobj) != NULL);
}

int ff_del_syncobj(_SYNC_t sobj)
{
    vSemaphoreDelete(sobj);
    return 1;
}

int ff_req_grant(_SYNC_t sobj)
{
    return (int)(xSemaphoreTake(sobj, 10000) == pdTRUE);
}

void ff_rel_grant(_SYNC_t sobj)
{
    xSemaphoreGive(sobj);
}

STATIC FATFS *lookup_path(const TCHAR **path) 
{
    mp_vfs_mount_t *fs = mp_vfs_lookup_path(*path, path);
    printf("lookup_path %s fs %.*s\n", *path, fs->len, fs->str);
    if (fs == MP_VFS_NONE || fs == MP_VFS_ROOT) 
    {
        return NULL;
    }
    return &((fs_user_mount_t*)MP_OBJ_TO_PTR(fs->obj))->fatfs;
}

static void test_fat()
{
	const char * path = "/flashbdev";
	FATFS *vfs = lookup_path(&path);
	FRESULT res;
	if(NULL != (vfs))
	{
		FF_DIR dir;
		res = f_opendir(vfs, &dir, ".");
		printf("f_opendir %d\n", res);
		if (FR_OK == res)
		{
			for (;;) {
				FILINFO fno;
				FRESULT res = f_readdir(&dir, &fno);
				printf("f_readdir %d\n", res);
				char *fn = fno.fname;
				if (res != FR_OK || fn[0] == 0) 
				{
					// stop on error or end of dir
					break;
				}
				else
				{
					printf("fn %.*s\n", strlen(fn), fn);
					if (fno.fattrib & AM_DIR) 
					{
						// dir
						printf("dir\n");
					} else 
					{
						// file
						printf("file\n");
					}
					printf("size %d \n", fno.fsize);
				}
			}
			
			FIL fp = { 0 };
			
			res = f_open(vfs, &fp, "./main.py", FA_WRITE | FA_CREATE_NEW);
			printf("write f_open res %d\n", res);
			if (FR_OK == res) 
			{
				f_close(&fp);
			}
			
			res = f_open(vfs, &fp, "./main.py", FA_READ);
			printf("read vfs %p f_open res %d\n", vfs, res);
			if (FR_OK == res) 
			{
				printf("obj %p flag %hhd err %hhd fptr %d clust %d sect %d\n", \
					&fp.obj, fp.flag, fp.err, fp.fptr, fp.clust, fp.sect);
					
				printf("f_read p %p\n", f_read);
				UINT n = 0;
				char buf[128];
				res = f_read(&fp, (void *)buf, 128, &n);
				printf("f_read res %d\n", res);
				if (res == FR_OK) 
				{
					ets_printf("f_read n %d buf %s\n", n, buf);
				}
				f_close(&fp);
			}
			f_closedir(&dir);
		}
	}
}

bool MicroPythonCheckFile()
{
    const char *path = "/flashbdev";
    FATFS *vfs = lookup_path(&path);
    if(NULL != (vfs))
    {
        FIL fp = { 0 };
        FRESULT res = f_open(vfs, &fp, "./system.py", FA_READ | FA_WRITE | FA_CREATE_NEW);
        if (FR_OK == res)
        {
            #define MpcfDefaultText "# This File Will Loop execute.\n"
            UINT written = 0;
            res = f_write(&fp, (void *)MpcfDefaultText, sizeof(MpcfDefaultText) - 1, &written);
            if (FR_OK == res && written == (sizeof(MpcfDefaultText) - 1))
            {
                f_close(&fp);
                return true;
            }
        }
    }
    return false;
}

#include "user_mongoose.h"
extern FATFS *mongoose_vfs;

#include "user_smartconfig.h"

#define SAFE_KEY 27 // 34

// MicroPython runs as a task under FreeRTOS
#define MP_TASK_PRIORITY        (ESP_TASK_PRIO_MIN + 1)
#define MP_TASK_STACK_SIZE      (16 * 1024)
#define MP_TASK_STACK_LEN       (MP_TASK_STACK_SIZE / sizeof(StackType_t))

STATIC StaticTask_t mp_task_tcb;
STATIC StackType_t mp_task_stack[MP_TASK_STACK_LEN] __attribute__((aligned (8)));

int vprintf_null(const char *format, va_list ap) {
    // do nothing: this is used as a log target during raw repl mode
    return 0;
}

void mp_task(void *pvParameter) {
    volatile uint32_t sp = (uint32_t)get_sp();
    #if MICROPY_PY_THREAD
    mp_thread_init(&mp_task_stack[0], MP_TASK_STACK_LEN);
    #endif
    uart_init();

    // Allocate the uPy heap using malloc and get the largest available region
    size_t mp_task_heap_size = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    void *mp_task_heap = malloc(mp_task_heap_size);

soft_reset:
    // initialise the stack pointer for the main thread
    mp_stack_set_top((void *)sp);
    mp_stack_set_limit(MP_TASK_STACK_SIZE - 1024);
    gc_init(mp_task_heap, mp_task_heap + mp_task_heap_size);
    mp_init();
    mp_obj_list_init(mp_sys_path, 0);
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR_));
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_lib));
    mp_obj_list_init(mp_sys_argv, 0);
    readline_init0();

    // initialise peripherals
    machine_pins_init();

    // run boot-up scripts
    pyexec_frozen_module("_boot.py");
    pyexec_file("boot.py");
    // if (pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL) {
    //     pyexec_file("main.py");
    // }

    
    // test_fat();
    
    const char * path = "/flashbdev";
    do
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        mongoose_vfs = lookup_path(&path);
    } while(mongoose_vfs == NULL);
    
    // micro python check file 
    MicroPythonCheckFile();

    gpio_set_direction(SAFE_KEY, GPIO_MODE_INPUT);
    while (0 == gpio_get_level(SAFE_KEY))
    {
        mg_loop();
    }
    
    // test_fat();
    
    // size_t stack_size = 0;
    // mp_thread_create(entry_main, NULL, &stack_size);
    
    while(pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL) 
    {
        mg_loop();
        
        pyexec_file("system.py");
    }

    #if MICROPY_PY_THREAD
    mp_thread_deinit();
    #endif

    gc_sweep_all();

    mp_hal_stdout_tx_str("PYB: soft reboot\r\n");

    // deinitialise peripherals
    machine_pins_deinit();
    usocket_events_deinit();

    mp_deinit();
    fflush(stdout);
    goto soft_reset;
}

#include "soc/rtc.h"

void app_main(void) {
    
    nvs_flash_init();
    
    rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);
    
    config_default_wifi();
    
    mg_init();
    
    xTaskCreateStaticPinnedToCore(mp_task, "mp_task", MP_TASK_STACK_LEN, NULL, MP_TASK_PRIORITY, &mp_task_stack[0], &mp_task_tcb, tskNO_AFFINITY);
}

void nlr_jump_fail(void *val) {
    printf("NLR jump failed, val=%p\n", val);
    esp_restart();
}

// modussl_mbedtls uses this function but it's not enabled in ESP IDF
void mbedtls_debug_set_threshold(int threshold) {
    (void)threshold;
}
