/**
 * @file    zdemo_threads.h
 * @brief   Thread ids of the statically defined application threads.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     ZRT-001, ZRT-005
 */

#ifndef ZDEMO_THREADS_H
#define ZDEMO_THREADS_H

#include <zephyr/kernel.h>

/** Health thread (health.c). */
extern const k_tid_t zdemo_health_thread;
/** Comms thread (comms.c). */
extern const k_tid_t zdemo_comms_thread;
/** Button thread (button.c). */
extern const k_tid_t zdemo_button_thread;
/** LED thread (led.c). */
extern const k_tid_t zdemo_led_thread;

#endif /* ZDEMO_THREADS_H */
