#include "mpc.h"
#include "mrc_image.h"
//#include "sky_font16.h"
#include <stdarg.h>


extern int32 mrc_plat(int32 code, int32 param);
typedef void (*MR_PLAT_EX_CB)(uint8* output, int32 output_len);
extern int32 mrc_platEx(int32 code, uint8* input, int32 input_len, uint8** output, int32* output_len, MR_PLAT_EX_CB *cb);

PT_DSM_COMMON_RSP gp;

int16 SCRW;
int16 SCRH;
int16 FONTW;
int16 FONTH;
static uint16* pScreenBuf;
static uint16* pScreenBuf0;
static uint16* pScreenBuf1;
static int32 timeH,stw, sth;
static int16 tx, ty, tw, th,ty2,isToast;
static int DSIN[91],DCOS[91];
static long int DTAN[91];

char* ExtractFileExt(const char* name)
{
	int32 len;
	char* extname, *position;

	/** “.” 的ASCII值为: 46 */
	position = (char*)mrc_strrchr(name, '.'); 
	if(position)
		len = mrc_strlen(position);
	else
		return (char*)mrc_malloc(1);  //如果文件没有后缀，返回一个字节防止出错

	extname = (char*)mrc_malloc(len+1);
	mrc_memset(extname, 0, len+1);
	mrc_strcpy(extname, position + 1); 

	return extname;
}

void mpc_init(void)
{
  int i;
  int32 fw,fh;
  mr_screeninfo screen;
  static int DSIN_TABLE[91] = {0,71,142,214,285,356,428,499,570,640,711,781,851,921,990,1060,1128,1197,1265,1333,1400,1467,1534,1600,1665,1731,1795,1859,1922,1985,2047,2109,2170,2230,2290,2349,2407,2465,2521,2577,2632,2687,2740,2793,2845,2896,2946,2995,3043,3091,3137,3183,3227,3271,3313,3355,3395,3435,3473,3510,3547,3582,3616,3649,3681,3712,3741,3770,3797,3823,3848,3872,3895,3917,3937,3956,3974,3991,4006,4020,4033,4045,4056,4065,4073,4080,4086,4090,4093,4095,4096};
  static int DCOS_TABLE[91] = {4096,4095,4093,4090,4086,4080,4073,4065,4056,4045,4033,4020,4006,3991,3974,3956,3937,3917,3895,3872,3848,3823,3797,3770,3741,3712,3681,3649,3616,3582,3547,3510,3473,3435,3395,3355,3313,3271,3227,3183,3137,3091,3043,2995,2946,2896,2845,2793,2740,2687,2632,2577,2521,2465,2407,2349,2290,2230,2170,2109,2048,1985,1922,1859,1795,1731,1666,1600,1534,1467,1400,1333,1265,1197,1129,1060,990,921,851,781,711,640,570,499,428,357,285,214,143,71,0};
 static long int DTAN_TABLE[91] = {0,71,143,214,286,358,430,502,575,648,722,796,870,945,1021,1097,1174,1252,1330,1410,1490,1572,1654,1738,1823,1909,1997,2086,2177,2270,2364,2461,2559,2659,2762,2868,2975,3086,3200,3316,3436,3560,3687,3819,3955,4095,4241,4392,4548,4711,4881,5058,5242,5435,5637,5849,6072,6307,6554,6816,7094,7389,7703,8038,8397,8783,9199,9649,10137,10670,11253,11895,12605,13396,14283,15285,16427,17740,19268,21070,23227,25858,29141,33355,38965,46809,58562,78132,117240,234442,250875938};

  for(i = 0;i<91;i++)
  {
	  DSIN[i]=DSIN_TABLE[i];
	  DCOS[i]=DCOS_TABLE[i];
	  DTAN[i]=DTAN_TABLE[i];
  }
  mrc_getScreenInfo(&screen);
  SCRW = (uint16)screen.width;
  SCRH = (uint16)screen.height;
  mrc_textWidthHeight("鼎",0,MR_FONT_MEDIUM,&fw,&fh);
  FONTW = (int16)fw;
  FONTH = (int16)fh;
  pScreenBuf0=getscrbuf();
}

void mpc_exit(void)
{
	if (pScreenBuf!=NULL)
	{
		free(pScreenBuf);
		pScreenBuf=NULL;
	}
	if (pScreenBuf1!=NULL)
	{
		free(pScreenBuf1);
		pScreenBuf1=NULL;
	}
}
/*
int32 MRC_EXT_EXIT(void)
{

  return MR_SUCCESS;
}
*/


