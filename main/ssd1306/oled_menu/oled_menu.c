#include "oled_menu.h"


// 前向声明，避免循环引用
void MenuManager_ClearKeyQueue(void);


// 最大移动偏移量
#define MAX_MOVE_OFFSET 5

/**
 * @brief 初始化菜单管理器
 * @param manager 菜单管理器指针
 */
void MenuManager_Init(MenuManager* manager) {
    if (manager == NULL) return;
    
    manager->rootMenu = NULL;
    manager->currentMenu = NULL;
    manager->selectedItem = NULL;
    manager->startRow = 0;
    manager->moveMode = IMAGE_MOVE_LEFT_RIGHT;  // 默认使用左右移动模式
    manager->isEvenVisibleImages = 1;  // 默认初始化为偶数
    manager->blockKeyEvents = false;  // 默认不阻塞按键事件
    manager->startRowInitialized = false;  // 默认startRow未初始化
    
    // 初始化状态栈
    manager->stackDepth = 0;
    memset(manager->stateStack, 0, sizeof(manager->stateStack));
}


/**
 * @brief 创建文本菜单项
 * @param name 菜单项名称
 * @param action 选择菜单项执行的操作
 * @return 菜单项指针，失败返回NULL
 */
static MenuItem* MenuItem_CreateText(char* name, MenuAction action) {
    MenuItem* item = (MenuItem*)malloc(sizeof(MenuItem));
    if (item == NULL) return NULL;
    
    // 初始化所有字段
    item->type = MENU_ITEM_TEXT;
    
    // 复制菜单项名称
    item->name = (char*)malloc(strlen(name) + 1);
    if (item->name == NULL) {
        free(item);
        return NULL;
    }
    strcpy(item->name, name);
    
    item->action = action;
    item->parent = NULL;
    item->child = NULL;
    item->next = NULL;
    
    // 图片相关字段设为默认值
    item->imageData = NULL;
    item->imageWidth = 0;
    item->imageHeight = 0;
    
    return item;
}

/**
 * @brief 创建图片菜单项（支持不显示文字）
 * @param name 菜单项名称
 * @param imageData 图片数据
 * @param width 图片宽度
 * @param height 图片高度
 * @param action 选择菜单项执行的操作
 * @param showName 是否显示名称
 * @return 菜单项指针，失败返回NULL
 */
static MenuItem* MenuItem_CreateImage(char* name, const uint8_t* imageData, uint16_t width, uint16_t height, MenuAction action) {
    MenuItem* item = (MenuItem*)malloc(sizeof(MenuItem));
    if (item == NULL) return NULL;
    
    // 初始化所有字段
    item->type = MENU_ITEM_IMAGE;
    
    // 复制菜单项名称
    item->name = (char*)malloc(strlen(name) + 1);
    if (item->name == NULL) {
        free(item);
        return NULL;
    }
    strcpy(item->name, name);
    
    item->action = action;
    item->parent = NULL;
    item->child = NULL;
    item->next = NULL;
    
    // 设置图片相关字段
    item->imageData = imageData;
    item->imageWidth = width;
    item->imageHeight = height;
    
    item->moveOffset = 0;  // 默认没有偏移
    
    return item;
}

/**
 * @brief 向父菜单项添加子菜单项
 * @param parent 父菜单项指针
 * @param child 要添加的子菜单项指针
 */
static void MenuItem_AddChild(MenuItem* parent, MenuItem* child) {
    if (parent == NULL || child == NULL) return;
    
    // 设置父子关系
    child->parent = parent;
    
    // 如果父菜单项没有子菜单，则直接添加
    if (parent->child == NULL) {
        parent->child = child;
    } else {
        // 否则添加到子菜单链表的末尾
        MenuItem* current = parent->child;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = child;
    }
}

/**
 * @brief 向菜单项添加同级菜单项
 * @param item 菜单项指针
 * @param sibling 要添加的同级菜单项指针
 */
