#ifndef __OLED_MENU_H
#define __OLED_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "OLED.h"  // 使用OLED显示函数
#include "esp_log.h"
#include <freertos/queue.h>


// 菜单操作枚举
typedef enum {
    MENU_OP_UP,      // 上移
    MENU_OP_DOWN,    // 下移
    MENU_OP_ENTER,   // 进入子菜单
    MENU_OP_BACK,    // 返回上一级菜单
    MENU_OP_LEFT,    // 左移（用于图片移动）
    MENU_OP_RIGHT    // 右移（用于图片移动）
} MenuOperation;

// 菜单类型枚举，提高代码可读性
typedef enum {
    MENU_TYPE_TEXT = 0,
    MENU_TYPE_IMAGE = 1,
    MENU_TYPE_ACTION = 2
} MenuDefType;

// 前向声明
typedef struct MenuItem MenuItem;

// 菜单项选择回调函数类型
typedef void (*MenuAction)(void);

// 菜单结构定义
typedef struct {
    const char* name;          // 菜单项名称
    MenuDefType type;          // 类型: 0=普通菜单, 1=图像菜单, 2=动作菜单
    const uint8_t* image;      // 图像数据指针
    uint16_t imageWidth;       // 图像宽度
    uint16_t imageHeight;      // 图像高度
    MenuAction action;         // 菜单项动作函数
    int parentIndex;           // 父菜单索引 (-1表示根菜单)
} MenuItemDef;

// 菜单项类型枚举
typedef enum {
    MENU_ITEM_TEXT,    // 文本菜单项
    MENU_ITEM_IMAGE    // 图片菜单项
} MenuItemType;

// 图片菜单项移动模式枚举
typedef enum {
    IMAGE_MOVE_LEFT_RIGHT,  // 左右移动模式
    IMAGE_MOVE_UP_DOWN      // 上下移动模式
} ImageMoveMode;

// 菜单项结构体 - 链表节点
typedef struct MenuItem {
    MenuItemType type;       // 菜单项类型
    char* name;              // 菜单项名称
    MenuAction action;       // 选择此菜单项执行的操作，NULL表示进入子菜单
    struct MenuItem* parent; // 父菜单项指针
    struct MenuItem* child;  // 第一个子菜单项指针
    struct MenuItem* next;   // 下一个同级菜单项指针
    
    // 图片菜单项特有字段
    const uint8_t* imageData; // 图片数据
    uint16_t imageWidth;      // 图片宽度
    uint16_t imageHeight;     // 图片高度
    int8_t moveOffset;        // 图片移动偏移量
} MenuItem;

// 菜单状态结构体 - 用于保存每一级菜单的状态
typedef struct {
    uint8_t selectedIndex; // 选中项的索引
    uint8_t startRow;      // 显示起始行
} MenuState;

// 定义菜单栈的最大深度
#define MAX_MENU_DEPTH 8

// 菜单管理器结构体
typedef struct {
    MenuItem* rootMenu;      // 根菜单指针
    MenuItem* currentMenu;   // 当前菜单指针
    MenuItem* selectedItem;  // 当前选中的菜单项
    uint8_t visibleRows;     // 屏幕可见行数
    uint8_t startRow;        // 当前显示的起始行
    ImageMoveMode moveMode;  // 当前层级的图片移动模式
    bool isEvenVisibleImages; // 可见图像数量的奇偶性（1表示偶数，0表示奇数）
    bool blockKeyEvents;      // 是否阻塞按键事件处理
    bool startRowInitialized; // startRow是否已初始化（用于确保特定代码只运行一次）
    
    // 菜单状态栈 - 用于多级菜单导航时保存和恢复状态
    MenuState stateStack[MAX_MENU_DEPTH]; // 状态栈数组
    uint8_t stackDepth;                    // 当前栈深度
} MenuManager;

extern MenuItemDef menuItems[];
extern const uint8_t MENU_ITEM_COUNT;

// 菜单初始化函数
void MenuManager_Init(MenuManager* manager);

// 菜单树构建函数
MenuItem* build_menu_tree(void);

// 设置根菜单
void MenuManager_SetRootMenu(MenuManager* manager, MenuItem* root);

// 菜单操作处理
bool MenuManager_HandleOperation(MenuManager* manager, MenuOperation op);

// 显示当前菜单
void MenuManager_DisplayMenu(MenuManager* manager, uint8_t startX, uint8_t startY, uint8_t fontSize);

// 销毁菜单树
void MenuManager_Destroy(MenuManager* manager);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // __OLED_MENU_H