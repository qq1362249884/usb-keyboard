/* Play mp3 file by audio pipeline
   with possibility to start, stop, pause and resume playback
   as well as adjust volume

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_error.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "board.h"
#include "esp_spiffs.h"
#include "mp3_player.h"

static const char *TAG = "MP3_PLAYER_MAX98375A";

// 单例实例，全局唯一
static MP3Player* s_mp3_player = NULL;
// 互斥锁，用于确保单例初始化的线程安全
static SemaphoreHandle_t s_mp3_player_mutex = NULL;

static void set_file_marker(MP3Player* player, int direction) // direction: 0=current, 1=next, -1=prev
{
    const char *file_path = NULL;
    const char *song_name = NULL;
    int new_idx = player->current_song_idx;

    // 根据方向计算新的索引，确保不会超出范围
    if (direction == 1) { // next
        new_idx = (player->current_song_idx < 1) ? player->current_song_idx + 1 : player->current_song_idx;
    } else if (direction == -1) { // prev
        new_idx = (player->current_song_idx > 0) ? player->current_song_idx - 1 : player->current_song_idx;
    } else { // current
        new_idx = player->current_song_idx;
    }

    // 如果索引没有变化，但文件已关闭，重新打开文件
    if (new_idx == player->current_song_idx && direction != 0) {
        ESP_LOGI(TAG, "[ * ] Already at %s, no change", direction == 1 ? "last song" : "first song");
        // 如果文件已关闭，重新打开文件
        if (player->current_file == NULL) {
            ESP_LOGI(TAG, "[ * ] File is closed, reopening current song");
        } else {
            return; // 文件已打开，不需要重新设置
        }
    }

    // 关闭当前打开的文件
    if (player->current_file != NULL) {
        fclose(player->current_file);
        player->current_file = NULL;
    }

    // 更新当前索引
    player->current_song_idx = new_idx;

    switch (player->current_song_idx) {
        case 0:
            file_path = MUSIC_FILE_1;
            song_name = "Song 1 - M500003c89uw1rfLwc.mp3";
            break;
        case 1:
            file_path = MUSIC_FILE_2;
            song_name = "Song 2 - music-16b-2c-44100hz.mp3";
            break;
        default:
            ESP_LOGE(TAG, "[ * ] Not supported index = %d", player->current_song_idx);
            return;
    }
    
    // 打开音乐文件
    player->current_file = fopen(file_path, "rb");
    if (player->current_file == NULL) {
        ESP_LOGE(TAG, "[ * ] Failed to open music file: %s", file_path);
        return;
    }
    
    // 获取文件大小
    fseek(player->current_file, 0, SEEK_END);
    player->current_file_size = ftell(player->current_file);
    fseek(player->current_file, 0, SEEK_SET);
    player->current_file_pos = 0;
    
    ESP_LOGI(TAG, "[ * ] Playing: %s, file size: %d bytes", song_name, player->current_file_size);
}

// 向后兼容，保持原有函数接口
static void set_next_file_marker(MP3Player* player)
{
    set_file_marker(player, 1);
}

int mp3_music_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    MP3Player* player = (MP3Player*)ctx;
    
    // 检查文件是否已打开
    if (player->current_file == NULL) {
        ESP_LOGI(TAG, "[ * ] No music file open, returning AEL_IO_DONE");
        return AEL_IO_DONE;
    }
    
    // 计算剩余可读数据
    size_t remaining_size = player->current_file_size - player->current_file_pos;
    
    // 检查是否已经读取完所有数据
    if (remaining_size <= 0) {
        ESP_LOGI(TAG, "[ * ] MP3 file fully read, returning AEL_IO_DONE");
        fclose(player->current_file);
        player->current_file = NULL;
        return AEL_IO_DONE; // 使用标准的文件结束标记
    }
    
    // 计算实际可以读取的大小，确保不会读取超出文件范围的数据
    size_t read_size = (len < remaining_size) ? len : remaining_size;
    
    // 从文件中读取数据
    size_t bytes_read = fread(buf, 1, read_size, player->current_file);
    if (bytes_read != read_size) {
        ESP_LOGW(TAG, "[ * ] Failed to read full data from file, expected: %d, got: %d", read_size, bytes_read);
        if (feof(player->current_file)) {
            ESP_LOGI(TAG, "[ * ] End of file reached");
            fclose(player->current_file);
            player->current_file = NULL;
            return AEL_IO_DONE;
        }
    }
    
    // 更新文件位置
    player->current_file_pos += bytes_read;
    
    // 记录读取进度
    static size_t last_log_pos = 0;
    if (player->current_file_pos - last_log_pos > 102400) {
        int progress = (player->current_file_pos * 100) / player->current_file_size;
        ESP_LOGI(TAG, "[ * ] MP3 read progress: %d%% (%d/%d bytes)", progress, player->current_file_pos, player->current_file_size);
        last_log_pos = player->current_file_pos;
    }
    
    return bytes_read;
}

static void mp3_player_task(void *arg)
{
    MP3Player* player = (MP3Player*)arg;
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // 初始化资源标志
    player->spiffs_initialized = false;
    player->pipeline_initialized = false;
    player->mp3_decoder_initialized = false;
    player->i2s_stream_initialized = false;
    player->evt_initialized = false;

    ESP_LOGI(TAG, "[ 1 ] Initialize SPIFFS file system for music resources");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "music",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        // 设置任务句柄为NULL，触发任务退出
        player->task_handle = NULL;
        // 退出任务
        vTaskDelete(NULL);
        return;
    }
    player->spiffs_initialized = true;
    
    // MAX98375A不需要音频解码芯片控制，音量控制通过硬件实现
    player->volume = 100; // 固定音量，MAX98375A不支持软件音量控制
    
    // 初始化音频板，确保只初始化一次
    if (!player->audio_board_initialized) {
        ESP_LOGI(TAG, "[ 2 ] Initialize MAX98375A audio board");
        player->board_handle = audio_board_init();
        if (!player->board_handle) {
            ESP_LOGE(TAG, "Failed to initialize audio board");
            // 设置任务句柄为NULL，触发任务退出
            player->task_handle = NULL;
            // 退出任务
            vTaskDelete(NULL);
            return;
        }
        // MAX98375A是I2S音频放大器，不需要编解码器控制
        player->audio_board_initialized = true;
    } else {
        ESP_LOGI(TAG, "[ 2 ] Audio board already initialized");
    }

    ESP_LOGI(TAG, "[ 3 ] Create audio pipeline, add all elements to pipeline, and subscribe pipeline event");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    player->pipeline = audio_pipeline_init(&pipeline_cfg);
    if (!player->pipeline) {
        ESP_LOGE(TAG, "Failed to create audio pipeline");
        // 设置任务句柄为NULL，触发任务退出
        player->task_handle = NULL;
        // 退出任务
        vTaskDelete(NULL);
        return;
    }
    player->pipeline_initialized = true;

    ESP_LOGI(TAG, "[3.1] Create mp3 decoder to decode mp3 file and set custom read callback");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    player->mp3_decoder = mp3_decoder_init(&mp3_cfg);
    if (!player->mp3_decoder) {
        ESP_LOGE(TAG, "Failed to create mp3 decoder");
        // 设置任务句柄为NULL，触发任务退出
        player->task_handle = NULL;
        // 退出任务
        vTaskDelete(NULL);
        return;
    }
    audio_element_set_read_cb(player->mp3_decoder, mp3_music_read_cb, player);
    player->mp3_decoder_initialized = true;

    ESP_LOGI(TAG, "[3.2] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    player->i2s_writer = i2s_stream_init(&i2s_cfg);
    if (!player->i2s_writer) {
        ESP_LOGE(TAG, "Failed to create i2s stream");
        // 设置任务句柄为NULL，触发任务退出
        player->task_handle = NULL;
        // 退出任务
        vTaskDelete(NULL);
        return;
    }
    player->i2s_stream_initialized = true;

    ESP_LOGI(TAG, "[3.3] Register all elements to audio pipeline");
    audio_pipeline_register(player->pipeline, player->mp3_decoder, "mp3");
    audio_pipeline_register(player->pipeline, player->i2s_writer, "i2s");

    ESP_LOGI(TAG, "[3.4] Link it together [mp3_music_read_cb]-->mp3_decoder-->i2s_stream-->[MAX98375A]");
    const char *link_tag[2] = {"mp3", "i2s"};
    audio_pipeline_link(player->pipeline, &link_tag[0], 2);

    ESP_LOGI(TAG, "[ 4 ] Set up event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    player->evt = audio_event_iface_init(&evt_cfg);
    if (!player->evt) {
        ESP_LOGE(TAG, "Failed to create event interface");
        // 设置任务句柄为NULL，触发任务退出
        player->task_handle = NULL;
        // 退出任务
        vTaskDelete(NULL);
        return;
    }
    player->evt_initialized = true;

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(player->pipeline, player->evt);

    
    // 设置初始状态为暂停，不自动播放
    set_next_file_marker(player);
    player->is_playing = false;
    

    while (1) {
        // 检查任务是否需要退出
        if (player->task_handle == NULL) {
            ESP_LOGI(TAG, "MP3 player task is being terminated, exiting loop");
            break;
        }
        
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(player->evt, &msg, 10 / portTICK_PERIOD_MS);
        if (ret != ESP_OK) {
            // 让出CPU时间，防止看门狗触发
            vTaskDelay(1 / portTICK_PERIOD_MS);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) player->mp3_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(player->mp3_decoder, &music_info);
            i2s_stream_set_clk(player->i2s_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) player->mp3_decoder
            && msg.cmd == AEL_MSG_CMD_FINISH) {
            ESP_LOGI(TAG, "[ * ] MP3 decoder finished, switching to next song");
            
            // 停止管道并等待完全停止
            audio_pipeline_stop(player->pipeline);
            audio_pipeline_wait_for_stop(player->pipeline);
            
            // 重置管道元素
            audio_pipeline_reset_elements(player->pipeline);
            audio_pipeline_reset_ringbuffer(player->pipeline);
            
            // 切换到下一个文件
            set_next_file_marker(player);
            
            // 只在当前是播放状态时才重新启动管道
            if (player->is_playing) {
                audio_element_state_t state = audio_element_get_state(player->mp3_decoder);
                if (state == AEL_STATE_RUNNING || state == AEL_STATE_PAUSED) {
                    audio_pipeline_run(player->pipeline);
                }
            }
            continue;
        }
        
        /* Stop when the last pipeline element (i2s_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) player->i2s_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            // 不退出循环，继续等待下一个播放命令
        }
    }
    
    // 退出任务
    vTaskDelete(NULL);
}

MP3Player* mp3_player_init()
{
    // 初始化互斥锁（如果尚未初始化）
    if (s_mp3_player_mutex == NULL) {
        s_mp3_player_mutex = xSemaphoreCreateMutex();
        if (s_mp3_player_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex for MP3 player");
            return NULL;
        }
    }
    
    // 线程安全地获取单例实例
    xSemaphoreTake(s_mp3_player_mutex, portMAX_DELAY);
    
    // 检查实例是否已存在，避免重复创建
    if (s_mp3_player != NULL) {
        ESP_LOGW(TAG, "MP3 player already initialized, returning existing instance");
        xSemaphoreGive(s_mp3_player_mutex);
        return s_mp3_player;
    }
    
    // 创建MP3Player实例
    MP3Player* player = (MP3Player*)malloc(sizeof(MP3Player));
    if (player == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for MP3Player");
        xSemaphoreGive(s_mp3_player_mutex);
        return NULL;
    }
    
    // 初始化实例
    memset(player, 0, sizeof(MP3Player));
    player->current_song_idx = 0;
    player->volume = 100;
    
    // 创建任务
    xTaskCreate(mp3_player_task, "mp3_player_task", 3*4096, player, 5, &player->task_handle);
    
    // 保存单例实例
    s_mp3_player = player;
    
    xSemaphoreGive(s_mp3_player_mutex);
    
    return player;
}

static void mp3_player_cleanup(MP3Player* player)
{
    if (player == NULL) {
        return;
    }
    
    // 清理资源，按照初始化顺序的相反顺序释放
    ESP_LOGI(TAG, "[ 5 ] Stop audio_pipeline and clean up resources in reverse order");
    
    // 1. 关闭当前打开的文件
    if (player->current_file != NULL) {
        fclose(player->current_file);
        player->current_file = NULL;
        player->current_file_size = 0;
        player->current_file_pos = 0;
    }
    
    // 2. 停止并清理音频管道
    if (player->pipeline_initialized && player->pipeline) {
        // 停止管道
        if (player->i2s_stream_initialized && player->i2s_writer) {
            audio_element_state_t state = audio_element_get_state(player->i2s_writer);
            if (state == AEL_STATE_RUNNING || state == AEL_STATE_PAUSED) {
                audio_pipeline_stop(player->pipeline);
                audio_pipeline_wait_for_stop(player->pipeline);
            }
        }
        
        // 移除监听器
        if (player->evt_initialized && player->evt) {
            audio_pipeline_remove_listener(player->pipeline);
        }
        
        // 释放管道资源
        audio_pipeline_terminate(player->pipeline);
        
        if (player->mp3_decoder_initialized) {
            audio_pipeline_unregister(player->pipeline, player->mp3_decoder);
        }
        if (player->i2s_stream_initialized) {
            audio_pipeline_unregister(player->pipeline, player->i2s_writer);
        }
        
        audio_pipeline_deinit(player->pipeline);
        player->pipeline = NULL;
        player->pipeline_initialized = false;
    }
    
    // 3. 释放事件接口
    if (player->evt_initialized && player->evt) {
        audio_event_iface_destroy(player->evt);
        player->evt = NULL;
        player->evt_initialized = false;
    }
    
    // 4. 释放音频元素
    if (player->mp3_decoder_initialized && player->mp3_decoder) {
        // 直接释放元素，不需要状态检查
        audio_element_deinit(player->mp3_decoder);
        player->mp3_decoder = NULL;
        player->mp3_decoder_initialized = false;
    }
    
    if (player->i2s_stream_initialized && player->i2s_writer) {
        // 直接释放元素，不需要状态检查
        audio_element_deinit(player->i2s_writer);
        player->i2s_writer = NULL;
        player->i2s_stream_initialized = false;
    }
    
    // 5. 注销SPIFFS文件系统
    if (player->spiffs_initialized) {
        esp_vfs_spiffs_unregister("music");
        player->spiffs_initialized = false;
    }
    
    // 6. 去初始化音频板
    if (player->audio_board_initialized && player->board_handle) {
        audio_board_deinit(player->board_handle);
        player->board_handle = NULL;
        player->audio_board_initialized = false;
    }
    
    // 7. 重置播放状态
    player->is_playing = false;
}

void mp3_player_deinit(MP3Player* player)
{
    if (player == NULL) {
        return;
    }
    
    TaskHandle_t task_to_delete = player->task_handle;
    
    if (task_to_delete != NULL) {
        ESP_LOGI(TAG, "Stopping MP3 player task");
        
        // 设置任务句柄为NULL，触发任务内部的退出逻辑
        player->task_handle = NULL;
        
        // 只有当调用者不是任务本身时，才等待任务退出
        if (xTaskGetCurrentTaskHandle() != task_to_delete) {
            // 等待任务自行清理并退出，超时时间为2秒
            vTaskDelay(pdMS_TO_TICKS(500)); // 给任务足够的时间清理
        }
        // 如果是任务本身调用，任务会在函数返回后自行删除
    }
    
    // 清理资源
    mp3_player_cleanup(player);
    
    // 线程安全地清除单例实例
    if (s_mp3_player_mutex != NULL) {
        xSemaphoreTake(s_mp3_player_mutex, portMAX_DELAY);
        
        if (s_mp3_player == player) {
            s_mp3_player = NULL;
        }
        xSemaphoreGive(s_mp3_player_mutex);
        
        // 删除互斥锁
        vSemaphoreDelete(s_mp3_player_mutex);
        s_mp3_player_mutex = NULL;
    }
    
    // 释放内存
    free(player);
}

void mp3_player_play_pause(MP3Player* player)
{
    if (player == NULL || !player->pipeline || !player->i2s_writer) {
        ESP_LOGW(TAG, "MP3 player not initialized");
        return;
    }
    
    // 确保有音乐文件打开
    if (player->current_file == NULL) {
        ESP_LOGI(TAG, "[ * ] No music file open, setting default file");
        set_file_marker(player, 0); // 使用当前歌曲或默认歌曲
    }
    
    audio_element_state_t el_state = audio_element_get_state(player->i2s_writer);
    switch (el_state) {
        case AEL_STATE_INIT :
            ESP_LOGI(TAG, "[ * ] Starting audio pipeline");
            audio_pipeline_run(player->pipeline);
            player->is_playing = true;
            break;
        case AEL_STATE_RUNNING :
            ESP_LOGI(TAG, "[ * ] Pausing audio pipeline");
            audio_pipeline_pause(player->pipeline);
            player->is_playing = false;
            break;
        case AEL_STATE_PAUSED :
            ESP_LOGI(TAG, "[ * ] Resuming audio pipeline");
            audio_pipeline_resume(player->pipeline);
            player->is_playing = true;
            break;
        case AEL_STATE_FINISHED :
            ESP_LOGI(TAG, "[ * ] Restarting audio pipeline");
            audio_pipeline_reset_ringbuffer(player->pipeline);
            audio_pipeline_reset_elements(player->pipeline);
            audio_pipeline_change_state(player->pipeline, AEL_STATE_INIT);
            // 确保有音乐文件打开
            if (player->current_file == NULL) {
                set_file_marker(player, 0);
            }
            audio_pipeline_run(player->pipeline);
            player->is_playing = true;
            break;
        default :
            ESP_LOGI(TAG, "[ * ] Not supported state %d", el_state);
    }
}

void mp3_player_stop_playback(MP3Player* player)
{
    if (player == NULL || !player->pipeline) {
        ESP_LOGW(TAG, "MP3 player not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "[ * ] Stopping audio playback");
    
    // 先检查管道状态，避免重复停止
    if (player->i2s_stream_initialized && player->i2s_writer) {
        audio_element_state_t state = audio_element_get_state(player->i2s_writer);
        if (state == AEL_STATE_RUNNING || state == AEL_STATE_PAUSED) {
            audio_pipeline_stop(player->pipeline);
            audio_pipeline_wait_for_stop(player->pipeline);
        }
    }
    
    // 只更新播放状态，不重置管道和元素
    // 管道和元素的清理工作由 mp3_player_cleanup 统一处理
    player->is_playing = false;
}

void mp3_player_next_song(MP3Player* player)
{
    if (player == NULL || !player->pipeline || !player->i2s_writer) {
        ESP_LOGW(TAG, "MP3 player not initialized");
        return;
    }
    
    // 获取当前播放状态
    bool was_playing = player->is_playing;
    
    ESP_LOGI(TAG, "[ * ] Switching to next song");
    
    // 关闭当前打开的文件（如果有）
    if (player->current_file != NULL) {
        fclose(player->current_file);
        player->current_file = NULL;
    }
    
    // 停止当前播放，无论是否在运行状态
    audio_element_state_t state = audio_element_get_state(player->i2s_writer);
    if (state == AEL_STATE_RUNNING || state == AEL_STATE_PAUSED) {
        audio_pipeline_stop(player->pipeline);
        audio_pipeline_wait_for_stop(player->pipeline);
    }
    
    // 重置管道
    audio_pipeline_reset_ringbuffer(player->pipeline);
    audio_pipeline_reset_elements(player->pipeline);
    
    // 切换到下一首歌曲
    set_file_marker(player, 1);
    
    // 如果之前在播放，继续播放；否则保持暂停
    if (was_playing) {
        // 开始播放
        ESP_LOGI(TAG, "[ * ] Starting audio pipeline after next song");
        // 检查管道状态，避免重复启动
        audio_element_state_t current_state = audio_element_get_state(player->i2s_writer);
        if (current_state != AEL_STATE_RUNNING && current_state != AEL_STATE_PAUSED) {
            audio_pipeline_run(player->pipeline);
        }
        player->is_playing = true;
    } else {
        player->is_playing = false;
    }
}

/**
 * @brief Switch to previous song
 * 
 * This function switches to the previous song in the playlist.
 */