//混色，屏幕取色与ARGB，返回16位的565
uint16 getColor(uint16 color1, uint32 color2)
{

  int r1 = GET_R565(color1);
  int g1 = GET_G565(color1);
  int b1 = GET_B565(color1);
  
  int r2 = (color2>>16)&0xff;
  int g2 = (color2>>8)&0xff;
  int b2 = color2&0xff;

  int draw_a = (color2>>24)&0xff;
  
  int r = r1 * (255-draw_a)/255 + r2 * draw_a/255;
  int g = g1 * (255-draw_a)/255 + g2 * draw_a/255;
  int b = b1 * (255-draw_a)/255 + b2 * draw_a/255;
  
  return ((uint16)((r) & 0xF8) << 8) | ((uint16)((g) & 0xFC) << 3)|((uint16)((b) >> 3) & 0x1F);
}

uint16 getPixel(int32 x, int32 y)
{
    return *(pScreenBuf0+ (SCRW*y+x));
}

//画点 参数：x, y, argb
void drawPoint(int x,int y,uint32 color)
{
	static int32 lastcolor1=0;
	static uint32 lastcolor2=0;
	static int32 lastocolor=0;

    uint16 o_color=0;
    uint16 scr_color16=0;
    //获取屏幕颜色
    if(x<0 || x>=SCRW)return;
    if(y<0 || y>=SCRH)return;
    scr_color16 = getPixel(x, y);

  //混合

  if (scr_color16!=lastcolor1||color!=lastcolor2)
  {
	  o_color = getColor(scr_color16, color);
	  lastcolor1=scr_color16;
	  lastcolor2=color;
	  lastocolor=o_color;
  }

  mrc_drawPoint(x,y, lastocolor);
  
}

/**
 * 整数平方根函数（使用牛顿迭代法）
 * 输入: n - 非负整数
 * 返回: floor(sqrt(n))
 */
int32 int_sqrt(int32 n) {
    int32 x, x1;

    if (n <= 0) return 0;
    if (n == 1) return 1;

    /* 初始估计值：使用位移找到最接近的2的幂 */
    x = n;
    x1 = (x + 1) >> 1;

    /* 牛顿迭代：x(n+1) = (x(n) + n/x(n)) / 2 */
    while (x1 < x) {
        x = x1;
        x1 = (x + n / x) >> 1;
    }

    return x;
}

//获取两点之间的长度 的平方
int32 getLineSize(int x,int y, int x2,int y2)
{
  int32 ret;
  ret = (x2-x)*(x2-x) + (y2-y)*(y2-y);
  
  return int_sqrt(ret);
}

void drawCir(int32 x,int32 y,int32 r,uint32 color)
{
	int ymax,xmax,ymin,xmin;
  int ix,iy;

  ymax=MIN(SCRH,y+r);
  xmax=MIN(SCRW,x+r);
  ymin=MAX(0,y-r);
  xmin=MAX(0,x-r);

  for(ix=xmin; ix<xmax; ix++)
  {
    for(iy=ymin; iy<ymax; iy++)
    {
      if(getLineSize(ix,iy, x,y)<= r)
      {
        //考虑效率问题，不透明的圆单独处理

        if(color>>24==0xff)
        {
          mrc_drawPoint(ix,iy, MAKECOLOR565(color));
        }
        else
        {
          drawPoint(ix,iy, color);
        }
      }
    }
  }
//printf("color>>24 = %d\n",color>>24);
}

void drawRect(int32 x,int32 y,int32 w,int32 h,uint32 color)
{
	int ymax,xmax,ymin,xmin;
  int ix,iy;

  ymax=MIN(SCRH,y+h);
  xmax=MIN(SCRW,x+w);
  ymin=MAX(0,y);
  xmin=MAX(0,x);

  for(ix=xmin; ix<xmax; ix++)
  {
    for(iy=ymin; iy<ymax; iy++)
    {
      drawPoint(ix,iy,color);
    }
  }
}

void mydrawRect(int32 x,int32 y,int32 w,int32 h,uint32 color)
{
	int r = (color>>16)&0xff;
	int g = (color>>8)&0xff;
	int b = color&0xff;
    mrc_drawRect(x,y,w,h,r,g,b);
}


