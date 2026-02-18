/*
 * main.c - 我的世界
 玩家按1～9键挖/放相邻方块
 建议合成镐，桥、梯子道具
 挖石头获得洞口道具
 放置洞口道具下入洞穴维度
 放梯子道具可上到陆地维度
 */

#include "mrc_base.h"
#include "mrc_graphics.h"
#include "fontread.h"
#include "mpc.h"
#include "mrc_exb.h"
#include "mrc_base_i.h"
#include "mrc_tool.h"

/*==============================================================================
 * 宏定义
 *============================================================================*/
#define SCREEN_WIDTH        240
#define SCREEN_HEIGHT       320
#define BLOCK_WIDTH         24
#define BLOCK_HEIGHT        24
#define WORLD_WIDTH         128
#define WORLD_HEIGHT        128
#define INVENTORY_SLOTS      32
#define BG_BLOCK_COUNT        8
#define FG_BLOCK_COUNT       14
#define PLAYER_DIR_COUNT      8

#define NOISE_SEED         1337
#define WATER_THRESHOLD     800     /* 水面阈值 */
#define BEACH_WIDTH         100     /* 海滩宽度 */

/*==============================================================================
 * 枚举类型
 *============================================================================*/
typedef enum PlayerDir {
    DIR_NORTH,
    DIR_NORTHEAST,
    DIR_EAST,
    DIR_SOUTHEAST,
    DIR_SOUTH,
    DIR_SOUTHWEST,
    DIR_WEST,
    DIR_NORTHWEST
} PlayerDir;

typedef enum State {
    ST_INGAME,      /* 游戏中 */
    ST_INVENTORY,   /* 背包 */
    ST_CRAFTING,    /* 合成 */
    ST_MENU,         /* 菜单 */
    ST_CONFIRMRESET      /*重置游戏确认页*/
} State;

/*==============================================================================
 * 结构体定义
 *============================================================================*/
typedef struct InventoryItem {
    char id;
    char count;
} InventoryItem;

typedef struct Player {
    int x;
    int y;
    int dimension;  //陆地/洞穴
    PlayerDir direction;
    InventoryItem items[INVENTORY_SLOTS];  //背包
    int selectedItem;  //已选中物品
} Player;

typedef struct Block {
    char background;
    char foreground;
} Block;

typedef struct BlockInfo {
    mrpfileSt* image;
    int palette;  //配色
    int canStepOn;
    int canPlace;
    int canPlaceOnWater;
    char dropItem;  //掉落物(背包里)，比如 岩石->石头、石头->洞口、树->木头
    char dropItemCount;  //掉落数量
    char requiresPickaxe;
} BlockInfo;

typedef struct Recipe {
    InventoryItem result;
    InventoryItem items[4];
} Recipe;  //合成配方

/*==============================================================================
 * 全局变量声明
 *============================================================================*/
/* 游戏状态 */
State state;
Player player;
Block blockCache[16];
int32 blockCacheIndex;
int32 inventoryX;
int32 inventoryY;
int32 world_file;
int32 player_file;

int32 world_seed_offset;        /* 世界种子偏移，用于生成随机地形 */
int32 timerkey;  //长按键
int isgameing;

/* BMP图片资源 */
mrpfileSt img_ground;
mrpfileSt img_short_grass;
mrpfileSt img_tall_grass;
mrpfileSt img_ice;
mrpfileSt img_water;
mrpfileSt img_tree;
mrpfileSt img_dead_tree;
mrpfileSt img_fallen_tree;
mrpfileSt img_cactus;
mrpfileSt img_rock;
mrpfileSt img_planks;
mrpfileSt img_workbench;
mrpfileSt img_pickaxe;
mrpfileSt img_stick;
mrpfileSt img_bridge;
mrpfileSt img_hole;
mrpfileSt img_stone;
mrpfileSt img_ladder;
mrpfileSt img_player_n;
mrpfileSt img_player_ne;
mrpfileSt img_player_e;
mrpfileSt img_player_se;
mrpfileSt img_player_s;
mrpfileSt img_player_sw;
mrpfileSt img_player_w;
mrpfileSt img_player_nw;

/* 玩家图片数组 */
mrpfileSt *playerImages[PLAYER_DIR_COUNT];

/* 方块信息数组 */
BlockInfo bgBlocks[BG_BLOCK_COUNT];
BlockInfo fgBlocks[FG_BLOCK_COUNT];

/* 前景物品名称表，角标 = ID - 1 */
const char itemname[14][8] = {
    "树",      // ID1
    "枯树",    // ID2
    "倒树",    // ID3
    "仙人掌",  // ID4
    "岩石",    // ID5
    "木板",    // ID6
    "工作台",  // ID7
    "镐",      // ID8
    "棍子",    // ID9
    "桥",      // ID10
    "洞口",    // ID11
    "石头",    // ID12
    "梯子",    // ID13
    "稀有矿"   // ID14
};

/* 常量全局变量（可直接初始化） */
const uint32 palettes[8][3] = {
    {0xFFFF6600, 0xFF00CC00, 0xFF00FF66},
    {0xFF665544, 0xFFFFCCAA, 0xFFFFFF99},
    {0xFF3366FF, 0xFF6699FF, 0xFFFFFFFF},
    {0xFF000000, 0xFF665544, 0xFFFFCCAA},
    {0xFF000000, 0xFF999999, 0xFF000000},
    {0xFF999999, 0xFF999999, 0xFFEEEEEE},
    {0xFF000000, 0xFF444444, 0xFF665544},
    {0xFF222222, 0xFF999999, 0xFFEEEEEE}
};
//镐、梯子、桥合成配方
const Recipe recipes[] = {
    {{8, 1}, {{6, 4}, {0, 0}, {0, 0}, {0, 0}}},
    {{13, 1}, {{6, 4}, {0, 0}, {0, 0}, {0, 0}}},
    {{10, 1}, {{6, 4}, {0, 0}, {0, 0}, {0, 0}}},
    {{0, 0}, {{0, 0}, {0, 0}, {0, 0}, {0, 0}}}
};

/*======================================================================
 * 函数声明
 *=====================================================================*/