static void MenuItem_AddSibling(MenuItem* item, MenuItem* sibling) {
    if (item == NULL || sibling == NULL) return;
    
    // 设置兄弟项的父菜单与当前项相同
    sibling->parent = item->parent;
    
    // 添加到链表末尾
    MenuItem* current = item;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = sibling;
}

/**
 * @brief 设置菜单管理器的根菜单
 * @param manager 菜单管理器指针
 * @param root 根菜单指针
 */
void MenuManager_SetRootMenu(MenuManager* manager, MenuItem* root) {
    if (manager == NULL || root == NULL) return;
    
    manager->rootMenu = root;
    manager->currentMenu = root;
    // 将selectedItem设置为第一个子菜单项，确保初始显示时有选中项
    manager->selectedItem = (root->child != NULL) ? root->child : root;
    manager->startRow = 0;
}

/**
 * @brief 获取当前菜单的子菜单项数量
 * @param menu 当前菜单指针
 * @return 子菜单项数量
 */
static uint8_t MenuItem_GetChildCount(MenuItem* menu) {
    if (menu == NULL || menu->child == NULL) return 0;
    
    uint8_t count = 0;
    MenuItem* current = menu->child;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

/**
 * @brief 根据索引获取当前菜单的子菜单项
 * @param menu 当前菜单指针
 * @param index 索引值
 * @return 子菜单项指针
 */
static MenuItem* MenuItem_GetChildByIndex(MenuItem* menu, uint8_t index) {
    if (menu == NULL || menu->child == NULL) return NULL;
    
    uint8_t count = 0;
    MenuItem* current = menu->child;
    while (current != NULL && count < index) {
        count++;
        current = current->next;
    }
    return current;
}

/**
 * @brief 获取当前菜单项在同级菜单中的索引
 * @param item 当前菜单项指针
 * @return 索引值
 */
static uint8_t MenuItem_GetIndexInParent(MenuItem* item) {
    if (item == NULL || item->parent == NULL) return 0;
    
    uint8_t index = 0;
    MenuItem* current = item->parent->child;
    while (current != NULL && current != item) {
        index++;
        current = current->next;
    }
    return index;
}

/**
 * @brief 处理菜单操作
 * @param manager 菜单管理器指针
 * @param op 菜单操作类型
 * @return 操作是否成功
 */
bool MenuManager_HandleOperation(MenuManager* manager, MenuOperation op) {
    if (manager == NULL || manager->currentMenu == NULL) return false;
    
    uint8_t childCount = MenuItem_GetChildCount(manager->currentMenu);  //获取当前子菜单项数量
    if (childCount == 0) return false; 
    
    uint8_t selectedIndex = MenuItem_GetIndexInParent(manager->selectedItem);
    
    switch (op) {
        case MENU_OP_UP:
            // 向上移动选择项
            if (manager->moveMode == IMAGE_MOVE_LEFT_RIGHT) {
                // 水平排列时，UP键不处理
                return false;
            }
            
            if (selectedIndex > 0) {
                selectedIndex--;
                manager->selectedItem = MenuItem_GetChildByIndex(manager->currentMenu, selectedIndex);
                
                // 调整显示起始行
                if (selectedIndex < manager->startRow) {
                    manager->startRow = selectedIndex  ;
                }
                return true;
            }
            break;
            
        case MENU_OP_DOWN:
            // 向下移动选择项
            if (manager->moveMode == IMAGE_MOVE_LEFT_RIGHT) {
                // 水平排列时，DOWN键不处理
                return false;
            }
            
            if (selectedIndex < childCount - 1) {
                selectedIndex++;
                manager->selectedItem = MenuItem_GetChildByIndex(manager->currentMenu, selectedIndex);
                
                // 调整显示起始行
                if (selectedIndex >= manager->startRow + manager->visibleRows) {
                    manager->startRow = selectedIndex - manager->visibleRows + 1;
                }
                return true;
            }
            break;
            
        case MENU_OP_RIGHT:
            // 水平排列时，RIGHT键处理
            if (manager->moveMode == IMAGE_MOVE_LEFT_RIGHT && manager->isEvenVisibleImages == 1) {
                if (selectedIndex < childCount - 1) {
                    selectedIndex++;
                    manager->selectedItem = MenuItem_GetChildByIndex(manager->currentMenu, selectedIndex);
                    
                    // 调整显示起始行
                    if (selectedIndex >= manager->startRow + manager->visibleRows) {
                        manager->startRow = selectedIndex - manager->visibleRows + 3;
                    }
                    return true;
                }
            }else if (manager->moveMode == IMAGE_MOVE_LEFT_RIGHT && manager->isEvenVisibleImages == 0) {
                if(selectedIndex < childCount - 1){
                    selectedIndex++ ;
                }else{
                    selectedIndex = 0 ;
                }

                manager->selectedItem = MenuItem_GetChildByIndex(manager->currentMenu, selectedIndex);
                if(selectedIndex - 1 < 0){
                    manager->startRow = childCount - 1 ;
                }else if(selectedIndex - 1 >= 0){
                    manager->startRow = selectedIndex - 1 ;
                }

                return true;
            }
            break;
            
        case MENU_OP_LEFT:
            // 水平排列时，LEFT键处理
            if (manager->moveMode == IMAGE_MOVE_LEFT_RIGHT && manager->isEvenVisibleImages == 1) {
                if (selectedIndex > 0) {
                    selectedIndex--;
                    manager->selectedItem = MenuItem_GetChildByIndex(manager->currentMenu, selectedIndex);
                    
                    // 调整显示起始行
                    if (selectedIndex < manager->startRow) {
                        manager->startRow = selectedIndex - 2;
                    }
                    return true;
                }
            }else if (manager->moveMode == IMAGE_MOVE_LEFT_RIGHT && manager->isEvenVisibleImages == 0) {
                if(selectedIndex > 0){
                    selectedIndex-- ;
                }else{
                    selectedIndex = childCount - 1 ;
                }

                manager->selectedItem = MenuItem_GetChildByIndex(manager->currentMenu, selectedIndex);
                if(selectedIndex - 1 < 0){
                    manager->startRow = childCount - 1 ;
                }else if(selectedIndex - 1 >= 0){
                    manager->startRow = selectedIndex - 1 ;
                }

                return true;
            }
            break;
                        
        case MENU_OP_BACK:
            
            // 返回上一级菜单
            if (manager->currentMenu->parent != NULL) {
                // 从栈中弹出状态，恢复到进入子菜单前的状态
                if (manager->stackDepth > 0) {
                    manager->stackDepth--;
                    MenuState* state = &manager->stateStack[manager->stackDepth];
                    
                    manager->currentMenu = manager->currentMenu->parent;
                    // 恢复到进入子菜单前的选中项
                    manager->selectedItem = MenuItem_GetChildByIndex(manager->currentMenu, state->selectedIndex);
                    // 恢复到进入子菜单前的startRow值
                    manager->startRow = state->startRow;
                    
                    // 根据当前菜单类型设置移动模式
                    if (manager->selectedItem->type == MENU_ITEM_IMAGE) {
                        manager->moveMode = IMAGE_MOVE_LEFT_RIGHT;  // 图像菜单使用左右移动
                    } else {
                        manager->moveMode = IMAGE_MOVE_UP_DOWN;     // 文本菜单使用上下移动
                    }
                    return true;
                }
            }
            break;
                        
        case MENU_OP_ENTER:
            // 进入子菜单或执行操作
            if (manager->selectedItem->child != NULL) {
                // 保存当前菜单状态到栈中，以便返回时恢复
                if (manager->stackDepth < MAX_MENU_DEPTH) {
                    uint8_t parentMenuIndex = MenuItem_GetIndexInParent(manager->selectedItem);
                    MenuState* state = &manager->stateStack[manager->stackDepth];
                    state->selectedIndex = parentMenuIndex;
                    state->startRow = manager->startRow;
                    manager->stackDepth++;
                    
                    // 有子菜单，进入子菜单
                    manager->currentMenu = manager->selectedItem;
                    manager->selectedItem = manager->selectedItem->child;
                    manager->startRow = 0;
                    
                    // 根据子菜单类型设置模式
                    if (manager->selectedItem->type == MENU_ITEM_IMAGE) {
                        manager->moveMode = IMAGE_MOVE_LEFT_RIGHT;  // 图像菜单使用左右移动
                    } else {
                        manager->moveMode = IMAGE_MOVE_UP_DOWN;     // 文本菜单使用上下移动
                    }
                    return true;
                }
            } else if (manager->selectedItem->action != NULL) {
                // 没有子菜单但有操作函数，执行操作
                // 设置blockKeyEvents为true，临时禁用按键事件处理
                manager->blockKeyEvents = true;
                // 执行操作函数
                manager->selectedItem->action();
                // 清空按键队列，避免执行action期间积累的事件被处理
                MenuManager_ClearKeyQueue();
                // 操作完成后，恢复按键事件处理
                manager->blockKeyEvents = false;
                return true;
            }
            break;
            
        default:
            break;
    }
    
    return false;
}

/**
 * @brief 根据字体大小和屏幕尺寸自动计算可见行数
 * @param fontSize 字体大小
 * @param hasTitle 是否包含菜单标题
 * @return 计算得到的可见行数
 */
// 根据字体大小计算行高
static uint8_t calculateLineHeight(uint8_t fontSize) {
    switch (fontSize) {
        case OLED_6X8_HALF:
            return 8;
        case OLED_8X16_HALF:
            return 16;
        case OLED_10X20_HALF:
            return 20;
        default:
            return 16;  // 默认16像素高
    }
}

static uint8_t calculateVisibleRows(uint8_t fontSize, bool hasTitle) {
    // 根据字体大小计算行高
    uint8_t lineHeight = calculateLineHeight(fontSize);
    
    // 计算可用高度
    uint16_t availableHeight = OLED_HEIGHT;
    
    // 如果有菜单标题，减去标题行高度和间距
    if (hasTitle) {
        availableHeight -= (lineHeight + 2);
    }
    
    // 计算可见行数，确保至少显示1行
    uint8_t visibleRows = availableHeight / lineHeight;
    return (visibleRows > 0) ? visibleRows : 1;
}

/**
 * @brief 显示当前菜单
 * @param manager 菜单管理器指针
 * @param startX 显示起始X坐标
 * @param startY 显示起始Y坐标
 * @param fontSize 字体大小
*/
void MenuManager_DisplayMenu(MenuManager* manager, uint8_t startX, uint8_t startY, uint8_t fontSize) {
    if (manager == NULL || manager->currentMenu == NULL) return;

    // 计算行高（根据字体大小）
    uint8_t lineHeight = calculateLineHeight(fontSize);
    
    // 自动计算可见行数
    bool hasTitle = (manager->currentMenu->parent != NULL);
    manager->visibleRows = calculateVisibleRows(fontSize, hasTitle);
    
    // 如果是图像菜单项，需要根据屏幕宽度重新计算可见数量
    if (manager->currentMenu->child != NULL && manager->currentMenu->child->type == MENU_ITEM_IMAGE) {
        // 获取第一个图像菜单项的宽度
        uint16_t imageWidth = manager->currentMenu->child->imageWidth;
        uint8_t spacing = 10; // 图像间距
        
        // 计算水平方向可以显示的图像数量（考虑OLED_WIDTH为128）
        uint8_t visibleImages = (OLED_WIDTH - startX) / (imageWidth + spacing);
        manager->isEvenVisibleImages = (visibleImages % 2 == 0) ? 1 : 0;
        // 只有当startRow尚未初始化时才执行此操作
        if (manager->isEvenVisibleImages == 0 && !manager->startRowInitialized) {
            manager->startRow = MenuItem_GetChildCount(manager->currentMenu) - 1;
            manager->startRowInitialized = true;  // 设置标志位表示已执行
        }
        manager->visibleRows = (visibleImages > 0) ? visibleImages : 1;
    }else{
        manager->isEvenVisibleImages = 1 ;
    }
    
    // 清空显示区域
    OLED_Clear();
    
    // 显示当前菜单名称（如果不是根菜单）
    if (manager->currentMenu->parent != NULL) {
        OLED_ShowMixString(startX, startY, manager->currentMenu->name, OLED_16X16_FULL, fontSize);
        startY += lineHeight + 2;  // 增加间距
    }
    
    // 显示菜单项列表
    uint8_t displayCount = 0;
    MenuItem* current = MenuItem_GetChildByIndex(manager->currentMenu, manager->startRow);

    while (current != NULL && displayCount < manager->visibleRows) {
        uint8_t yPos = startY, xPos = startX ,xFixed = 0;
        
        // 根据菜单项类型设置不同的显示位置
        if (current->type == MENU_ITEM_TEXT) {
            // 文本菜单项：垂直排列
            yPos = startY + displayCount * lineHeight;
            xPos = startX;
        } else if (current->type == MENU_ITEM_IMAGE && manager->isEvenVisibleImages == 1) {
            // 图片菜单项：水平排列，使用屏幕底部对齐
            // 对于图像菜单项，使用OLED_HEIGHT作为基准，确保图像在屏幕内显示
            yPos = OLED_HEIGHT - current->imageHeight; // 从屏幕底部向上对齐
            
            // 计算水平等距排列的x坐标
            uint16_t totalWidth = manager->visibleRows * current->imageWidth + (manager->visibleRows - 1) * 10;
            uint16_t startXPos = (OLED_WIDTH - totalWidth) / 2;
            xPos = startXPos + displayCount * (current->imageWidth + 10);
        } else if (current->type == MENU_ITEM_IMAGE && manager->isEvenVisibleImages == 0){
            yPos = OLED_HEIGHT - current->imageHeight; // 从屏幕底部向上对齐

            uint16_t totalWidth = manager->visibleRows * current->imageWidth + (manager->visibleRows - 1) * 10;
            uint16_t startXPos = (OLED_WIDTH - totalWidth) / 2;
            xFixed = startXPos + ((manager->visibleRows - 1) / 2) * (current->imageWidth + 10);
            xPos = startXPos + displayCount * (current->imageWidth + 10);
        } 

        // 如果是当前选中项且为文本菜单项，反色显示背景
        if (current == manager->selectedItem && current->type == MENU_ITEM_TEXT) {
            OLED_ReverseArea(startX, yPos, OLED_WIDTH - startX, lineHeight);
        }
        
        // 根据菜单项类型进行不同的显示处理
        switch (current->type) {
            case MENU_ITEM_TEXT:
                // 文本菜单项，显示文本
                OLED_ShowMixString(startX, yPos, current->name, OLED_16X16_FULL, fontSize);
                break;
                
            case MENU_ITEM_IMAGE:

                // 图片菜单项，显示图片
                if (current->imageData != NULL && manager->isEvenVisibleImages == 1) {
                    // 移除行高限制，始终使用原始尺寸完整显示图像
                    OLED_ShowImage(xPos, yPos, current->imageWidth, current->imageHeight, current->imageData);

                    // // 如果是当前选中项，绘制比图像大两个像素的正方形框作为指示
                    if (current == manager->selectedItem) {
                        OLED_DrawRectangle(xPos - 1, yPos - 1, current->imageWidth + 2, current->imageHeight + 2, 0);
                    }

                }else if (current->imageData != NULL && manager->isEvenVisibleImages == 0) {
                    
                    OLED_ShowImage(xPos, yPos, current->imageWidth, current->imageHeight, current->imageData);
                    if(current == manager->selectedItem){
                        OLED_DrawRectangle(xFixed - 1, yPos - 1, current->imageWidth + 2, current->imageHeight + 2, 0);
                    }
                }

                break;
                
            default:
                // 默认按文本处理
                OLED_ShowMixString(startX, yPos, current->name, OLED_16X16_FULL, fontSize);
                break;
        }
        
        // 恢复反色区域（如果是选中项且为文本菜单项）
        if (current == manager->selectedItem && current->type == MENU_ITEM_TEXT) {
            OLED_ReverseArea(startX, yPos, OLED_WIDTH - startX, lineHeight);
        }
        
        // 如果有子菜单，显示指示箭头
        if (current->child != NULL && manager->currentMenu->parent != NULL) {
            OLED_ShowString(OLED_WIDTH - 12, yPos, ">", fontSize);
        }
        
        if (current->next == NULL) {
            // 遍历到最后一个同层菜单项，自动指向第一个同层菜单项
            current = MenuItem_GetChildByIndex(manager->currentMenu, 0);
        }else {
            current = current->next;
        }

        displayCount++;
    }
    
    // 刷新OLED显示
    OLED_Update();
}

/**
 * @brief 递归销毁菜单树
 * @param item 菜单项指针
 */
static void MenuItem_DestroyRecursive(MenuItem* item) {
    if (item == NULL) return;
    
    // 递归销毁子菜单
    if (item->child != NULL) {
        MenuItem_DestroyRecursive(item->child);
    }
    
    // 销毁同级菜单项
    if (item->next != NULL) {
        MenuItem_DestroyRecursive(item->next);
    }

    // 释放菜单项名称和菜单项本身
    free(item->name);
    free(item);
}

/**
 * @brief 销毁菜单管理器和所有菜单
 * @param manager 菜单管理器指针
 */
void MenuManager_Destroy(MenuManager* manager) {
    if (manager == NULL) return;
    
    // 递归销毁根菜单树
    if (manager->rootMenu != NULL) {
        MenuItem_DestroyRecursive(manager->rootMenu);
    }
    
    // 重置管理器状态
    manager->rootMenu = NULL;
    manager->currentMenu = NULL;
    manager->selectedItem = NULL;
    manager->visibleRows = 0;
    manager->startRow = 0;
}

/**
 * @brief 构建菜单树
 * @return 根菜单指针
 */
MenuItem* build_menu_tree(void) {
    MenuItem* items[MENU_ITEM_COUNT]; // 存储创建的菜单项指针
    
    // 初始化所有元素为NULL
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        items[i] = NULL;
    }
    
    // 第一步：创建所有菜单项
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        MenuItemDef* def = &menuItems[i];
        
        if (def->type == 1) {
            // 图像菜单
            items[i] = MenuItem_CreateImage((char*)def->name, def->image, def->imageWidth, def->imageHeight, def->action);
        } else {
            // 文本菜单或动作菜单
            items[i] = MenuItem_CreateText((char*)def->name, def->action);
        }
    }
    
    // 第二步：构建菜单层次结构
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        MenuItemDef* def = &menuItems[i];
        
        if (def->parentIndex >= 0 && def->parentIndex < MENU_ITEM_COUNT) {
            // 找到父菜单
            MenuItem* parent = items[def->parentIndex];
            MenuItem* child = items[i];
            
            if (parent && child) {
                // 检查父菜单是否已有子菜单
                if (parent->child == NULL) {
                    // 第一个子菜单，直接添加为子节点
                    MenuItem_AddChild(parent, child);
                } else {
                    // 不是第一个子菜单，添加为同级菜单
                    MenuItem* lastChild = parent->child;
                    // 找到最后一个同级菜单
                    while (lastChild->next != NULL) {
                        lastChild = lastChild->next;
                    }
                    // 添加为同级菜单
                    MenuItem_AddSibling(lastChild, child);
                }
            }
        }
    }
    
    // 返回根菜单（索引为0的菜单项）
    return items[0];
}