void mp3_player_prev_song(MP3Player* player)
{
    if (player == NULL || !player->pipeline || !player->i2s_writer) {
        ESP_LOGW(TAG, "MP3 player not initialized");
        return;
    }
    
    // 获取当前播放状态
    bool was_playing = player->is_playing;
    
    ESP_LOGI(TAG, "[ * ] Switching to previous song");
    
    // 关闭当前打开的文件（如果有）
    if (player->current_file != NULL) {
        fclose(player->current_file);
        player->current_file = NULL;
    }
    
    // 停止当前播放，无论是否在运行状态
    audio_element_state_t state = audio_element_get_state(player->i2s_writer);
    if (state == AEL_STATE_RUNNING || state == AEL_STATE_PAUSED) {
        audio_pipeline_stop(player->pipeline);
        audio_pipeline_wait_for_stop(player->pipeline);
    }
    
    // 重置管道
    audio_pipeline_reset_ringbuffer(player->pipeline);
    audio_pipeline_reset_elements(player->pipeline);
    
    // 切换到上一首歌曲
    set_file_marker(player, -1);
    
    // 如果之前在播放，继续播放；否则保持暂停
    if (was_playing) {
        // 开始播放
        ESP_LOGI(TAG, "[ * ] Starting audio pipeline after previous song");
        // 检查管道状态，避免重复启动
        audio_element_state_t current_state = audio_element_get_state(player->i2s_writer);
        if (current_state != AEL_STATE_RUNNING && current_state != AEL_STATE_PAUSED) {
            audio_pipeline_run(player->pipeline);
        }
        player->is_playing = true;
    } else {
        player->is_playing = false;
    }
}

