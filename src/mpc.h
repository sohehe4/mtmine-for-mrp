/*
** Copyright 2012-03-08 zengming00

** Version: 1.4-20260119
   增加三角函数mytan和myatan

** Version: 1.3-20251223
   toast绘制文字改用系统文字，不依赖16字库

   增加mysin, mycos三角函数，整型查表法原理

** Version: 1.2-20240821
   优化drawrect提速
** Version: 1.1-20240812

   优化drawpoint与混色公式提速
**
** 注：“冒泡开发实验系统”以下简称mpc
** 为方便mpc中创建的源程序在计算机中调试或生成而创建
** 此文件只适用于核心版本号为1001的“冒泡开发实验系统”所生成的源代码
**
** 移植步骤：
**     1.整理所有源程序中的include和文件路径， 同时将"base.h"、"sound.h"、"ctype.h"删除
**     2.添加 【#include "mpc.h"】，添加mpc.c文件
**     3.将timerstart的第四个参数去除双引号，如:timerstart(timer,100,p1,"updown",1);
**         改为timerstart(timer,100,p1,updown,1);
**     4.在源程序init入口处添加 【mpc_init();】注意，必须最先在init函数中调用
**     5.在源程序exitapp处添加 【mpc_exit();】注意，必须最后在exitapp函数中调用
**     int32 exitapp()
       {
         mpc_exit();
         return 0;
	   }
**
** 常见问题：
**     1.mpc中所有宏和函数等都是全局的，因此当手机开发者的源码不遵守游戏规则时编译将会出现大量错误
**        这时就需要移植者手工整理这些宏、函数。。。的声明（--!）
**     2.手机开发者在源码中使用了只有mpc才支持的脚本模式（类似bat脚本的功能），使得编译出现语法错误
**        这些脚本的功能可以看作是初始化功能，因为在mpc中首先被执行的并不是init函数而是这些脚本，所
**        以，只要把这些脚本放到init函数中即可
**     3.局部变量或全局变量未初始化，mpc中所有变量都会被初始化为0，当手机开发者没有注意到这一点时，会给
**        移植带来很大的麻烦
**     4.类型不一致，导致mrpbuilder失败，“#define open mrc_open ”可以看出，mpc与API之间几乎是无差别的
**          当出现大量类型不一致时，可以修改宏定义使其进行强制类型转换(参考u2c)
**     5.局部变量定义的问题，mpc支持不在语句块开头处定义变量的功能（C99)，这一功能使得程序变得易读、方便，但是
**         这会导致一些编译器无法编译，此时需要将这些变量提升到语句块的头部
**
** 其他建议：
**     手机开发者应尽量遵守C语言（C89）规范，过度使用mpc特有功能导致的后果是得不偿失的
*/

#ifndef _MPC_H__
#define _MPC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mrc_base.h"
#include "mrc_exb.h"
#include "mrc_sound.h"
#include "mrc_bmp.h"
#include "mrc_android.h"
#include "mrc_graphics.h"

#define BMP_HEAD "\x42\x4D\x42\xC8\x00\x00\x00\x00\x00\x00\x46\x00\x00\x00\x38\x00\x00\x00\xA0\x00\x00\x00\xA0\x00\x00\x00\x01\x00\x10\x00\x03\x00\x00\x00\x00\xC8\x00\x00\xA0\x0F\x00\x00\xA0\x0F\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" //54字节
#define BMP_HEAD_MTK "\x00\xF8\x00\x00\xE0\x07\x00\x00\x1F\x00\x00\x00\x00\x00\x00\x00" //16字节

#define _VERSION 1001  //“冒泡开发实验系统”内部核心版本号

//将32位的COLOR转16位COLOR
#define MAKECOLOR565(color) ((uint16)((color>>8)&0xf800) |  (uint16)((color>>5)&0x07e0) | (uint16)((color>>3)&0x1f))


/* 从16位颜色值中提取R分量（5位，0-31） */
#define GET_R565(color) (uint8)( (((color) >> 11) & 0x1F) << 3 )

/* 从16位颜色值中提取G分量（6位，0-63） */
#define GET_G565(color) (uint8)( (((color) >> 5)  & 0x3F) << 2 )

/* 从16位颜色值中提取B分量（5位，0-31） */
#define GET_B565(color) (uint8)( ((color) & 0x1F) << 3 )

/*
#ifndef size_t
typedef uint32 size_t;
#endif
#ifndef ptrdiff_t
typedef int32 ptrdiff_t;
#endif
#ifndef intptr_t
typedef int32 intptr_t;
#endif
*/
#ifndef MAX
    #define  MAX( x, y ) ( ((x) > (y)) ? (x) : (y) )
