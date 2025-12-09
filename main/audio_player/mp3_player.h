/*
 * MP3 Player header file for MAX98375A
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef MP3_PLAYER_MAX98375A_H
#define MP3_PLAYER_MAX98375A_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "board.h"

// 音乐文件路径定义
#define MUSIC_FILE_1 "/spiffs/song1.mp3"
#define MUSIC_FILE_2 "/spiffs/song2.mp3"

/**
 * @brief MP3 Player 结构体定义，使用面向对象思想封装
 */
typedef struct {
    // 任务相关
    TaskHandle_t task_handle;           /*!< MP3播放器任务句柄 */
    
    // 音频管道相关
    audio_pipeline_handle_t pipeline;  /*!< 音频管道句柄 */
    audio_element_handle_t mp3_decoder;/*!< MP3解码器句柄 */
    audio_element_handle_t i2s_writer;/*!< I2S流写入器句柄 */
    audio_event_iface_handle_t evt;    /*!< 音频事件接口句柄 */
    
    // 音频板相关
    audio_board_handle_t board_handle;/*!< 音频板句柄 */
    
    // 状态相关
    bool is_playing;                  /*!< 播放状态标志 */
    int current_song_idx;             /*!< 当前歌曲索引，0:第一首, 1:第二首 */
    int volume;                       /*!< 音量值(0-100) */
    
    // 文件相关
    FILE *current_file;               /*!< 当前打开的音乐文件句柄 */
    size_t current_file_size;         /*!< 当前文件大小 */
    size_t current_file_pos;          /*!< 当前文件读取位置 */
    
    // 资源初始化标志
    bool audio_board_initialized;     /*!< 音频板初始化标志 */
    bool spiffs_initialized;          /*!< SPIFFS初始化标志 */
    bool pipeline_initialized;        /*!< 音频管道初始化标志 */
    bool mp3_decoder_initialized;     /*!< MP3解码器初始化标志 */
    bool i2s_stream_initialized;      /*!< I2S流初始化标志 */
    bool evt_initialized;             /*!< 事件接口初始化标志 */
} MP3Player;

/**
 * @brief 初始化MP3播放器
 * 
 * @return MP3Player* 初始化成功返回MP3Player指针，失败返回NULL
 */
MP3Player* mp3_player_init();

/**
 * @brief 销毁MP3播放器
 * 
 * @param player MP3Player指针
 */
void mp3_player_deinit(MP3Player* player);

/**
 * @brief 播放或暂停MP3播放
 * 
 * @param player MP3Player指针
 */
void mp3_player_play_pause(MP3Player* player);

/**
 * @brief 停止MP3播放
 * 
 * @param player MP3Player指针
 */
void mp3_player_stop_playback(MP3Player* player);

/**
 * @brief 切换到下一首歌曲
 * 
 * @param player MP3Player指针
 */
void mp3_player_next_song(MP3Player* player);

/**
 * @brief 切换到上一首歌曲
 * 
 * @param player MP3Player指针
 */
void mp3_player_prev_song(MP3Player* player);

/**
 * @brief 增加音量
 * 
 * @param player MP3Player指针
 */
void mp3_player_volume_up(MP3Player* player);

/**
 * @brief 减少音量
 * 
 * @param player MP3Player指针
 */
void mp3_player_volume_down(MP3Player* player);

/**
 * @brief 检查MP3播放器是否在播放
 * 
 * @param player MP3Player指针
 * @return bool 播放状态，true为播放中，false为暂停或停止
 */
bool mp3_player_is_playing(MP3Player* player);

/**
 * @brief 获取当前歌曲索引
 * 
 * @param player MP3Player指针
 * @return int 当前歌曲索引
 */
int mp3_player_get_current_song(MP3Player* player);

/**
 * @brief 获取当前音量
 * 
 * @param player MP3Player指针
 * @return int 当前音量值(0-100)
 */
int mp3_player_get_volume(MP3Player* player);

#endif /* MP3_PLAYER_MAX98375A_H */