void ShadeRect(int x, int y, int w, int h, int AR, int AG, int AB, int BR, int BG, int BB, int mode)
{
  int16 i,j,t;

  BR-=AR;
  BG-=AG;
  BB-=AB;
  switch(mode)
  {
  case SHADE_UPDOWN:
    t=x+w-1;
    for(i=0;     i<h;     i++)
      mrc_drawLine(x, y+i, t, y+i, (uint8)(AR+BR*i/h),(uint8)(AG+BG*i/h),(uint8)(AB+BB*i/h));
    return;
  case SHADE_DOWNUP:
    t=x+w-1;
    for(i=h-1,j=0;    i>=0;    i--,j++)
      mrc_drawLine(x, y+i, t, y+i, (uint8)(AR+BR*j/h),(uint8)(AG+BG*j/h),(uint8)(AB+BB*j/h));
    return;
  case SHADE_LEFTRIGHT:
    t=y+h-1;
    for(i=0;     i<w;     i++)
      mrc_drawLine(x+i, y, x+i, t, (uint8)(AR+BR*i/w),(uint8)(AG+BG*i/w),(uint8)(AB+BB*i/w));
    return;
  case SHADE_RIGHTLEFT:
    t=y+h-1;
    for(i=w-1,j=0;    i>=0;    i--,j++)
      mrc_drawLine(x+i, y, x+i, t, (uint8)(AR+BR*j/w),(uint8)(AG+BG*j/w),(uint8)(AB+BB*j/w));
    return;
  }
}


void DrawEffRect(int16 x, int16 y, int16 w, int16 h, char r, char g, char b)
{
  effsetcon(x+1, y, w-2, h, r, g, b);
  effsetcon(x, y+1, w, h-2, r, g, b);
}

void toastCB(int32 data)
{
//  uint32 i,addPos=0;
//  memset(getscrbuf(),0,SCRW*SCRH*2);
//  memcpy(getscrbuf(),pScreenBuf,SCRW*SCRH*2);
/*  for (i=y2+21; i>=y2; i--)
  {
    memcpy(pScreenBuf0+i*SCRW,pScreenBuf+addPos,SCRW*2);
    addPos+=SCRW*2;
  }
  */
  mrc_bitmapShowFlip(pScreenBuf,0,ty2,SCRW,SCRW,44,BM_COPY|TRANS_MIRROR_ROT180,0,0,MAKERGB(0,0,0));
  mrc_bitmapShowFlip(pScreenBuf1,SCRW/2,ty2,SCRW,SCRW,44,BM_COPY|TRANS_MIRROR_ROT180,0,0,MAKERGB(0,0,0));
  ref(tx-5, ty-5, tw+10, th+10);
  timerstop(timeH);
  timerdel(timeH);
  timeH=0;
  isToast=0;

}

void toast(char *Msg, int sp)
{
  uint32 i,addPos=0;

  mr_colourSt clr1 = { 248, 247, 247};
  mr_screenRectSt rect1 = {0,0, 240, 320};
  int32 errIndex,unStrLen;
  uint16* uMsg;

  ty = SCRH/4 * 3;
  ty2=ty-5;
//  pScreenBuf0=getscrbuf();
  if(pScreenBuf==NULL)
    pScreenBuf=malloc(SCRW*44*2);
  if(pScreenBuf1==NULL)
	  pScreenBuf1=malloc(SCRW*44*2);
  if (pScreenBuf!=NULL&&pScreenBuf1!=NULL)
  {
    //判断不在toast时再复制屏幕
    if(isToast==0)
    {
      memset(pScreenBuf,0,SCRW*44*2);
	  memset(pScreenBuf1,0,SCRW*44*2);
//		memcpy(pScreenBuf,getscrbuf(),SCRW*SCRH*2);
      for (i=ty2+43; i>=ty2; i--)
      {
        memcpy(pScreenBuf+addPos,pScreenBuf0+i*SCRW,SCRW);
		memcpy(pScreenBuf1+addPos,pScreenBuf0+i*SCRW+SCRW/2,SCRW);
        addPos+=SCRW;
      }
      isToast=1;
    }
	uMsg=c2u(Msg,&errIndex,&unStrLen);
//    textwh(Msg, FALSE, 0, &tw, &th);
//    mrc_skyfont_textWidthHeight((char*)uMsg,0,-1,0,0,rect1,SKYFONT_SUPPORT_HALF_CHAR,&stw,&sth);
	mrc_textWidthHeight((char*)uMsg,1,1,&stw,&sth);
	tw=(int16)stw;
	th=(int16)sth;
    tx=(tw < SCRW)?(SCRW-tw) >> 1:0;
    DrawEffRect(tx-5, ty-5, tw+10, th+10, 128, 128, 128);
//    dtext(Msg, tx, ty, 248, 247, 247, 0, 0);
//    mrc_skyfont_drawTextLeft((char*)uMsg,0,-1,tx,ty,rect1,clr1,SKYFONT_SUPPORT_HALF_CHAR);
	mrc_drawText((char*)uMsg,tx,ty,248, 247, 247,1,1);
    ref(tx-5, ty-5, tw+10, th+10);
    if(timeH==0)
    {
      timeH=timercreate();
      timerstart(timeH,(sp<800)?800:sp,0,toastCB,0);
    }
    sleep((sp<800)?800:sp);
  }
}