#endif
#ifndef MIN
    #define  MIN( x, y ) ( ((x) < (y)) ? (x) : (y) )
#endif
#ifndef ABS
    #define ABS(VAL) (((VAL)>0)?(VAL):(-(VAL)))
#endif

//ctype
#define isalpha(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define isdigit(c) ((c) >= '0' && (c) <= '9')
#define isalnum(c) (isalpha(c) || isdigit(c))
#define isprint(ch) ((ch) >= 0x20 && (ch) <= 0x7e)
#define tolower(c) ((c) |= 0x20)
#define toupper(c) ((c) &= 0xdf)


//base

extern int16  SCRW;//屏幕宽
extern int16  SCRH;//屏幕高
extern int16  FONTW;//手机中等字库字体的宽
extern int16  FONTH;//手机中等字库字体的高

enum
{
	SHADE_UPDOWN,		//从上到下
	SHADE_LEFTRIGHT,	//从左到右
	SHADE_DOWNUP,		//从下到上
	SHADE_RIGHTLEFT		//从右到左
};

//基本按键值（未定义的其他按键也可以使用，但需知道其键值）
enum
{
  _0,           //按键 0
  _1,           //按键 1
  _2,           //按键 2
  _3,           //按键 3
  _4,           //按键 4
  _5,           //按键 5
  _6,           //按键 6
  _7,           //按键 7
  _8,           //按键 8
  _9,           //按键 9
  _STAR,        //按键 *
  _POUND,       //按键 #
  _UP,          //按键 上
  _DOWN,        //按键 下
  _LEFT,        //按键 左
  _RIGHT,       //按键 右
  _SLEFT=17,    //按键 左软键
  _SRIGHT,      //按键 右软键
  _SEND,        //按键 接听键
  _SELECT       //按键 确认/选择（若方向键中间有确认键，建议设为该键）
};

//基本事件（其他事件需自己定义）
enum
{
  KY_DOWN, 	 //按键按下
  KY_UP,       //按键释放
  MS_DOWN, 	 //鼠标按下
  MS_UP, 	     //鼠标释放
  MN_SLT, //菜单选择
  MN_RET, //菜单返回
  MR_DIALOG, //对话框
  MS_MOVE=12   //鼠标移动
};

enum
{
  DLG_OK,         //对话框/文本框等的"确定"键被点击(选择)
  DLG_CANCEL  //对话框/文本框等的"取消"("返回")键被点击(选择)
};

enum
{
  SEEK_SET,             //从文件起始开始
  SEEK_CUR,             //从当前位置开始
  SEEK_END             //从文件末尾开始
};
enum
{
  IS_FILE=1,      //文件
  IS_DIR=2,      //目录
  IS_INVALID=8  //无效(非文件、非目录)
};

//sound
enum {_MIDI=1,_WAVE,_MP3,_AMR,_PCM,_M4A,_AMR_WB};

typedef struct
{
  uint16            x;
  uint16            y;
  uint16            w;
  uint16            h;
} rectst;

typedef struct
{
  uint8            r;
  uint8            g;
  uint8            b;
} colorst;


/********************************C库函数********************************/

#define  printf(...)

#define strrchr mrc_strrchr

#define wstrlen mrc_wstrlen


/*******************************框架函数*********************************/

#define init            MRC_EXT_INIT
#define exitapp         MRC_EXT_EXIT
#define event           mrc_appEvent
#define pause           mrc_appPause
#define resume          mrc_appResume

//////////////////////以上为平台必需实现的函数////////////////////////

/********************************文件接口********************************/

#define open mrc_open

#define close mrc_close

#define filestate mrc_fileState

#define write mrc_write

#define read mrc_read

#define seek mrc_seek

#define getlen mrc_getLen

#define remove mrc_remove

#define rename mrc_rename

#define mkdir mrc_mkDir

#define rmdir mrc_rmDir

#define findstart mrc_findStart

#define findnext mrc_findGetNext

#define findstop mrc_findStop

/********************************绘图接口********************************/

#define dtext mrc_drawText
#define dtextex( pcText,  x,  y, rect, color,  flag,  font)\
    mrc_drawTextEx(pcText,x, y, *(mr_screenRectSt*)rect,*(mr_colourSt*)color, flag, font)

#define dtextright( pcText,  x,  y, rect, color,  flag,  font)\
    mrc_drawTextRight(pcText,x, y, *(mr_screenRectSt*)rect,*(mr_colourSt*)color, flag, font)