static void initGlobalVars(void);
static void initFiles(void);
static void loadAllBmpResources(void);
static void releaseAllBmpResources(void);
static void timer_callback(int32 data);
static Block generateBlockOverworld(int x, int y);
static Block generateBlockCave(int x, int y);
static int intLerp(int a, int b, int t, int tMax);
static int intPerlin2D(int x, int y, int seed);
static int intRidgedNoise2D(int x, int y, int seed);
static int outOfBounds(int x, int y);
static Block getBlock(int x, int y);
static void setBlock(int x, int y, Block block);
static void drawBlock(int x, int y);
static void drawPlayer(void);
static void drawInventoryItem(InventoryItem item, int x, int y);
static int haveAdjacentBlock(int id);
static void drawHUD(void);
static void drawWorld(void);
static void movePlayer(int dX, int dY);
static int giveOneItem(InventoryItem *newItems, int id);
static int giveItem(int id, int count, int dry);
static int takeOneItem(InventoryItem *newItems, int id);
static int takeItem(int id, int count, int dry);
static void placeBlock(int dX, int dY);
static void save_world(int32 tid);
static void handle_keyevt_ingame(int32 type, int32 keycode);
static void handle_keyevt_inventory(int32 keycode);
static int canCraft(Recipe recipe);
static void handle_keyevt_crafting(int32 keycode);
static void drawInventory(void);
static void drawCrafting(void);
static void drawMenu(void);
static void drawSoftKeyHints(char* left, char* right);
static void drawConfirmResetGame(void);
static void setState(State newState);
static int canStandAt(int x, int y, int dimension);
void timerkeyCB(int32 data);
/*======================================================================
 * 全局变量初始化
 *=====================================================================*/
static void initGlobalVars(void) {
    
    my_sand();
    world_seed_offset = mrc_rand() % 10000;  /* 随机世界种子 */
    
    blockCacheIndex = -16;
    inventoryX = 0;
    inventoryY = 0;
    player.x = 128;
    player.y = 128;
    player.dimension = 0;
    player.direction = DIR_NORTH;
    player.selectedItem = 0;
    world_file = 0;
    player_file = 0;
    
    mrc_memset(player.items, 0, sizeof(player.items));
    mrc_memset(blockCache, 0, sizeof(blockCache));
}

/*======================================================================
 * 资源加载与释放
 
 背景方块：值对应[角标]（0~7）
·前景方块：值比[角标]大1（1~14)，安全措施，if (前景值>0) 绘图
 *=====================================================================*/