void DrawShadeRect(int16 x, int16 y, int16 w, int16 h, uint32 pixelA, uint32 pixelB, int8 mode)
{
  //RGBA+(RGBB-RGBA)*N/Step
  int32 AR,AG,AB;
  int32 BR,BG,BB;

  AR = (uint8)(pixelA >> 16) & 0xff;
  AG = (uint8)(pixelA >> 8) & 0xff;
  AB = (uint8)(pixelA) & 0xff;

  BR = (uint8)((pixelB >> 16) & 0xff);
  BG = (uint8)((pixelB >> 8) & 0xff);
  BB = (uint8)((pixelB) & 0xff);
  ShadeRect(x,y,w,h,AR,AG,AB,BR,BG,BB,mode);

}

int DrawIMG(char* filename, int x,int y)
{
#ifndef WIN32
  PMRAPP_IMAGE_WH p;
  MRAPP_IMAGE_ORIGIN_T t_imgsizeInfo;
  T_DRAW_DIRECT_REQ input;

  char* filext=NULL;

  filext=ExtractFileExt(filename);

  if (mrc_strlen(filext)>0)
  {
	  mrc_stopGif(gp);	//注意每调用一次显示函数就需要停止一次否则一直在闪动

	  t_imgsizeInfo.src = (char*)filename;
	  t_imgsizeInfo.len = mrc_strlen(filename);
	  t_imgsizeInfo.src_type = SRC_NAME;
	  mrc_getImageInfo(&t_imgsizeInfo, (uint8**)&p);

	  input.src_type = SRC_NAME;
	  input.ox = x;
	  input.oy = y;
	  input.w = p->width;
	  input.h = p->height;

	  input.src = (char*)filename;
	  input.src_len = mrc_strlen(filename);

	  if (strcmp(filext,"gif")==0)
	  {
		  mrc_drawGifToFrameBuffer(&input,(uint8**)&gp);
	  } 
	  else
	  {
		  return mrc_platEx(3010, (uint8*)&input, sizeof(T_DRAW_DIRECT_REQ), NULL, NULL, NULL);
	  }  
  }


  
#else
  return 0;
#endif
}

int stopDrawIMGif(void)
{
#ifndef WIN32
	return mrc_stopGif(gp);
#else
	return 0;
#endif
}

int32 my_readAllEx(char* filename, char* buf, int32 *len)
{
  int32 ret,filelen,f,oldlen;

  ret = mrc_fileState(filename);
  if((ret != MR_IS_FILE))
    return MR_FAILED;

  filelen = mrc_getLen(filename);
  if (filelen <= 0)
    return MR_FAILED;
  filelen = (filelen > *len)? *len:filelen;

  f = mrc_open(filename, MR_FILE_RDONLY );
  if (f == 0)
    return MR_FAILED;

  oldlen = 0;
  while(oldlen < filelen)
  {
    ret = mrc_read(f, (char*)buf+oldlen, filelen-oldlen);
    if (ret <= 0)
    {
      mrc_close(f);
      return MR_FAILED;
    }
    oldlen = oldlen + ret;
  }
  mrc_close(f);
  *len = filelen;
  return MR_SUCCESS;
}
/*
void *readFileFromAssets(char *filename, int32 *len)
{
	return mrc_readFileFromMrp((const char*)filename, len, 0);
}
*/