#define c2u(cp, err, size) mrc_c2u((char *)cp, (int32 *)err, (int32 *)size)

#define textwh(pcText, is_unicode, font,  w,  h)\
    mrc_textWidthHeight((char*) pcText, (int) is_unicode, (uint16) font, (int32*) w, (int32*) h)

#define unitextrow  mrc_unicodeTextRow

#define drect mrc_drawRect

#define dline mrc_drawLine

#define dpoint mrc_drawPoint

#define dpointex mrc_drawPointEx

#define cls mrc_clearScreen

#define ref mrc_refreshScreen

#define effsetcon mrc_EffSetCon

/********************************声音接口********************************/

#define  soundinit       mrc_playSoundExInit
#define soundloadfile       mrc_playSoundExLoadFile
#define soundplay( type,  block,  loop) mrc_playSoundEx(type,block,loop,NULL)
#define soundpause       mrc_pauseSoundEx
#define soundresume        mrc_resumeSoundEx
#define soundstop         mrc_stopSoundEx
#define soundclose         mrc_closeSoundEx
#define setvolume        mrc_setVolume
#define getsoundtotaltime        mrc_getSoundTotalTime
#define getsoundcurtime          mrc_getSoundCurTime
#define getsoundcurtimems        mrc_getSoundCurTimeMs
#define setplaypos( type, pos)         mrc_setPlayPos(type,*(T_DSM_AUDIO_POS*)pos)
#define setplaytime( type, pos)         mrc_setPlayTime(type,*(T_DSM_AUDIO_POS*)pos)
#define getdevicestate        mrc_getDeviceState

/********************************本地化UI接口********************************/

#define menucreate mrc_menuCreate

#define menuset mrc_menuSetItem

#define menushow mrc_menuShow

#define menudel mrc_menuRelease

#define menuref mrc_menuRefresh

#define dlgcreate mrc_dialogCreate

#define dlgdel mrc_dialogRelease
#define dlgref mrc_dialogRefresh
#define textcreate mrc_textCreate
#define textdel mrc_textRelease
#define textref mrc_textRefresh
#define editcreate mrc_editCreate
#define editdel mrc_editRelease

#define editget mrc_editGetText


/********************************网络接口********************************/

#define initnetwork mrc_initNetwork
#define gethostbyname mrc_getHostByName
#define socket mrc_socket
#define connect mrc_connect
#define getsocketstate mrc_getSocketState
#define send mrc_send
#define recv mrc_recv
#define closesocket mrc_closeSocket
#define closenetwork mrc_closeNetwork

extern void mrc_connectWAP(char* wap);
#define wap mrc_connectWAP

/********************************其他接口********************************/
#define exit  mrc_exit

#define getuptime mrc_getUptime

#define getdatetime mrc_getDatetime
#define getmemremain mrc_getMemoryRemain
#define shake mrc_startShake
#define getsysmem mrc_getSysMem

#define readfilefrommrp mrc_readFileFromMrpEx

#define freefiledata mrc_freeFileData
#define freeorigin mrc_freeOrigin

#define u2c( input,  input_len, output,  output_len)\
    mrc_unicodeToGb2312((uint8*) input, (int32) input_len, (uint8**) output, (int32*) output_len)

int32 mrc_runMrp(char* mrp_name,char* file_name,char* parameter);
#define  runmrp( mrp_name, filename) mrc_runMrp( mrp_name, filename,NULL)

#define getparentpath mrc_GetParentPath

#define sand mrc_sand
#define srand mrc_sand
#define time(x) mrc_getUptime()
#define rand mrc_rand
extern int32 mrc_sendSms(char* pNumber, char*pContent, int32 flags);
#define sms mrc_sendSms
#define lcd mrc_LCDCanSleep
extern int32 mrc_sleep(uint32 ms);
#define sleep mrc_sleep

// 以下函数定义在mrc_bmp.h文件中//////////////////////////////////////


#define getscrbuf w_getScreenBuffer
#define setscrbuf w_setScreenBuffer
#define getscrsize mrc_getScreenSize
#define setscrsize mrc_setScreenSize

#define BM_TRANS BM_TRANSPARENT

#define bmpshowflip mrc_bitmapShowFlip

/********************************定时器接口********************************/

#define timercreate mrc_timerCreate

#define timerdel mrc_timerDelete

#define timerstop mrc_timerStop

#define timerstart mrc_timerStart

#define timersettime mrc_timerSetTimeEx

/*****************************以下为mpc扩展接口****************************/

void mpc_init(void);//加载本库