static void loadAllBmpResources(void) {
    loadmrpfile("img_ground.bmp", &img_ground);
    loadmrpfile("img_short_grass.bmp", &img_short_grass);
    loadmrpfile("img_tall_grass.bmp", &img_tall_grass);
    loadmrpfile("img_ice.bmp", &img_ice);
    loadmrpfile("img_water.bmp", &img_water);
    loadmrpfile("img_tree.bmp", &img_tree);
    loadmrpfile("img_dead_tree.bmp", &img_dead_tree);
    loadmrpfile("img_fallen_tree.bmp", &img_fallen_tree);
    loadmrpfile("img_cactus.bmp", &img_cactus);
    loadmrpfile("img_rock.bmp", &img_rock);
    loadmrpfile("img_planks.bmp", &img_planks);
    loadmrpfile("img_workbench.bmp", &img_workbench);
    loadmrpfile("img_pickaxe.bmp", &img_pickaxe);
    loadmrpfile("img_stick.bmp", &img_stick);
    loadmrpfile("img_bridge.bmp", &img_bridge);
    loadmrpfile("img_hole.bmp", &img_hole);
    loadmrpfile("img_stone.bmp", &img_stone);
    loadmrpfile("img_ladder.bmp", &img_ladder);
    loadmrpfile("img_player_n.bmp", &img_player_n);
    loadmrpfile("img_player_ne.bmp", &img_player_ne);
    loadmrpfile("img_player_e.bmp", &img_player_e);
    loadmrpfile("img_player_se.bmp", &img_player_se);
    loadmrpfile("img_player_s.bmp", &img_player_s);
    loadmrpfile("img_player_sw.bmp", &img_player_sw);
    loadmrpfile("img_player_w.bmp", &img_player_w);
    loadmrpfile("img_player_nw.bmp", &img_player_nw);
    
    playerImages[0] = &img_player_n;
    playerImages[1] = &img_player_ne;
    playerImages[2] = &img_player_e;
    playerImages[3] = &img_player_se;
    playerImages[4] = &img_player_s;
    playerImages[5] = &img_player_sw;
    playerImages[6] = &img_player_w;
    playerImages[7] = &img_player_nw;
    
    /* 初始化背景方块信息 */
    bgBlocks[0].image = &img_ground;  //普通草地
    bgBlocks[0].palette = 0;
    bgBlocks[0].canStepOn = 0;
    bgBlocks[0].canPlace = 0;
    bgBlocks[0].canPlaceOnWater = 0;
    
    bgBlocks[1].image = &img_short_grass;  //短草地
    bgBlocks[1].palette = 0;
    bgBlocks[1].canStepOn = 0;
    bgBlocks[1].canPlace = 0;
    bgBlocks[1].canPlaceOnWater = 0;
    
    bgBlocks[2].image = &img_tall_grass;  //高草地
    bgBlocks[2].palette = 0;
    bgBlocks[2].canStepOn = 0;
    bgBlocks[2].canPlace = 0;
    bgBlocks[2].canPlaceOnWater = 0;
    
    bgBlocks[3].image = &img_ground;  //普通地面+调色1，枯黄
    bgBlocks[3].palette = 1;
    bgBlocks[3].canStepOn = 0;
    bgBlocks[3].canPlace = 0;
    bgBlocks[3].canPlaceOnWater = 0;
    
    bgBlocks[4].image = &img_ground;  //普通地面+调色2，茂盛
    bgBlocks[4].palette = 2;
    bgBlocks[4].canStepOn = 0;
    bgBlocks[4].canPlace = 0;
    bgBlocks[4].canPlaceOnWater = 0;
    
    bgBlocks[5].image = &img_ice;  //冰原
    bgBlocks[5].palette = 2;
    bgBlocks[5].canStepOn = 0;
    bgBlocks[5].canPlace = 0;
    bgBlocks[5].canPlaceOnWater = 0;
    
    bgBlocks[6].image = &img_water;  //水面
    bgBlocks[6].palette = 2;
    bgBlocks[6].canStepOn = 0;
    bgBlocks[6].canPlace = 0;
    bgBlocks[6].canPlaceOnWater = 0;
    
    bgBlocks[7].image = &img_ground;  //普通地面+调色6，石质
    bgBlocks[7].palette = 6;
    bgBlocks[7].canStepOn = 0;
    bgBlocks[7].canPlace = 0;
    bgBlocks[7].canPlaceOnWater = 0;
    
    /* 初始化前景方块信息 */
    //挖树获得木板+4
    fgBlocks[0].image = &img_tree;
    fgBlocks[0].palette = 0;
    fgBlocks[0].canStepOn = 0;
    fgBlocks[0].canPlace = 0;
    fgBlocks[0].canPlaceOnWater = 0;
    fgBlocks[0].dropItem = 6;
    fgBlocks[0].dropItemCount = 3;
    fgBlocks[0].requiresPickaxe = 0;
    //挖枯树获得木板+2
    fgBlocks[1].image = &img_dead_tree;
    fgBlocks[1].palette = 0;
    fgBlocks[1].canStepOn = 0;
    fgBlocks[1].canPlace = 0;
    fgBlocks[1].canPlaceOnWater = 0;
    fgBlocks[1].dropItem = 6;
    fgBlocks[1].dropItemCount = 2;
    fgBlocks[1].requiresPickaxe = 0;
    //挖倒树获得木板+3
    fgBlocks[2].image = &img_fallen_tree;
    fgBlocks[2].palette = 0;
    fgBlocks[2].canStepOn = 0;
    fgBlocks[2].canPlace = 0;
    fgBlocks[2].canPlaceOnWater = 0;
    fgBlocks[2].dropItem = 6;
    fgBlocks[2].dropItemCount = 1;
    fgBlocks[2].requiresPickaxe = 0;
    //挖仙人掌无材料获得
    fgBlocks[3].image = &img_cactus;  //仙人掌
    fgBlocks[3].palette = 0;
    fgBlocks[3].canStepOn = 0;
    fgBlocks[3].canPlace = 0;
    fgBlocks[3].canPlaceOnWater = 0;
    fgBlocks[3].dropItem = 0;
    fgBlocks[3].dropItemCount = 0;
    fgBlocks[3].requiresPickaxe = 0;
    //挖岩石获得石头+1
    fgBlocks[4].image = &img_rock;
    fgBlocks[4].palette = 1;
    fgBlocks[4].canStepOn = 1;
    fgBlocks[4].canPlace = 1;
    fgBlocks[4].canPlaceOnWater = 0;
    fgBlocks[4].dropItem = 12;
    fgBlocks[4].dropItemCount = 1;
    fgBlocks[4].requiresPickaxe = 0;
    
    //挖木板获得木板
    fgBlocks[5].image = &img_planks;  //木板
    fgBlocks[5].palette = 3;
    fgBlocks[5].canStepOn = 1;
    fgBlocks[5].canPlace = 1;
    fgBlocks[5].canPlaceOnWater = 1;
    fgBlocks[5].dropItem = 6;
    fgBlocks[5].dropItemCount = 1;
    fgBlocks[5].requiresPickaxe = 0;
    //挖工作台获得工作台
    fgBlocks[6].image = &img_workbench;  //工作台
    fgBlocks[6].palette = 3;
    fgBlocks[6].canStepOn = 0;
    fgBlocks[6].canPlace = 1;
    fgBlocks[6].canPlaceOnWater = 0;
    fgBlocks[6].dropItem = 7;
    fgBlocks[6].dropItemCount = 1;
    fgBlocks[6].requiresPickaxe = 0;
    //挖镐获得镐
    fgBlocks[7].image = &img_pickaxe;  //镐
    fgBlocks[7].palette = 3;
    fgBlocks[7].canStepOn = 0;
    fgBlocks[7].canPlace = 1;
    fgBlocks[7].canPlaceOnWater = 0;
    fgBlocks[7].dropItem = 8;
    fgBlocks[7].dropItemCount = 1;
    fgBlocks[7].requiresPickaxe = 0;
    //挖棍子获得棍子
    fgBlocks[8].image = &img_stick;  //棍子
    fgBlocks[8].palette = 3;
    fgBlocks[8].canStepOn = 0;
    fgBlocks[8].canPlace = 1;
    fgBlocks[8].canPlaceOnWater = 0;
    fgBlocks[8].dropItem = 9;
    fgBlocks[8].dropItemCount = 1;
    fgBlocks[8].requiresPickaxe = 0;
    //挖桥获得桥
    fgBlocks[9].image = &img_bridge;
    fgBlocks[9].palette = 4;
    fgBlocks[9].canStepOn = 1;
    fgBlocks[9].canPlace = 1;
    fgBlocks[9].canPlaceOnWater = 1;
    fgBlocks[9].dropItem = 10;
    fgBlocks[9].dropItemCount = 1;
    fgBlocks[9].requiresPickaxe = 0;
    //挖洞口道具无获得
    fgBlocks[10].image = &img_hole;  
    fgBlocks[10].palette = 3;
    fgBlocks[10].canStepOn = 1;
    fgBlocks[10].canPlace = 1;
    fgBlocks[10].canPlaceOnWater = 0;
    fgBlocks[10].dropItem = 0;
    fgBlocks[10].dropItemCount = 0;
    fgBlocks[10].requiresPickaxe = 0;
    //挖石头获得洞口
    fgBlocks[11].image = &img_stone;
    fgBlocks[11].palette = 5;
    fgBlocks[11].canStepOn = 1;
    fgBlocks[11].canPlace = 1;
    fgBlocks[11].canPlaceOnWater = 0;
    fgBlocks[11].dropItem = 11; 
    fgBlocks[11].dropItemCount = 1;
    fgBlocks[11].requiresPickaxe = 1;
    //挖梯子获得梯子
    fgBlocks[12].image = &img_ladder;  //梯子
    fgBlocks[12].palette = 3;
    fgBlocks[12].canStepOn = 1;
    fgBlocks[12].canPlace = 1;
    fgBlocks[12].canPlaceOnWater = 0;
    fgBlocks[12].dropItem = 13;  
    fgBlocks[12].dropItemCount = 1;
    fgBlocks[12].requiresPickaxe = 0;
    //挖稀有石头获得梯子
    fgBlocks[13].image = &img_stone; //稀有石头
    fgBlocks[13].palette = 7;  //有配色
    fgBlocks[13].canStepOn = 1;
    fgBlocks[13].canPlace = 1;
    fgBlocks[13].canPlaceOnWater = 0;
    fgBlocks[13].dropItem = 13;  //破坏稀有矿石能得到梯子
    fgBlocks[13].dropItemCount = 1;
    fgBlocks[13].requiresPickaxe = 1;
}

static void releaseAllBmpResources(void) {
    freemrpfile(&img_ground);
    freemrpfile(&img_short_grass);
    freemrpfile(&img_tall_grass);
    freemrpfile(&img_ice);
    freemrpfile(&img_water);
    freemrpfile(&img_tree);
    freemrpfile(&img_dead_tree);
    freemrpfile(&img_fallen_tree);
    freemrpfile(&img_cactus);
    freemrpfile(&img_rock);
    freemrpfile(&img_planks);
    freemrpfile(&img_workbench);
    freemrpfile(&img_pickaxe);
    freemrpfile(&img_stick);
    freemrpfile(&img_bridge);
    freemrpfile(&img_hole);
    freemrpfile(&img_stone);
    freemrpfile(&img_ladder);
    freemrpfile(&img_player_n);
    freemrpfile(&img_player_ne);
    freemrpfile(&img_player_e);
    freemrpfile(&img_player_se);
    freemrpfile(&img_player_s);
    freemrpfile(&img_player_sw);
    freemrpfile(&img_player_w);
    freemrpfile(&img_player_nw);
}

/*======================================================================
 * 定时器回调
 *===================================================================*/

/*==================================================================
 * 地形生成函数
 *===================================================================*/
static int intLerp(int a, int b, int t, int tMax) {
    return a + (b - a) * t / tMax;
}