int mycos(int x)
{
	int ret;
    
    if(x>=0)
		x=x%361;
	else
		x=-x%361;

	if( x >= 0 && x <= 90)
		ret=DCOS[x];
	else if(x >90 && x <=180)
		ret= - DSIN[x - 90];
	else if(x > 180 && x <= 270)
		ret= - DCOS[x - 180];
	else if(x > 270 && x <= 360)
		ret=DSIN[x - 270];
	return ret;

}

int mysin(int x)
{
	int ret;
	
    if(x>=0)
		x=x%361;
	else
		x=-x%361;	

	if(x >= 0 && x <= 90)
		ret=DSIN[x];
	else if(x > 90 && x <= 180)
		ret=DCOS[x - 90];
	else if(x > 180 && x <= 270)
		ret=- DSIN[x - 180];
	else if(x >270 && x <= 360)
		ret=- DCOS[x - 270];
	return ret;
}

/**
 * 计算给定角度的正切值
 * 参数x: 角度值（整数，单位：度）
 * 返回值: 正切值 × 4096
 */
long int mytan(int x)
{
    int sign = 1;  // 符号，1为正，-1为负
    int reduced_angle;
    long int result;
    
    // 1. 处理周期性：tan(x+180°) = tan(x)
    x = x % 360;
    if (x < 0) {
        x += 360;
    }
        
    if (x >= 0 && x <= 90) {
        // 第一象限：直接查表
        sign = 1;
        reduced_angle = x;
    } else if (x > 90 && x <= 180) {
        // 第二象限：tan(180°-x) = -tan(x)
        sign = -1;
        reduced_angle = 180 - x;
    } else if (x > 180 && x <= 270) {
        // 第三象限：tan(180°+x) = tan(x)
        sign = 1;
        reduced_angle = x - 180;
    } else {
        // 第四象限：tan(360°-x) = -tan(x)
        sign = -1;
        reduced_angle = 360 - x;
    }
    
    // 4. 处理 90° 的情况（正切无穷大）
    if (reduced_angle == 90) {
        // 返回一个很大的值作为近似（最大正切值）
        // DTAN[90] = 250875938（大约是 tan89° 的值）
        if (sign > 0) {
            return DTAN[89];  // 近似无穷大
        } else {
            return -DTAN[89];
        }
    }
    
    // 5. 查表并返回带符号的值
    result = DTAN[reduced_angle];
    return sign > 0 ? result : -result;
}

static int d_atan(long int value)
{
    long int low = 0, up = 90, spec = 45, binary = 1, sitaValue = 0;
    long int sita;
    sita = value * 4096;

    while (binary)
    {
        if (sita > DTAN[spec])
        {
            up = up;
            low = spec;
        }
        else if (sita < DTAN[spec])
        {
            up = spec;
            low = low;
        }
        else if (sita == DTAN[spec])
        {
            sitaValue = spec;
            binary = 0;
        }
        if ((up - low) == 1)
        {
            sitaValue = low;
            binary = 0;
        }
        else
        {
            spec = ((up - low) >> 1) + low;
        }

    }
    return sitaValue;
}

int myatan(long int value) {
    if (value >= 0) {
        return d_atan(value);
    } else {
        return -d_atan(-value);
    }
}

//参数：旋转中心点(px,py)，旋转横向半径rx，旋转纵向半径ry， 旋转后坐标指针(*x,*y)
void toSpin(int px,int py,int rx,int ry,int r,int* x,int* y)
{
	int cx,cy,rr;

	if(r>=0)
		rr=r%361;
	else
		rr=-r%361;
	
	cx=mycos(rr);
	cy=mysin(rr);
	*x=rx*cx/4096+px;
	*y=ry*cy/4096+py;

}


//设置平台根目录为T卡，（平台默认根目录为T卡mythroad，如要访问T卡根目录，例如歌曲，则需使用本接口）
void SetRootPath() 
{
	char* strDisk="C:/";
	uint8 *output = NULL;
	int32 output_len = 0;

	mrc_platEx(1204, (uint8*)strDisk, mrc_strlen(strDisk), &output, &output_len, NULL);
}

//自定义printf，需要引用#include <stdarg.h>
void app_printf(const char *format, ...)
{
    va_list args;
    char data[1000];
    int32 file = mrc_open("log.txt", MR_FILE_RDWR|MR_FILE_CREATE);

    //转换
    mrc_memset(data, 0, 1000);
    va_start(args, format);
    vsprintf(data, format, args);
    mrc_printf("%s", data);

    //输出
    if(file!=0)
    {
        mrc_seek(file, 0, MR_SEEK_END);
        mrc_write(file, data, sizeof(char)*mrc_strlen(data));
        mrc_close(file);
    }
}