void mpc_exit(void);//退出卸载

#define img DrawIMG

#define shaderect DrawShadeRect

#define readall my_readAllEx

#define isPointCollRect isMouseInArea

int32 my_readAllEx(char* filename, char* buf, int32 *len);//读取文件len长度到buf，

int DrawIMG(char* filename, int x,int y);//调试无效果，真机会调用系统接口绘制图片

int stopDrawIMGif(void);//如果使用过上面的函数打开过GIF，不需要时用此函数关闭GIF

void DrawShadeRect(int16 x, int16 y, int16 w, int16 h, uint32 pixelA, uint32 pixelB, int8 mode);//产生渐变色

void DrawEffRect(int16 x, int16 y, int16 w, int16 h, char r, char g, char b);//画透明圆角矩形

//若需要r,g,b分量，使用 GET_R565, GET_G565, GET_B565
uint16 getPixel(int32 x, int32 y);  //获取屏幕某个坐标像素的16位RGB565值

void drawPoint(int x,int y,uint32 color);//画点,含混色影响效率

void drawCir(int32 x,int32 y,int32 r,uint32 color);//画圆,含混色影响效率

void drawRect(int32 x,int32 y,int32 w,int32 h,uint32 color);//画矩形，含混色影响效率,需求效率建议用下面的mydrawRect或SDK原api

void mydrawRect(int32 x,int32 y,int32 w,int32 h,uint32 color);//画矩形

//void *readFileFromAssets(const char *filename, int32 *len);//读取MRP包内的资源，返回内容地址

/*
 * 整数平方根函数（使用牛顿迭代法）
 * 输入: n - 非负整数
 * 返回: 整数
 */
int32 int_sqrt(int32 n);

int32 getLineSize(int x,int y, int x2,int y2);//获取两点之间的长度

void toast(char *Msg, int sp);//toast消息，sp为毫秒，请参阅《斯凯Mythroad 单机MRP开发SDK发布说明_AF01.32X2》字库插件章节

/*以下三角函数的参数填角度，官方原版是弧度
返回值扩了4096倍，假如返回值在算式里做乘数，后面要除以4096
                  假如做除数，  要乘4096
*/

int mysin(int x);

int mycos(int x);

/*计算给定角度的正切值
参数x: 角度值（整数，单位：度）
返回值: 正切值 × 4096  */
long int mytan(int x);

/*计算给定正切值对应的角度
参数value: 正切值（tanθ）x 4096
返回: -90° 到 90° 之间的整数角度，不是弧度值*/
int myatan(long int value);

void toSpin(int px,int py,int rx,int ry,int r,int* x,int* y);//用来获得点（rx，ry）绕点（px，py）旋转r度后的坐标（x，y）

void SetRootPath(void);//设置平台根目录为T卡，（平台默认根目录为T卡mythroad，如要访问T卡根目录，例如歌曲，则需使用本接口）

void app_printf(const char *format, ...);

char* ExtractFileExt(const char* name);//获取扩展名，内存需要由调用者释放

void showlogo(char* filename,int32 w,int32 h);//显示启动图，需打包进mrp

int PrintScr(int x,int y,int w,int h,int max_w,char* filename);/*(参数分别是截图点坐标x,y,截图宽高wh,手机屏幕宽度,如果filename不为空，则使用)*/

//默认默认2字宽，1字高
//触屏了“右软键”区域
extern int isRightKey(int32 x,int32 y);
//触屏了“左软键”区域
extern int isLeftKey(int32 x,int32 y);
//触屏了中部“OK”键区域
extern int isMiddleKey(int32 x,int32 y);
//点击坐标位于某个矩形区域
extern int isMouseInArea(int32 x,int32 y,int32 x1,int32 y1,int32 w,int32 h);

typedef struct {
	uint16* bitmap;
	int32 filelen; 
} mrpfileSt;
/*
加载例子
mrpfileSt dino_bmp
loadmrpfile("dino.bmp",&dino_bmp);
*/
void loadmrpfile(char* filename,mrpfileSt* filest);
/*
释放例子
mrpfileSt dino_bmp
freemrpfile(&dino_bmp);
*/
void freemrpfile(mrpfileSt* filest);

/*由于rect的全局变量不能赋值，
特创立此函数赋值方便
例：声明全局变量 mr_screenRectSt rect; 
initRect(&rect, 0, 0, SCRW, SCRH);
*/
void initRect(mr_screenRectSt* rect,uint16 x,uint16 y,uint16 w,uint16 h);

#ifdef __cplusplus
}
#endif


#endif