static int intPerlin2D(int x, int y, int seed) {
    int32 scaledX, scaledY, x0, y0, x1, y1;
    int32 hash00, hash01, hash10, hash11;
    int32 dx0, dy0, dot00, dot01, dot10, dot11;
    int32 tx, ty, lerpX0, lerpX1, noise;
    static const int GRADIENTS[8][2] = {{1,1}, {-1,1}, {1,-1}, {-1,-1}, 
                                        {2,1}, {-2,1}, {2,-1}, {-2,-1}};
    
    scaledX = x * 64 / 100;
    scaledY = y * 64 / 100;
    x0 = scaledX & ~1;
    y0 = scaledY & ~1;
    x1 = x0 + 2;
    y1 = y0 + 2;
    
    hash00 = (x0 * 1103515245 + y0 * 12345 + seed) & 7;
    hash01 = (x0 * 1103515245 + y1 * 12345 + seed) & 7;
    hash10 = (x1 * 1103515245 + y0 * 12345 + seed) & 7;
    hash11 = (x1 * 1103515245 + y1 * 12345 + seed) & 7;
    
    dx0 = scaledX - x0;
    dy0 = scaledY - y0;
    
    dot00 = GRADIENTS[hash00][0] * dx0 + GRADIENTS[hash00][1] * dy0;
    dot01 = GRADIENTS[hash01][0] * dx0 + GRADIENTS[hash01][1] * (scaledY - y1);
    dot10 = GRADIENTS[hash10][0] * (scaledX - x1) + GRADIENTS[hash10][1] * dy0;
    dot11 = GRADIENTS[hash11][0] * (scaledX - x1) + GRADIENTS[hash11][1] * (scaledY - y1);
    
    tx = dx0 * dx0 * (3 - 2 * dx0) / 4;
    ty = dy0 * dy0 * (3 - 2 * dy0) / 4;
    
    lerpX0 = intLerp(dot00, dot10, tx, 4);
    lerpX1 = intLerp(dot01, dot11, tx, 4);
    noise = intLerp(lerpX0, lerpX1, ty, 4);
    
    return noise * 250;
}

static int intRidgedNoise2D(int x, int y, int seed) {
    int noise, absNoise;
    noise = intPerlin2D(x, y, seed);
    absNoise = (noise < 0) ? -noise : noise;
    return 2000 - absNoise * 2;
}

/*
heightNoise Ridged -1000 ~ 2000 受种子影响，大部分区域在 0~2000 之间，负数极少
biomeNoise Perlin -1500 ~ 1500 平滑变化，正负均匀
detailNoise Perlin -1500 ~ 1500 高频细节，变化较快
*/
//生成陆地维度
static Block generateBlockOverworld(int x, int y) {
    int32 heightNoise, biomeNoise, detailNoise;
    int32 randomVal;
    Block block;
    
    /* 使用世界种子偏移让每次生成不同 */
    heightNoise = intRidgedNoise2D(x, y, NOISE_SEED + world_seed_offset);
    biomeNoise = intPerlin2D(x, y, NOISE_SEED + 100 + world_seed_offset);
    detailNoise = intPerlin2D(x * 2, y * 2, NOISE_SEED + 200 + world_seed_offset);
    
    /* 基于位置的随机数 */
    randomVal = (x * 7919 + y * 7907 + world_seed_offset) % 100;
    if (randomVal < 0) randomVal = -randomVal;
    
    /* 水陆判断 */
    if (heightNoise < WATER_THRESHOLD - BEACH_WIDTH) {
        /* 深水区 */
        block.background = 6;
        block.foreground = 0;
    } else if (heightNoise < WATER_THRESHOLD) {
        /* 浅水/海滩区 - 可能放桥 */
        block.background = 6;
        if (randomVal < 10) {
            block.foreground = 10;  /* 桥 */
        } else {
            block.foreground = 0;
        }
    } else {
        /* 陆地区域 */
        if (biomeNoise < -300) {
            block.background = 5;  /* 冰原 */
        } else if (biomeNoise < 0) {
            block.background = 1;  /* 短草地 */
        } else if (biomeNoise < 300) {
            block.background = 0;  /* 普通草地 */
        } else {
            block.background = 2;  /* 高草地 */
        }
        
        /* 放置前景物体 */
        block.foreground = 0;
        
        if (detailNoise > 350 && randomVal < 25) {
            if (biomeNoise < -200) {
                block.foreground = 3;  /* 倒树 */
            } else {
                block.foreground =  1; /* 树 */
            }
        } else if (detailNoise > 200 && randomVal < 50) {
            if (randomVal < 10) {
                block.foreground = 5;  /* 岩石 */
            } else if (randomVal > 10 && randomVal < 20)
            {
				block.foreground = 4;  /*仙人掌*/
            } 
            else if (randomVal > 20 && randomVal < 30)
            {
				block.foreground =  1; /* 树 */
            } else {
                block.foreground = 2;  /* 枯树 */
            }
        } else if (biomeNoise > 300 && randomVal < 5)
        {
			block.foreground = 4;  /*仙人掌*/
        }
    }
    mrc_printf("biomeNoise: %d, detailNoise: %d , randomVal: %d\n", biomeNoise, detailNoise, randomVal);
    return block;
}

/*
caveNoise Ridged -1000 ~ 2000 同 heightNoise
mineralNoise Perlin -1500 ~ 1500 高频矿物分布
*/
//生成洞穴维度
static Block generateBlockCave(int x, int y) {
    int32 caveNoise, mineralNoise, randomVal;
    Block block;
    
    caveNoise = intRidgedNoise2D(x, y, NOISE_SEED + 400 + world_seed_offset);
    mineralNoise = intPerlin2D(x * 3, y * 3, NOISE_SEED + 500 + world_seed_offset);
    
    randomVal = (x * 7919 + y * 7907 + world_seed_offset + 1000) % 100;
    if (randomVal < 0) randomVal = -randomVal;
    
    block.background = 7;  /* 默认石头 */
    
    if (caveNoise > 1300) {
        /* 洞穴通道 */
        if (mineralNoise > 800 && randomVal < 10) {
            block.foreground = 14;  /* 稀有矿石 */
        } else if (mineralNoise < -800 && randomVal < 20) {
            block.foreground = 5;  /* 岩石 */
        } else {
            block.foreground = 0;   /* 空气 */
        }
    } else {
        /* 实心区域 */
        if (mineralNoise > 800 && randomVal < 20) {
            block.foreground = 14;  /* 稀有矿石 */
        } else if (mineralNoise > 100 && randomVal < 40) {
            block.foreground = 5;  /* 岩石 */
        } else {
            block.foreground = 3; //倒树
        }
    }
    mrc_printf("mineralNoise: %d, randomVal: %d\n", mineralNoise, randomVal);
    return block;
}

/*==============================================================================
 * 文件操作
 *============================================================================*/