void showlogo(char* filename,int32 w,int32 h)
{
 int32 filelen=0;
 uint16 *logoBmp=0;

logoBmp = mrc_readFileFromMrp(filename, &filelen,0);
cls(0,0,0);

if(logoBmp)
  mrc_bitmapShowEx(logoBmp,(SCRW-w)/2,(SCRH-h)/2,w,w,h,BM_COPY,0,0);

ref(0,0,SCRW,SCRH);
sleep(1000);

if(logoBmp)
{
	freefiledata(logoBmp,filelen);
	logoBmp=0;
}
}

int PrintScr(int x,int y,int w,int h,int max_w,char* filename)
{
	mr_datetime datetime;
	char saveShotPath[40]={0};
	char*pTempBmp = NULL;
	int fileH = 0;
	int i=0;
	int fileLen = 54+16+2;
	int addPos = 0;

	if (filename!=NULL&&mrc_strlen(filename)>0&&mrc_strlen(filename)<40)
	{
		mrc_strcpy(saveShotPath,filename);	
	} 
	else
	{
		if(mrc_fileState("Screenshut")!=2)
			mrc_mkDir("Screenshut");
		getdatetime(&datetime);
		sprintf(saveShotPath,"Screenshut/%d.%d %d:%d.bmp",datetime.month,datetime.day,datetime.hour,datetime.minute);
	}
	

	if (mrc_fileState(saveShotPath)!=8)
		mrc_remove(saveShotPath);
	
	fileH = open(saveShotPath,12);
	if (fileH>0)
	{
		pTempBmp = malloc(fileLen);

		memset(pTempBmp,0,fileLen);
		memcpy(pTempBmp,BMP_HEAD,54);
		memcpy(pTempBmp+2,&fileLen,4);
		memcpy(pTempBmp+18,&w,4);
		memcpy(pTempBmp+22,&h,4);
		memcpy(pTempBmp+54,BMP_HEAD_MTK,16);

		seek(fileH,0,SEEK_SET);
		write(fileH,pTempBmp,70);

		free(pTempBmp);
		pTempBmp = NULL;

		for(i = h+y-1;i>=y;i--)
		{
			write(fileH,pScreenBuf0+(x+i*max_w),w*2);
			write(fileH,"\00\00",w%2*2);

		}

		close(fileH);
		fileH = 0;
	}
	
	return 0;

}

int isRightKey(int32 x,int32 y)
{
	if(y > (SCRH - FONTH-5))
	{
		if(x > (SCRW - FONTH- FONTH-5))
		{
			return TRUE;
		}
	}
	
	return FALSE;
}

int isLeftKey(int32 x,int32 y)
{
	if(y > (SCRH - FONTH-5))
	{
		if(x < FONTH+FONTH+5)
		{
			return TRUE;
		}
	}

	return FALSE;
}

int isMiddleKey(int32 x,int32 y)
{
	if(y > (SCRH - FONTH-5))
	{
		if((x > SCRW/2-FONTH) &&(x < SCRW/2+FONTH))
		{
			return TRUE;
		}
	}

	return FALSE;
}

int isMouseInArea(int32 x,int32 y,int32 x1,int32 y1,int32 w,int32 h)
{
	if((x >= x1)&&(x <= (x1+w)))
	{
		if((y >= y1) && (y<= (y1+h)))
			return TRUE;
	}

	return FALSE;
}

/*
加载例子
mrpfileSt dino_bmp
loadmrpfile("dino.bmp",&dino_bmp);
*/
void loadmrpfile(char* filename,mrpfileSt* filest)
{
	filest->bitmap=(uint16*)mrc_readFileFromMrp(filename,&(filest->filelen),0);
}

/*
释放例子
mrpfileSt dino_bmp
freemrpfile(&dino_bmp);
*/
void freemrpfile(mrpfileSt* filest)
{
	if(filest->bitmap)
	{
		mrc_freeFileData(filest->bitmap,filest->filelen);
		filest->bitmap=NULL;
	}
}

void initRect(mr_screenRectSt* rect,uint16 x,uint16 y,uint16 w,uint16 h)
{
    rect->x=x;
    rect->y=y;
    rect->w=w;
    rect->h=h;
}