/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __AUDIO_TASK_H__
#define __AUDIO_TASK_H__

/**
 * @brief Start the audio processing task
 *        Reads from I2S microphone and feeds data to HFP AG
 */
void audio_task_start(void);

/**
 * @brief Stop the audio processing task
 */
void audio_task_stop(void);

#endif /* __AUDIO_TASK_H__ */