static void initFiles(void) {
    int32 i, sx, sy;
    uint32 progress;
    Block block;
    static char text[30];
    
    mrc_mkDir("mtmine\\");
    
    if (mrc_fileState("mtmine\\world.bin") == MR_IS_INVALID) {
        
        /* 写入模式创建文件 */
        world_file = mrc_open("mtmine\\world.bin", MR_FILE_RDWR | MR_FILE_CREATE);
        
        /* 生成世界数据并写入 */
        for (i = 0; i < WORLD_WIDTH * WORLD_HEIGHT * 2; i++) {
            if (i < WORLD_WIDTH * WORLD_HEIGHT) {
                block = generateBlockOverworld(i % WORLD_WIDTH, i / WORLD_WIDTH);
            } else {
                block = generateBlockCave(i % WORLD_WIDTH, i / WORLD_WIDTH);
            }
            mrc_write(world_file, &block, sizeof(Block));
            
            /* 显示进度 */
            if (i < WORLD_WIDTH * WORLD_HEIGHT * 2) {
                progress = 100 * i / (WORLD_WIDTH * WORLD_HEIGHT * 2);
                mrc_sprintf(text, "生成世界…%d%%", progress);
                mrc_drawRect(0, SCRH/4*3, SCRW, 20, 0, 0, 0);
                _drawText(text, (SCRW - _textWidth(text, 0))/2, SCRH/4*3, 
                         255, 255, 255, 0, 1);
                mrc_refreshScreen(0, SCRH/4*3, SCRW, 20);
            }
        }                      
        
    } else {
        world_file = mrc_open("mtmine\\world.bin", MR_FILE_RDWR);
    }

	if (mrc_fileState("mtmine\\player.bin") == MR_IS_INVALID)
	{
		/* 寻找出生点（现在能正确读取） */
		for (sy = 0; sy < WORLD_HEIGHT; sy++) {
			for (sx = 0; sx < WORLD_WIDTH; sx++) {
				if (canStandAt(sx, sy, 0)) {
					player.x = sx;
					player.y = sy;
					goto spawn_found;
				}
			}
		}

spawn_found:
		player_file = mrc_open("mtmine\\player.bin", MR_FILE_RDWR | MR_FILE_CREATE);
		save_world(0);
	} 
	else
	{
		player_file = mrc_open("mtmine\\player.bin", MR_FILE_RDWR);
		mrc_read(player_file, &player, sizeof(Player));
	}
}

static void save_world(int32 tid) {
    if (player_file) {
        mrc_seek(player_file, 0, MR_SEEK_SET);
        mrc_write(player_file, &player, sizeof(Player));
    }
}

/*==============================================================================
 * 方块操作
 *============================================================================*/
static int outOfBounds(int x, int y) {
    return x < 0 || x >= WORLD_WIDTH || y < 0 || y >= WORLD_HEIGHT;
}

static Block getBlock(int x, int y) {
    int32 index;
    Block block;
    
    index = player.dimension * WORLD_WIDTH * WORLD_HEIGHT + y * WORLD_WIDTH + x;
    
    if (index < blockCacheIndex || index > blockCacheIndex + 15) {
        mrc_seek(world_file, index * sizeof(Block), MR_SEEK_SET);
        mrc_read(world_file, blockCache, 16 * sizeof(Block));
        blockCacheIndex = index;
    }
    
    block = blockCache[index - blockCacheIndex];
    return block;
}

static void setBlock(int x, int y, Block block) {
    int32 index;
    
    index = player.dimension * WORLD_WIDTH * WORLD_HEIGHT + y * WORLD_WIDTH + x;
    mrc_seek(world_file, index * sizeof(Block), MR_SEEK_SET);
    mrc_write(world_file, &block, sizeof(Block));
    
    if (index >= blockCacheIndex && index < blockCacheIndex + 16) {
        blockCache[index - blockCacheIndex] = block;
    }
}

/*==============================================================================
 * 绘制函数
 *============================================================================*/
static void drawBlock(int x, int y) {
    int32 screenX, screenY;
    Block block;
    BlockInfo bgInfo;
    BlockInfo fgInfo;
    
    screenX = SCREEN_WIDTH / 2 - BLOCK_WIDTH / 2 + (x - player.x) * BLOCK_WIDTH;
    screenY = SCREEN_HEIGHT / 2 - BLOCK_HEIGHT / 2 + (y - player.y) * BLOCK_HEIGHT;
    
    if (screenX + BLOCK_WIDTH < 0 || screenY + BLOCK_HEIGHT < 0) return;
    if (screenX > SCREEN_WIDTH || screenY > SCREEN_HEIGHT) return;
    
    if (outOfBounds(x, y)) {
        if (player.dimension == 0) {
            block.background = 6;
            block.foreground = 0;
        } else {
            block.background = 3;
            block.foreground = 0;
        }
    } else {
        block = getBlock(x, y);
    }
    
    bgInfo = bgBlocks[block.background];
    gl_drawRect(screenX, screenY, BLOCK_WIDTH, BLOCK_HEIGHT, palettes[bgInfo.palette][0]);
    mrc_bitmapShowEx(bgInfo.image->bitmap, screenX, screenY,
                     BLOCK_WIDTH, BLOCK_WIDTH, BLOCK_HEIGHT,
                     BM_TRANSPARENT, 0, 0);
    
    if (block.foreground) {
        fgInfo = fgBlocks[block.foreground - 1];
        gl_drawRect(screenX, screenY, BLOCK_WIDTH, BLOCK_HEIGHT, palettes[fgInfo.palette][0]);
        mrc_bitmapShowEx(fgInfo.image->bitmap, screenX, screenY,
                         BLOCK_WIDTH, BLOCK_WIDTH, BLOCK_HEIGHT,
                         BM_TRANSPARENT, 0, 0);
    }
}

static void drawPlayer(void) {
    drawBlock(player.x, player.y);
    mrc_bitmapShowEx(playerImages[player.direction]->bitmap,
                     SCREEN_WIDTH / 2 - BLOCK_WIDTH / 2,
                     SCREEN_HEIGHT / 2 - BLOCK_HEIGHT / 2,
                     BLOCK_WIDTH, BLOCK_WIDTH, BLOCK_HEIGHT,
                     BM_TRANSPARENT, 0, 0);
}

//绘制背包单个物品
static void drawInventoryItem(InventoryItem item, int x, int y) {
    char itemCount[4];
    int len;
    BlockInfo fgInfo;
    
    if (item.id < 1 || item.id > FG_BLOCK_COUNT) return;
    
    fgInfo = fgBlocks[item.id - 1];
    mrc_bitmapShowEx(fgInfo.image->bitmap, x + 3, y + 3,
                     BLOCK_WIDTH, 20, 20, BM_TRANSPARENT, 0, 0);
    
    if (item.count == 1) return;
    
    mrc_sprintf(itemCount, "%d", item.count);
    len = mrc_strlen(itemCount);
    _drawText(itemCount, x + 2, y + 26, 0, 0, 0, 0, 1);
}