void mp3_player_volume_up(MP3Player* player)
{
    if (player == NULL) {
        ESP_LOGW(TAG, "MP3 player not initialized");
        return;
    }
    
    player->volume += 10;
    if (player->volume > 100) {
        player->volume = 100;
    }
    // MAX98375A不支持软件音量控制，仅显示提示信息
    ESP_LOGI(TAG, "[ * ] MAX98375A does not support software volume control");
    ESP_LOGI(TAG, "[ * ] Volume would be set to %d %% (hardware control required)", player->volume);
}

void mp3_player_volume_down(MP3Player* player)
{
    if (player == NULL) {
        ESP_LOGW(TAG, "MP3 player not initialized");
        return;
    }
    
    player->volume -= 10;
    if (player->volume < 0) {
        player->volume = 0;
    }
    // MAX98375A不支持软件音量控制，仅显示提示信息
    ESP_LOGI(TAG, "[ * ] MAX98375A does not support software volume control");
    ESP_LOGI(TAG, "[ * ] Volume would be set to %d %% (hardware control required)", player->volume);
}

bool mp3_player_is_playing(MP3Player* player)
{
    if (player == NULL) {
        return false;
    }
    return player->is_playing;
}

int mp3_player_get_current_song(MP3Player* player)
{
    if (player == NULL) {
        return -1;
    }
    return player->current_song_idx;
}

int mp3_player_get_volume(MP3Player* player)
{
    if (player == NULL) {
        return 0;
    }
    return player->volume;
}

