#include "mrc_tool.h"

mr_colourSt text_color;

//生成绑定设备，游戏ID的代号，方便用户拿着代号 索取激活码,gb编码
char* getregactivateid()
{

	MRC_PHONEINFO_T phoneinfo;
	char temp[100]={0};
	char idstr[50]={0};

	char* mrpfile=NULL;
	char* retstr=NULL;
	int32 appid=0,len=0;

	mrc_getphoneInfo(&phoneinfo);
	mrpfile=mrc_getPackName();
	mrc_GetMrpInfo(mrpfile,MRP_APPID,(uint8*)&appid,sizeof(int32));

	mrc_sprintf(temp,"%s%dskymobi.com",phoneinfo.IMEI,appid);
	mrc_updcrc(NULL,0);
	mrc_sprintf(idstr,"%x",(uint32)mrc_updcrc((uint8*)temp,mrc_strlen(temp)));

	len=mrc_strlen(idstr)+2;
	retstr=mrc_malloc(len);
	mrc_memset(retstr,0,len);
	mrc_strcpy(retstr,idstr);


	return retstr;
}

//对比激活码是否有效,gb编码，成功返回MR_SUCCESS，失败MR_FAILD
int32 checkregactivate(char* key)
{
	char tmp[50]={0};
	char temp[50]={0};
	char* idstr=NULL;

	idstr=getregactivateid();
	mrc_sprintf(tmp,"%skey",idstr);
	mrc_updcrc(NULL,0);
	mrc_sprintf(temp,"%x",(uint32)mrc_updcrc((uint8*)tmp,mrc_strlen(tmp)));

	if (mrc_strncmp(temp,key,mrc_strlen(temp))==0)
	{
		return MR_SUCCESS;
	} 
	else
	{
		return MR_FAILED;
	}

}

//科学设置随机数种子
void my_sand()
{

	uint32 sandtmp=0;
	mr_datetime t_dt;

	mrc_getDatetime(&t_dt);
	sandtmp=(uint32)t_dt.second+(uint32)t_dt.minute+(uint32)t_dt.hour+(uint32)t_dt.day+(uint32)t_dt.month;
	mrc_sand(sandtmp);
}

/*优化的mrc_open函数，自动判断路径文件夹是否存在，若文件夹不存在会自动创建*/
void buildpath(char* filename)
{
	char* p=NULL;
	char tmp[512]={0};

	p=mrc_strstr(filename,"/");
	while(p)
	{
		mrc_strncpy(tmp,filename,p-filename);
		if(mrc_fileState(tmp)==MR_IS_INVALID)
			mrc_mkDir(tmp);

		p=mrc_strstr(p+1,"/");
	}

}


int ReadData(char* datapath,int32* maxScore)
{
	int32 hnd = mrc_open(datapath, 1);
	if(0 >= hnd)
	{
		maxScore = 0;
		return 0;
	}
	mrc_read(hnd, maxScore, sizeof(maxScore));
	mrc_close(hnd);
	return 0;
}

//保存历史最高分数 
void SaveData(char* datapath,int32 maxScore)
{
	int32 hnd=0,needread=0;
	int32 histroyscore=0;

	if (mrc_fileState(datapath)!=8)
		needread=1;


	hnd = mrc_open(datapath, 4|8);
	if(0 < hnd)
	{
		if (needread==1)
		{
			mrc_read(hnd, &histroyscore, sizeof(histroyscore));	
			mrc_seek(hnd,0,0);
		}

		if(histroyscore<maxScore)
			mrc_write(hnd, &maxScore, sizeof(maxScore));

		mrc_close(hnd);
	}

}
void Readscore(char* datapath,int32* maxScore)
{
	buildpath(datapath);

	*maxScore=0;

	mrc_safeStorage_read(0,datapath,0,maxScore,sizeof(int32));

}
void Savescore(char* datapath,int32 maxScore)
{
	int32 oldscore=0;

	buildpath(datapath);

	mrc_safeStorage_read(0,datapath,0,(void*)&oldscore,sizeof(int32));

	if (maxScore>oldscore)
	{
		mrc_safeStorage_write(0,datapath,(void*)&maxScore,sizeof(int32),0);
	}

}

//读取记录的最低数值
void Readlessscore(char* datapath, int32* lessscore)
{
	buildpath(datapath);

	*lessscore = -1;

	mrc_safeStorage_read(0,datapath,0,lessscore,sizeof(int32));

}
//保存数值，自动判断传入数值是否比记录小
void Savelessscore(char* datapath, int32 lessscore)
{
	int32 oldscore=-1;

	buildpath(datapath);

	mrc_safeStorage_read(0,datapath,0,(void*)&oldscore,sizeof(int32));

	if (lessscore<oldscore || oldscore == -1)
	{
		mrc_safeStorage_write(0,datapath,(void*)&lessscore,sizeof(int32),0);
	}

}

// 点(x,y)绕(px,py)旋转指定角度r，得到旋转后的坐标
void toSpin2(int px, int py, int rx, int ry, int r, int* x, int* y) {
    int angle, cos_angle, sin_angle;
    int dx, dy, tempX, tempY;

    // 将点(x,y)相对于旋转中心(px,py)平移到原点
    dx = *x - px;
    dy = *y - py;

    // 计算旋转后的坐标
    angle = r % 360; // 确保角度在0到359之间
    if (angle < 0) {
        angle += 360;
    }

    cos_angle = (angle % 180) / 90; // 0度或180度时cos为1，90度或270度时cos为-1
    sin_angle = angle % 180 == 90 ? -1 : 1; // 90度或270度时sin为-1，其他角度时sin为1

    // 应用旋转
    tempX = cos_angle * dx - sin_angle * dy;
    tempY = sin_angle * dx + cos_angle * dy;
    dx = tempX;
    dy = tempY;

    // 将结果转换回整数
    *x = dx + rx;
    *y = dy + ry;
}


int32 get_str_width(char *str)
{
    int32 w = 0;
    int32 h = 0;

    if (str != NULL)
    {
        mrc_textWidthHeight(str, 0, MR_FONT_MEDIUM, &w, &h);
        return w;
    }
    return 0;
}

int32 get_str_height(char *str)
{
    int32 w = 0;
    int32 h = 0;

    if (str != NULL)
    {
        mrc_textWidthHeight(str, 0, MR_FONT_MEDIUM, &w, &h);
        return h;
    }
    return 0;
}

int32 collRect(int32 x, int32 y, int32 w, int32 h, int32 xx, int32 yy, int32 ww, int32 hh)
{
    return (abs(x - xx) < (w + ww) / 2) && (abs(y - yy) < (h + hh) / 2);
}

int32 fps_start, fps_prev, fps_frame, fps_speed;
int32 initFpsTool(int32 delta)
{
    fps_start = mrc_getUptime();
    fps_frame = 0;
    fps_speed = delta;
    return fps_start;
}

// 获取当前fps
int32 getRealFps(void)
{
    return 0;
}

// 获取平均fps
int32 getMainFps(void)
{
	if ((mrc_getUptime() - fps_start)!=0)
	{
		return fps_frame * 1000 / (mrc_getUptime() - fps_start);
	}
	return 0;
}

// 绘制结束时定时器调用
int32 runFpsTool(void)
{
    fps_prev = mrc_getUptime();
    fps_frame++;
    return fps_frame;
}