//玩家周边有无前景方块
static int haveAdjacentBlock(int id) {
    int32 x, y;
    
    for (y = -1; y < 2; y++) {
        for (x = -1; x < 2; x++) {
            if (x == 0 && y == 0) continue;
            if (getBlock(player.x + x, player.y + y).foreground == id) {
                return 1;
            }
        }
    }
    return 0;
}

static void drawHUD(void) {
    const InventoryItem *selItem;
    
    /* 维度指示 */
    mrc_EffSetCon(0, 0, 40, 22, 128, 128, 128);
    if (player.dimension == 0) {
        _drawText("陆地", 3, 3, 255, 255, 255, 0, 1);
    } else {
        _drawText("洞穴", 3, 3, 255, 0, 0, 0, 1);
    }
    
    /* 选中物品框 */
    selItem = &player.items[player.selectedItem];
    gl_drawRect(0, 274, 36, 46, 0xFF000000);
    gl_drawRect(0, 276, 34, 44, 0xFFFFFFFF);
    
    /* 合成按钮 */
    if (haveAdjacentBlock(7)) {
        mrc_EffSetCon((SCRW - 32)/2, 300, 32, 20, 128, 128, 128);
        _drawText("合成", (SCRW - 32)/2, 300, 255, 255, 255, 0, 1);
    }
    
    if (selItem->count < 1) return;
    drawInventoryItem(*selItem, 4, 276);
}

static void drawSoftKeyHints(char* left, char* right) {
    int32 leftX, rightX, y;
    
    leftX = 10;
    y = SCRH - 20;
    
    if (left != NULL) {
        _drawText(left, leftX, y, 0, 0, 0, 0, 1);
    }
    
    if (right != NULL) {
        rightX = SCRW - _textWidth(right, 0) - 10;
        _drawText(right, rightX, y, 0, 0, 0, 0, 1);
    }
}

static void drawWorld(void) {
    int32 x, y;
    
    gl_clearScreen(135, 206, 235);  /* 天空蓝 */
    
    for (y = -7; y <= 7; y++) {
        for (x = -5; x <= 5; x++) {
            drawBlock(player.x + x, player.y + y);
        }
    }
    
    drawPlayer();
    drawHUD();
    drawSoftKeyHints(NULL, "返回");
    mrc_refreshScreen(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

//背包页面
static void drawInventory(void) {
    int x, y;
    InventoryItem item;
    char text[30] = {0};
    
    gl_clearScreen(255, 255, 255);
    
    item = player.items[inventoryY * 8 + inventoryX];
    if (item.id && item.count>0)
    {
        mrc_sprintf(text, "已选%s", itemname[item.id - 1]);
        _drawText(text, 3, 3, 0, 0, 0, 0, 1);
    }        
    
    for (y = 0; y < 4; y++) {
        for (x = 0; x < 8; x++) {
            if (inventoryX == x && inventoryY == y) {
                gl_drawRect(1 + x * 30, 81 + y * 43, 27, 40, 0xFFFFCCAA);
            } else {
                gl_drawRect(1 + x * 30, 81 + y * 43, 27, 40, 0xFFEEEEEE);
            }
            
            item = player.items[y * 8 + x];
            if (item.count < 1) continue;
            
            drawInventoryItem(item, x * 30, 80 + y * 43);
        }
    }
    
	drawSoftKeyHints(NULL,"返回");
    
    if (takeItem(6, 16, 1)) {
        _drawText("合成工作台", 3, 300, 0, 0, 0, 0, 1);
    }
    
    mrc_refreshScreen(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

//合成页面
static void drawCrafting(void) {
    int32 maxRecipe, x, y, i;
    Recipe recipe;
    char text[30] = {0};
    
    maxRecipe = 0;
    gl_clearScreen(135, 206, 235);
    
    while (recipes[maxRecipe].result.id) maxRecipe++;
    maxRecipe--;
    
    for (y = 0; y < 6; y++) {
        for (x = 0; x < 8; x++) {
            if (inventoryX == x && inventoryY == y) {
                gl_drawRect(1 + x * 30, 42 + y * 43, 27, 40, 0xFFFFCCAA);
            } else {
                gl_drawRect(1 + x * 30, 42 + y * 43, 27, 40, 0xFFEEEEEE);
            }
            
            if (y * 8 + x > maxRecipe) continue;
            
            drawInventoryItem(recipes[y * 8 + x].result, x * 30, 41 + y * 43);
        }
    }    
    
    if (inventoryY * 8 + inventoryX <= maxRecipe) {
        recipe = recipes[inventoryY * 8 + inventoryX];
        
        if (recipe.result.id)
        {
            mrc_sprintf(text, "%s 需要:", itemname[recipe.result.id - 1]);
            _drawText(text, 0, 10, 0, 0, 0, 0, 1);
        }
        
        for (i = 0; i < 4; i++) {
            if (!recipe.items[i].id) break;
            drawInventoryItem(recipe.items[i], 95 + 30 * i, 0);
        }
    }
        
    if (canCraft(recipe)) {        
        drawSoftKeyHints("可合成", "返回");
    } else {
        drawSoftKeyHints(NULL, "返回");
    }
  
    mrc_refreshScreen(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

static void drawMenu(void) {
    mrc_clearScreen(135, 206, 235);
    _drawText("我的世界", (SCRW - 70)/2, SCRH/6, 0, 0, 0, 0, 1);

	_drawText("玩家按1～9键挖/放相邻方块", 20, SCRH/3+20, 0, 0, 0, 0, 1);
	_drawText("建议合成镐，桥、梯子道具", 20, SCRH/3+40, 0, 0, 0, 0, 1);
	_drawText("挖石头获得洞口道具", 20, SCRH/3+60, 0, 0, 0, 0, 1);
	_drawText("放置洞口道具下入洞穴维度", 20, SCRH/3+80, 0, 0, 0, 0, 1);
	_drawText("放梯子道具可上到陆地维度", 20, SCRH/3+100, 0, 0, 0, 0, 1);	

    _drawText("重新开始", (SCRW - 70)/2, SCRH-20, 0, 0, 0, 0, 1);
    drawSoftKeyHints("开始","退出");
    mrc_refreshScreen(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

/*==============================================================================
 * 游戏逻辑
 *============================================================================*/
 
 // 检查指定位置在当前维度是否可站立
static int canStandAt(int x, int y, int dimension) {
    int oldDim = player.dimension;
	int canStand;
	Block block;

    player.dimension = dimension;
    
    block = getBlock(x, y);
    canStand = 1;
    
    if (block.foreground && !fgBlocks[block.foreground - 1].canStepOn) {
        canStand = 0;
    }
    if (block.background == 6 && block.foreground != 10) {
        canStand = 0;
    }
    
    player.dimension = oldDim;
    return canStand;
}

static void movePlayer(int dX, int dY) {
    int32 newX, newY;
    Block block;
    
    newX = player.x + dX;
    newY = player.y + dY;
    
    if (outOfBounds(newX, newY)) return;
    
    block = getBlock(newX, newY);
    
    // 移动可行性判断
    if (block.foreground && !fgBlocks[block.foreground - 1].canStepOn) return;
    if (block.background == 6 && block.foreground != 10) return;
    
    player.x = newX;
    player.y = newY;
    
    // 维度切换判断
    if (block.foreground == 11) {  // 走进洞口
        if (canStandAt(newX, newY, 1)) {  // 检查洞穴维度
            player.dimension = 1;
            toast("进入洞穴", 1000);
        } else {
            toast("无法进入：洞穴内此位置不可站立", 1000);
            // 可以选择不移回原位，或者移回
        }
    } else if (block.foreground == 13) { // 走进梯子
        if (canStandAt(newX, newY, 0)) {  // 检查陆地维度
            player.dimension = 0;
            toast("返回陆地", 1000);
        } else {
            toast("无法返回：陆地上此位置不可站立", 1500);
        }
    }
    
    drawWorld();
}

static int giveOneItem(InventoryItem *newItems, int id) {
    int s;
    
    for (s = 0; s < INVENTORY_SLOTS; s++) {
        if (newItems[s].id == id && newItems[s].count > 0 && newItems[s].count < 99) {
            newItems[s].count++;
            return 1;
        }
    }
    
    for (s = 0; s < INVENTORY_SLOTS; s++) {
        if (newItems[s].count < 1) {
            newItems[s].id = id;
            newItems[s].count = 1;
            if (newItems[player.selectedItem].count < 1) {
                player.selectedItem = s;
            }
            return 1;
        }
    }
    
    return 0;
}

static int giveItem(int id, int count, int dry) {
    InventoryItem newItems[INVENTORY_SLOTS];
    int i;
    
    mrc_memcpy(newItems, player.items, sizeof(newItems));
    
    for (i = 0; i < count; i++) {
        if (!giveOneItem(newItems, id)) return 0;
    }
    
    if (!dry) {
        mrc_memcpy(player.items, newItems, sizeof(newItems));
    }
    
    return 1;
}

static int takeOneItem(InventoryItem *newItems, int id) {
    int s;
    
    for (s = 0; s < INVENTORY_SLOTS; s++) {
        if (newItems[s].id == id && newItems[s].count > 0) {
            newItems[s].count--;
            return 1;
        }
    }
    
    return 0;
}

static int takeItem(int id, int count, int dry) {
    InventoryItem newItems[INVENTORY_SLOTS];
    int i;
    
    mrc_memcpy(newItems, player.items, sizeof(newItems));
    
    for (i = 0; i < count; i++) {
        if (!takeOneItem(newItems, id)) return 0;
    }
    
    if (!dry) {
        mrc_memcpy(player.items, newItems, sizeof(newItems));
    }
    
    return 1;
}

static void placeBlock(int dX, int dY) {
    InventoryItem selItem;
    int32 newX, newY;
    Block old_block;
    Block new_block;
    BlockInfo info;
    
    selItem = player.items[player.selectedItem];
    newX = player.x + dX;
    newY = player.y + dY;
    
	drawWorld();//玩家角度更新
    if (outOfBounds(newX, newY)) return;
    
    old_block = getBlock(newX, newY);
    
    if (old_block.foreground) {
        //挖方块
        
        info = fgBlocks[old_block.foreground - 1];
        if (info.requiresPickaxe && player.items[player.selectedItem].id != 8) return;
        
        if (giveItem(info.dropItem, info.dropItemCount, 0)) {
            new_block.background = old_block.background;
            new_block.foreground = 0;
            setBlock(newX, newY, new_block);
        }
        
    } else {
        /* 放置方块 */
        
        if (!fgBlocks[selItem.id - 1].canPlace) return;
        if (old_block.background == 6 && !fgBlocks[selItem.id - 1].canPlaceOnWater) return;
        
        if (selItem.count > 0) {
            new_block.background = old_block.background;
            new_block.foreground = selItem.id;
            setBlock(newX, newY, new_block);
            player.items[player.selectedItem].count--;
        }
        
    }
    
    drawWorld();
}

static int canCraft(Recipe recipe) {
    InventoryItem itemsCopy[INVENTORY_SLOTS];
    int i, c;
    
    mrc_memcpy(itemsCopy, player.items, sizeof(itemsCopy));
    
    for (i = 0; i < 4; i++) {
        if (!recipe.items[i].id) break;
        for (c = 0; c < recipe.items[i].count; c++) {
            if (!takeOneItem(itemsCopy, recipe.items[i].id)) return 0;
        }
    }
    
    return 1;
}

/*==============================================================================
 * 事件处理
 *============================================================================*/
//实现长按键效果
void timerkeyCB(int32 data)
{
    switch(data)
    {
        case _UP: 
            player.direction = DIR_NORTH; 
            movePlayer(0, -1); 
            break;
        case _DOWN: 
            player.direction = DIR_SOUTH; 
            movePlayer(0, 1); 
            break;
        case _LEFT: 
            player.direction = DIR_WEST; 
            movePlayer(-1, 0); 
            break;
        case _RIGHT: 
            player.direction = DIR_EAST; 
            movePlayer(1, 0); 
            break;
    }
}
 
static void handle_keyevt_ingame(int32 type, int32 keycode) {
    if (type == KY_DOWN)
    {
        switch (keycode) {
            case _UP: 
                player.direction = DIR_NORTH; 
                movePlayer(0, -1); 
                mrc_timerStart(timerkey, 200, keycode, timerkeyCB, 1);
                break;
            case _DOWN: 
                player.direction = DIR_SOUTH; 
                movePlayer(0, 1); 
                mrc_timerStart(timerkey, 200, keycode, timerkeyCB, 1);
                break;
            case _LEFT: 
                player.direction = DIR_WEST; 
                movePlayer(-1, 0); 
                mrc_timerStart(timerkey, 200, keycode, timerkeyCB, 1);
                break;
            case _RIGHT: 
                player.direction = DIR_EAST; 
                movePlayer(1, 0); 
                mrc_timerStart(timerkey, 200, keycode, timerkeyCB, 1);
                break;
            case _1: player.direction = DIR_NORTHWEST; placeBlock(-1, -1); break;
            case _2: player.direction = DIR_NORTH; placeBlock(0, -1); break;
            case _3: player.direction = DIR_NORTHEAST; placeBlock(1, -1); break;
            case _4: player.direction = DIR_WEST; placeBlock(-1, 0); break;
            case _6: player.direction = DIR_EAST; placeBlock(1, 0); break;
            case _7: player.direction = DIR_SOUTHWEST; placeBlock(-1, 1); break;
            case _8: player.direction = DIR_SOUTH; placeBlock(0, 1); break;
            case _9: player.direction = DIR_SOUTHEAST; placeBlock(1, 1); break;
            case _SLEFT: 
                setState(ST_INVENTORY); 
                return;
            case _SRIGHT: 
                save_world(0);
                
                if (world_file) 
                {
                    mrc_close(world_file);
                    world_file = 0;
                }
                if (player_file) 
                {    
                    mrc_close(player_file);
                    player_file = 0;
                }
                isgameing = 0;
                setState(ST_MENU);             
                return;
            case _SELECT:
                if (haveAdjacentBlock(7)) {
                    setState(ST_CRAFTING);
                    return;
                }
                break;
            default: break;
        }
    } else if (type == KY_UP)
    {
        if (keycode == _UP || keycode == _DOWN || keycode == _LEFT || keycode == _RIGHT)
            mrc_timerStop(timerkey);
    } 
}

static void handle_keyevt_inventory(int32 keycode) {
    switch (keycode) {
        case _UP: if (inventoryY > 0) inventoryY--; break;
        case _DOWN: if (inventoryY < 3) inventoryY++; break;
        case _LEFT: if (inventoryX > 0) inventoryX--; break;
        case _RIGHT: if (inventoryX < 7) inventoryX++; break;
        case _SELECT: player.selectedItem = inventoryY * 8 + inventoryX; break;
        case _SRIGHT: player.selectedItem = inventoryY * 8 + inventoryX; setState(ST_INGAME); return;
        case _SLEFT: if (takeItem(6, 16, 0)) giveItem(7, 1, 0); break;
        default: break;
    }
    drawInventory();
}

static void handle_keyevt_crafting(int32 keycode) {
    int idx, i;
    Recipe recipe;
    
    switch (keycode) {
        case _UP: if (inventoryY > 0) inventoryY--; break;
        case _DOWN: if (inventoryY < 5) inventoryY++; break;
        case _LEFT: if (inventoryX > 0) inventoryX--; break;
        case _RIGHT: if (inventoryX < 7) inventoryX++; break;
        case _SLEFT:
        case _SELECT:
            idx = inventoryY * 8 + inventoryX;
            recipe = recipes[idx];
            if (!recipe.result.id || !canCraft(recipe)) break;
            for (i = 0; i < 4; i++) {
                if (!recipe.items[i].id) break;
                takeItem(recipe.items[i].id, recipe.items[i].count, 0);
            }
            giveItem(recipe.result.id, recipe.result.count, 0);
            toast("合成成功", 800);
            break;
        case _SRIGHT: setState(ST_INGAME); return;
        default: break;
    }
    drawCrafting();
}

void drawConfirmResetGame()
{
    mrc_clearScreen(135, 206, 235);
    _drawText("提示", (SCRW - 34)/2, SCRH/6, 0, 0, 0, 0, 1);

	_drawText("确认将重置世界地图，背包", 20, SCRH/3+20, 0, 0, 0, 0, 1);
	_drawText("您考虑一下吧", (SCRW - 100)/2, SCRH/3+40, 0, 0, 0, 0, 1);

    _drawText("原作: gtrxAC", (SCRW - 100)/2, SCRH/3+80, 0, 0, 0, 0, 1);
    _drawText("sohehe4.ysepan.com", (SCRW - 150)/2, SCRH/3+100, 0, 0, 0, 0, 1);
    drawSoftKeyHints("重置","返回");
    mrc_refreshScreen(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

static void setState(State newState) {
    state = newState;
    
    switch (state) {
        case ST_INVENTORY:
            inventoryX = player.selectedItem % 8;
            inventoryY = player.selectedItem / 8;
            drawInventory();
            break;
        case ST_CRAFTING:
            inventoryX = 0;
            inventoryY = 0;
            drawCrafting();
            break;
        case ST_INGAME:
            if (isgameing == 0)
            {
                initGlobalVars();
                initFiles();
                isgameing = 1;
            }
			drawWorld();
            break;
        case ST_MENU:
            drawMenu();
            break;
        case ST_CONFIRMRESET:
            drawConfirmResetGame();
            break;
        default:
            break;
    }
}

/*==============================================================================
 * 平台必需函数
 *============================================================================*/
int32 mrc_init(void) {
    mpc_init();
    FontModuleInit(TRUE, TRUE, TRUE);
        
    loadAllBmpResources();
    
    /* 创建定时器 */
    timerkey = mrc_timerCreate();
    
    setState(ST_MENU);
    return 0;
}

int32 mrc_event(int32 code, int32 param0, int32 param1) {
    if (code == KY_DOWN) {
        switch (state) {
            case ST_INGAME: handle_keyevt_ingame(code, param0); break;
            case ST_INVENTORY: handle_keyevt_inventory(param0); break;
            case ST_CRAFTING: handle_keyevt_crafting(param0); break;
            case ST_MENU:
                if (param0 == _SLEFT) {
                    setState(ST_INGAME);
                } else if (param0 == _SRIGHT) {
                    mrc_exit();
                } else if (param0 == _SELECT)
                    setState(ST_CONFIRMRESET);
                break;
            case ST_CONFIRMRESET:
                if (param0 == _SLEFT) {
                    mrc_remove("mtmine\\world.bin");
                    mrc_remove("mtmine\\player.bin");
                    toast("重置成功", 800);
                    setState(ST_MENU);
                } else if (param0 == _SRIGHT) {
                    setState(ST_MENU);
                }
                break;
            default:
                break;
        }
    } else if (code == KY_UP)
    {
        if (state == ST_INGAME)
            handle_keyevt_ingame(code, param0);
    } else if (code == MS_DOWN) {
        if (isRightKey(param0, param1)) {
            mrc_event(KY_DOWN, _SRIGHT, 0);
        } else if (isLeftKey(param0, param1)) {
            mrc_event(KY_DOWN, _SLEFT, 0);
        } else if (isMiddleKey(param0, param1)) {
            mrc_event(KY_DOWN, _SELECT, 0);
        }
    }
    return 0;
}

int32 mrc_pause(void) {
    if (state == ST_INGAME) {
        save_world(0);
    }
    return 0;
}

int32 mrc_resume(void) {
    if (state == ST_INGAME) {
    } else if (state == ST_MENU) {
        drawMenu();
    } else if (state == ST_INVENTORY) {
        drawInventory();
    } else if (state == ST_CRAFTING) {
        drawCrafting();
    }
    return 0;
}

int32 mrc_exitApp(void) {
    
    save_world(0);
    
    if (world_file) 
    {
        mrc_close(world_file);
        world_file = 0;
    }
    if (player_file) 
    {    
        mrc_close(player_file);
        player_file = 0;
    }
    
    if (timerkey)
    {
        mrc_timerStop(timerkey);
        mrc_timerDelete(timerkey);
        timerkey = 0;
    }
    releaseAllBmpResources();
    FontModuleRelease();    
    
    return 0;
}

int32 mrc_extRecvAppEvent(int32 app, int32 code, int32 param0, int32 param1) {
    return 0;
}

int32 mrc_extRecvAppEventEx(int32 code, int32 p0, int32 p1, int32 p2, int32 p3, int32 p4, int32 p5) {
    return 0;
}