#include "Display_EPD_W21_spi.h"
#include "Display_EPD_W21.h"
#include "CH59x_common.h"

//Delay Functions（改用 CH59x 的毫秒延时）
#define delay_xms(x)   DelayMs(x)

////////////////////////////////////E-paper demo//////////////////////////////////////////////////////////
//Busy function
void Epaper_READBUSY(void)
{ 
  while(1)
  {	 //=1 BUSY
     if(isEPD_W21_BUSY==0) break;
  }  
}
//Full screen refresh initialization
void EPD_HW_Init(void)
{
	EPD_W21_RST_0;  // Module reset   
	delay_xms(10);//At least 10ms delay 
	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay 
	
	Epaper_READBUSY();   
	EPD_W21_WriteCMD(0x12);  //SWRESET
	Epaper_READBUSY();   
		
	EPD_W21_WriteCMD(0x01); //Driver output control      
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
	EPD_W21_WriteDATA(0x00);

	EPD_W21_WriteCMD(0x11); //data entry mode       
	EPD_W21_WriteDATA(0x01);

	EPD_W21_WriteCMD(0x44); //set Ram-X address start/end position   
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(EPD_WIDTH/8-1);    

	EPD_W21_WriteCMD(0x45); //set Ram-Y address start/end position          
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00); 

	EPD_W21_WriteCMD(0x3C); //BorderWavefrom
	EPD_W21_WriteDATA(0x05);	
	  	
  EPD_W21_WriteCMD(0x18); //Read built-in temperature sensor
	EPD_W21_WriteDATA(0x80);	

	EPD_W21_WriteCMD(0x4E);   // set RAM x address count to 0;
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteCMD(0x4F);   // set RAM y address count to 0X199;    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
  Epaper_READBUSY();
	
}


//////////////////////////////Display Update Function///////////////////////////////////////////////////////
//Full screen refresh update function
void EPD_Update(void)
{   
  EPD_W21_WriteCMD(0x22); //Display Update Control
  EPD_W21_WriteDATA(0xF7);   
  EPD_W21_WriteCMD(0x20); //Activate Display Update Sequence
  Epaper_READBUSY();   

}

//Partial refresh update function
void EPD_Part_Update(void)
{
	EPD_W21_WriteCMD(0x22); //Display Update Control
	EPD_W21_WriteDATA(0xFF);   
	EPD_W21_WriteCMD(0x20); //Activate Display Update Sequence
	Epaper_READBUSY(); 			
}
//////////////////////////////Display Data Transfer Function////////////////////////////////////////////
//Full screen refresh display function
void EPD_WhiteScreen_ALL(const unsigned char *datas)
{
   unsigned int i;	
  EPD_W21_WriteCMD(0x24);   //write RAM for black(0)/white (1)
  for(i=0;i<EPD_ARRAY;i++)
   {               
     EPD_W21_WriteDATA(datas[i]);
   }
   EPD_Update();	 
}

//Clear screen display
void EPD_WhiteScreen_White(void)
{
 unsigned int i;
 EPD_W21_WriteCMD(0x24);   //write RAM for black(0)/white (1)
 for(i=0;i<EPD_ARRAY;i++)
 {
		EPD_W21_WriteDATA(0xff);
	}
	EPD_Update();
}
//Display all black
void EPD_WhiteScreen_Black(void)
{
 unsigned int i;
 EPD_W21_WriteCMD(0x24);   //write RAM for black(0)/white (1)
 for(i=0;i<EPD_ARRAY;i++)
 {
		EPD_W21_WriteDATA(0x00);
	}
	EPD_Update();
}
//Partial refresh of background display, this function is necessary, please do not delete it!!!
void EPD_SetRAMValue_BaseMap( const unsigned char * datas)
{
	unsigned int i;   	
  EPD_W21_WriteCMD(0x24);   //Write Black and White image to RAM
  for(i=0;i<EPD_ARRAY;i++)
   {               
     EPD_W21_WriteDATA(datas[i]);
   }
  EPD_W21_WriteCMD(0x26);   //Write Black and White image to RAM
  for(i=0;i<EPD_ARRAY;i++)
   {               
     EPD_W21_WriteDATA(datas[i]);
   }
   EPD_Update();		 
	 
}
//Partial refresh display
void EPD_Dis_Part(unsigned int x_start,unsigned int y_start,const unsigned char * datas,unsigned int PART_COLUMN,unsigned int PART_LINE)
{
	unsigned int i;  
	unsigned int x_end,y_end;
	
	x_start=x_start/8; //x address start
	x_end=x_start+PART_LINE/8-1; //x address end
	y_start=y_start; //Y address start
	y_end=y_start+PART_COLUMN-1; //Y address end
	
	EPD_W21_RST_0;  // Module reset   
	delay_xms(10);//At least 10ms delay 
	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay 	
	EPD_W21_WriteCMD(0x3C); //BorderWavefrom,
	EPD_W21_WriteDATA(0x80);	
	
	EPD_W21_WriteCMD(0x44);       // set RAM x address start/end
	EPD_W21_WriteDATA(x_start);  //x address start
	EPD_W21_WriteDATA(x_end);   //y address end   
	EPD_W21_WriteCMD(0x45);    // set RAM y address start/end
	EPD_W21_WriteDATA(y_start%256);  //y address start2 
	EPD_W21_WriteDATA(y_start/256); //y address start1 
	EPD_W21_WriteDATA(y_end%256);  //y address end2 
	EPD_W21_WriteDATA(y_end/256); //y address end1   

	EPD_W21_WriteCMD(0x4E);        // set RAM x address count to 0;
	EPD_W21_WriteDATA(x_start);   //x start address
	EPD_W21_WriteCMD(0x4F);      // set RAM y address count to 0X127;    
	EPD_W21_WriteDATA(y_start%256);//y address start2
	EPD_W21_WriteDATA(y_start/256);//y address start1
	
	
	 EPD_W21_WriteCMD(0x24);   //Write Black and White image to RAM
   for(i=0;i<PART_COLUMN*PART_LINE/8;i++)
   {                         
     EPD_W21_WriteDATA(datas[i]);
   } 
	 EPD_Part_Update();

}
//Full screen partial refresh display
void EPD_Dis_PartAll(const unsigned char * datas)
{
	unsigned int i;
	unsigned int PART_COLUMN, PART_LINE;
	PART_COLUMN=EPD_HEIGHT,PART_LINE=EPD_WIDTH;

	EPD_W21_RST_0;  // Module reset
	delay_xms(10); //At least 10ms delay
	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay
	EPD_W21_WriteCMD(0x3C); //BorderWavefrom,
	EPD_W21_WriteDATA(0x80);


	EPD_W21_WriteCMD(0x24);   //Write Black and White image to RAM
	 for(i=0;i<PART_COLUMN*PART_LINE/8;i++)
	 {
		 EPD_W21_WriteDATA(datas[i]);
	 }
	 EPD_Part_Update();

}
//Full screen partial refresh display (GUI mode, no reset)
// 与 EPD_Dis_PartAll 相同但不做硬件复位，保留 EPD_HW_Init_GUI 配置的 RAM 布局
void EPD_Dis_PartAll_GUI(const unsigned char * datas)
{
	unsigned int i;
	unsigned int PART_COLUMN, PART_LINE;
	PART_COLUMN=EPD_HEIGHT,PART_LINE=EPD_WIDTH;

	EPD_W21_WriteCMD(0x3C); //BorderWavefrom
	EPD_W21_WriteDATA(0x80);

	EPD_W21_WriteCMD(0x24);   //Write Black and White image to RAM
	 for(i=0;i<PART_COLUMN*PART_LINE/8;i++)
	 {
		 EPD_W21_WriteDATA(datas[i]);
	 }
	 EPD_Part_Update();
}
//智能局部刷新：0x26 = 当前屏幕内容(old)，0x24 = 新帧(new)，局部刷新
//（0x26 必须等于当前屏幕，控制器对比两 RAM 只驱动差异像素 → 无残影）
void EPD_PartRefresh_GUI(const unsigned char * new_img, const unsigned char * old_img)
{
	unsigned int i;
	unsigned int PART_COLUMN, PART_LINE;
	PART_COLUMN=EPD_HEIGHT,PART_LINE=EPD_WIDTH;

	EPD_W21_WriteCMD(0x3C); //BorderWavefrom
	EPD_W21_WriteDATA(0x80);

	/* 0x26 = 当前屏幕内容（对比基准） */
	EPD_W21_WriteCMD(0x26);
	 for(i=0;i<PART_COLUMN*PART_LINE/8;i++)
	 {
		 EPD_W21_WriteDATA(old_img[i]);
	 }

	/* 0x24 = 新帧（目标） */
	EPD_W21_WriteCMD(0x24);
	 for(i=0;i<PART_COLUMN*PART_LINE/8;i++)
	 {
		 EPD_W21_WriteDATA(new_img[i]);
	 }

	EPD_Part_Update();
}
//窗口局部刷新（照参考项目 SSD1681 窗口命令方式，不 RST）：
// 在 RAM 中设置 X/Y 窗口，只写窗口内的新帧数据到 0x24，然后局部刷新。
// 0x26 保持为当前屏幕内容（用 EPD_SetRAMValue_BaseMap 或全屏刷新时同步）。
// 坐标：x 为字节列(0~23)，y 为行(0~175)，x_len 字节宽，y_len 行高。
void EPD_PartRefresh_Window(unsigned int x, unsigned int y,
                            const unsigned char * new_img, const unsigned char * old_img,
                            unsigned int x_len, unsigned int y_len)
{
	unsigned int i, j;
	unsigned int x_end = x + x_len - 1;
	/* GUI 布局 (EPD_HW_Init_GUI) 的 Y 方向反向：
	 * 数组行 j ↔ RAM Y = (EPD_HEIGHT+24-1) - j = 199 - j
	 * data entry 0x01: X 递增, Y 递减（每行 Y-1） */
	unsigned int y_top_ram = (EPD_HEIGHT + 24 - 1) - y;       /* 数组行 y → RAM Y（起始，最大） */
	unsigned int y_bottom_ram = (EPD_HEIGHT + 24 - 1) - (y + y_len - 1); /* 结束，最小 */

	PRINT("  [EPD] WIN arr(x=%u-%u y=%u-%u) ram(X=%u-%u Y=%u-%u)\n",
		x, x_end, y, y + y_len - 1,
		x, x_end, y_top_ram, y_bottom_ram);
	PRINT("  [EPD] WIN data row0[0..7]:");
	for(i = 0; i < 8 && i < x_len; i++)
		PRINT(" %02X", new_img[y * EPD_WIDTH / 8 + x + i]);
	PRINT("\n");

	/* 注意：调用前必须已执行 EPD_HW_Init_GUI（配置好 RAM 布局）。
	 * 此处不做 RST，否则会清掉刚配置的布局，窗口写入错位。 */
	EPD_W21_WriteCMD(0x3C); //BorderWavefrom
	EPD_W21_WriteDATA(0x80);

	/* set RAM x address start/end */
	EPD_W21_WriteCMD(0x44);
	EPD_W21_WriteDATA(x);
	EPD_W21_WriteDATA(x_end);
	/* set RAM y address start/end（Y 反向：start 大 end 小） */
	EPD_W21_WriteCMD(0x45);
	EPD_W21_WriteDATA(y_top_ram & 0xFF);
	EPD_W21_WriteDATA((y_top_ram >> 8) & 0xFF);
	EPD_W21_WriteDATA(y_bottom_ram & 0xFF);
	EPD_W21_WriteDATA((y_bottom_ram >> 8) & 0xFF);

	/* set RAM x address count */
	EPD_W21_WriteCMD(0x4E);
	EPD_W21_WriteDATA(x);
	/* set RAM y address count：从数组行 y（RAM Y 最大）开始，Y 递减 */
	EPD_W21_WriteCMD(0x4F);
	EPD_W21_WriteDATA(y_top_ram & 0xFF);
	EPD_W21_WriteDATA((y_top_ram >> 8) & 0xFF);

	/* 0x26 = 当前屏幕内容（旧帧窗口，对比基准） */
	EPD_W21_WriteCMD(0x26);
	for(j = 0; j < y_len; j++)
	{
		const unsigned char *row = old_img + (y + j) * EPD_WIDTH / 8 + x;
		for(i = 0; i < x_len; i++)
		{
			EPD_W21_WriteDATA(row[i]);
		}
	}

	/* 0x24 = 新帧（目标），局部刷新对比 0x24 vs 0x26 驱动差异 */
	EPD_W21_WriteCMD(0x24);
	for(j = 0; j < y_len; j++)
	{
		const unsigned char *row = new_img + (y + j) * EPD_WIDTH / 8 + x;
		for(i = 0; i < x_len; i++)
		{
			EPD_W21_WriteDATA(row[i]);
		}
	}
	EPD_Part_Update();
}

// Window partial refresh: erase the target area to white first, then write
// the new image into the same window. This is stronger than differential
// window refresh and reduces ghosting from the previous minute/page content.
void EPD_PartRefresh_WindowEraseThenWrite(unsigned int x, unsigned int y,
                                          const unsigned char *new_img,
                                          unsigned int x_len, unsigned int y_len)
{
	unsigned int i, j;
	unsigned int x_end = x + x_len - 1;
	unsigned int y_top_ram = (EPD_HEIGHT + 24 - 1) - y;
	unsigned int y_bottom_ram = (EPD_HEIGHT + 24 - 1) - (y + y_len - 1);

	EPD_W21_WriteCMD(0x3C); //BorderWavefrom
	EPD_W21_WriteDATA(0x80);

	/* Step 1: clear the target window to white. */
	EPD_W21_WriteCMD(0x44);
	EPD_W21_WriteDATA(x);
	EPD_W21_WriteDATA(x_end);
	EPD_W21_WriteCMD(0x45);
	EPD_W21_WriteDATA(y_top_ram & 0xFF);
	EPD_W21_WriteDATA((y_top_ram >> 8) & 0xFF);
	EPD_W21_WriteDATA(y_bottom_ram & 0xFF);
	EPD_W21_WriteDATA((y_bottom_ram >> 8) & 0xFF);

	EPD_W21_WriteCMD(0x4E);
	EPD_W21_WriteDATA(x);
	EPD_W21_WriteCMD(0x4F);
	EPD_W21_WriteDATA(y_top_ram & 0xFF);
	EPD_W21_WriteDATA((y_top_ram >> 8) & 0xFF);

	EPD_W21_WriteCMD(0x26);
	for(j = 0; j < y_len; j++)
	{
		for(i = 0; i < x_len; i++)
		{
			EPD_W21_WriteDATA(0xFF);
		}
	}

	EPD_W21_WriteCMD(0x4E);
	EPD_W21_WriteDATA(x);
	EPD_W21_WriteCMD(0x4F);
	EPD_W21_WriteDATA(y_top_ram & 0xFF);
	EPD_W21_WriteDATA((y_top_ram >> 8) & 0xFF);

	EPD_W21_WriteCMD(0x24);
	for(j = 0; j < y_len; j++)
	{
		for(i = 0; i < x_len; i++)
		{
			EPD_W21_WriteDATA(0xFF);
		}
	}
	EPD_Part_Update();

	/* Step 2: draw the new content into the cleared window. */
	EPD_W21_WriteCMD(0x44);
	EPD_W21_WriteDATA(x);
	EPD_W21_WriteDATA(x_end);
	EPD_W21_WriteCMD(0x45);
	EPD_W21_WriteDATA(y_top_ram & 0xFF);
	EPD_W21_WriteDATA((y_top_ram >> 8) & 0xFF);
	EPD_W21_WriteDATA(y_bottom_ram & 0xFF);
	EPD_W21_WriteDATA((y_bottom_ram >> 8) & 0xFF);

	EPD_W21_WriteCMD(0x4E);
	EPD_W21_WriteDATA(x);
	EPD_W21_WriteCMD(0x4F);
	EPD_W21_WriteDATA(y_top_ram & 0xFF);
	EPD_W21_WriteDATA((y_top_ram >> 8) & 0xFF);

	EPD_W21_WriteCMD(0x26);
	for(j = 0; j < y_len; j++)
	{
		for(i = 0; i < x_len; i++)
		{
			EPD_W21_WriteDATA(0xFF);
		}
	}

	EPD_W21_WriteCMD(0x4E);
	EPD_W21_WriteDATA(x);
	EPD_W21_WriteCMD(0x4F);
	EPD_W21_WriteDATA(y_top_ram & 0xFF);
	EPD_W21_WriteDATA((y_top_ram >> 8) & 0xFF);

	EPD_W21_WriteCMD(0x24);
	for(j = 0; j < y_len; j++)
	{
		const unsigned char *row = new_img + (y + j) * EPD_WIDTH / 8 + x;
		for(i = 0; i < x_len; i++)
		{
			EPD_W21_WriteDATA(row[i]);
		}
	}
	EPD_Part_Update();
}

// Example-style partial refresh: write only the new window data and trigger
// one partial update after the window is prepared.
void EPD_PartRefresh_WindowExample(unsigned int x, unsigned int y,
                                   const unsigned char *new_img,
                                   unsigned int x_len, unsigned int y_len)
{
	unsigned int i, j;
	unsigned int x_end = x + x_len - 1;
	unsigned int y_top_ram = (EPD_HEIGHT + 24 - 1) - y;
	unsigned int y_bottom_ram = (EPD_HEIGHT + 24 - 1) - (y + y_len - 1);

	/* Match the example: hardware reset, border waveform 0x80, one update. */
	EPD_W21_RST_0;
	delay_xms(10);
	EPD_W21_RST_1;
	delay_xms(10);
	Epaper_READBUSY();

	/* Restore the GUI RAM layout used by the framebuffer. */
	EPD_W21_WriteCMD(0x01);
	EPD_W21_WriteDATA((EPD_HEIGHT + 24 - 1) % 256);
	EPD_W21_WriteDATA((EPD_HEIGHT + 24 - 1) / 256);
	EPD_W21_WriteDATA(0x01);

	EPD_W21_WriteCMD(0x11);
	EPD_W21_WriteDATA(0x01);

	EPD_W21_WriteCMD(0x44);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(EPD_WIDTH / 8 - 1);

	EPD_W21_WriteCMD(0x45);
	EPD_W21_WriteDATA((EPD_HEIGHT + 24 - 1) % 256);
	EPD_W21_WriteDATA((EPD_HEIGHT + 24 - 1) / 256);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00);

	EPD_W21_WriteCMD(0x3C);
	EPD_W21_WriteDATA(0x80);

	EPD_W21_WriteCMD(0x18);
	EPD_W21_WriteDATA(0x80);

	/* Set the partial window and its start address. */
	EPD_W21_WriteCMD(0x44);
	EPD_W21_WriteDATA(x);
	EPD_W21_WriteDATA(x_end);
	EPD_W21_WriteCMD(0x45);
	EPD_W21_WriteDATA(y_top_ram & 0xFF);
	EPD_W21_WriteDATA((y_top_ram >> 8) & 0xFF);
	EPD_W21_WriteDATA(y_bottom_ram & 0xFF);
	EPD_W21_WriteDATA((y_bottom_ram >> 8) & 0xFF);

	EPD_W21_WriteCMD(0x4E);
	EPD_W21_WriteDATA(x);
	EPD_W21_WriteCMD(0x4F);
	EPD_W21_WriteDATA(y_top_ram & 0xFF);
	EPD_W21_WriteDATA((y_top_ram >> 8) & 0xFF);

	/* Like EPD_Dis_Part_RAM: do not rewrite 0x26 and do not erase first. */
	EPD_W21_WriteCMD(0x24);
	for(j = 0; j < y_len; j++)
	{
		const unsigned char *row = new_img + (y + j) * EPD_WIDTH / 8 + x;
		for(i = 0; i < x_len; i++)
		{
			EPD_W21_WriteDATA(row[i]);
		}
	}

	EPD_Part_Update();
}

// Example-style multi-window refresh. Each window is prepared separately,
// then all windows are activated by one partial update, matching the sample
// clock refresh sequence.
void EPD_PartRefresh_WindowsExample(const unsigned char *new_img,
                                    const unsigned int *x,
                                    const unsigned int *y,
                                    const unsigned int *x_len,
                                    const unsigned int *y_len,
                                    unsigned int count)
{
	unsigned int n, i, j;

	for(n = 0; n < count; n++)
	{
		unsigned int x_end = x[n] + x_len[n] - 1;
		unsigned int y_top_ram = (EPD_HEIGHT + 24 - 1) - y[n];
		unsigned int y_bottom_ram =
			(EPD_HEIGHT + 24 - 1) - (y[n] + y_len[n] - 1);

		/* EPD_Dis_Part_RAM resets before preparing every digit. */
		EPD_W21_RST_0;
		delay_xms(10);
		EPD_W21_RST_1;
		delay_xms(10);
		Epaper_READBUSY();

		/* Restore the GUI RAM layout used by the framebuffer. */
		EPD_W21_WriteCMD(0x01);
		EPD_W21_WriteDATA((EPD_HEIGHT + 24 - 1) % 256);
		EPD_W21_WriteDATA((EPD_HEIGHT + 24 - 1) / 256);
		EPD_W21_WriteDATA(0x01);

		EPD_W21_WriteCMD(0x11);
		EPD_W21_WriteDATA(0x01);

		EPD_W21_WriteCMD(0x44);
		EPD_W21_WriteDATA(0x00);
		EPD_W21_WriteDATA(EPD_WIDTH / 8 - 1);

		EPD_W21_WriteCMD(0x45);
		EPD_W21_WriteDATA((EPD_HEIGHT + 24 - 1) % 256);
		EPD_W21_WriteDATA((EPD_HEIGHT + 24 - 1) / 256);
		EPD_W21_WriteDATA(0x00);
		EPD_W21_WriteDATA(0x00);

		EPD_W21_WriteCMD(0x3C);
		EPD_W21_WriteDATA(0x80);

		EPD_W21_WriteCMD(0x18);
		EPD_W21_WriteDATA(0x80);

		EPD_W21_WriteCMD(0x44);
		EPD_W21_WriteDATA(x[n]);
		EPD_W21_WriteDATA(x_end);
		EPD_W21_WriteCMD(0x45);
		EPD_W21_WriteDATA(y_top_ram & 0xFF);
		EPD_W21_WriteDATA((y_top_ram >> 8) & 0xFF);
		EPD_W21_WriteDATA(y_bottom_ram & 0xFF);
		EPD_W21_WriteDATA((y_bottom_ram >> 8) & 0xFF);

		EPD_W21_WriteCMD(0x4E);
		EPD_W21_WriteDATA(x[n]);
		EPD_W21_WriteCMD(0x4F);
		EPD_W21_WriteDATA(y_top_ram & 0xFF);
		EPD_W21_WriteDATA((y_top_ram >> 8) & 0xFF);

		EPD_W21_WriteCMD(0x24);
		for(j = 0; j < y_len[n]; j++)
		{
			const unsigned char *row =
				new_img + (y[n] + j) * EPD_WIDTH / 8 + x[n];
			for(i = 0; i < x_len[n]; i++)
			{
				EPD_W21_WriteDATA(row[i]);
			}
		}
	}

	/* Match EPD_Dis_Part_Time: one update for all prepared windows. */
	EPD_Part_Update();
}
//Deep sleep function   
void EPD_DeepSleep(void)
{  	
  EPD_W21_WriteCMD(0x10); //Enter deep sleep
  EPD_W21_WriteDATA(0x01); 
  delay_xms(100);
}

//Partial refresh write address and data
void EPD_Dis_Part_RAM(unsigned int x_start,unsigned int y_start,const unsigned char * datas,unsigned int PART_COLUMN,unsigned int PART_LINE)
{
	unsigned int i;  
	unsigned int x_end,y_end;
	
	x_start=x_start/8; //x address start
	x_end=x_start+PART_LINE/8-1; //x address end
	
	y_start=y_start-1; //Y address start
	y_end=y_start+PART_COLUMN-1; //Y address end
	
	EPD_W21_RST_0;  // Module reset   
	delay_xms(10);//At least 10ms delay 
	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay 	
	EPD_W21_WriteCMD(0x3C); //BorderWavefrom,
	EPD_W21_WriteDATA(0x80);	
	
	EPD_W21_WriteCMD(0x44);       // set RAM x address start/end
	EPD_W21_WriteDATA(x_start);  //x address start
	EPD_W21_WriteDATA(x_end);   //y address end   
	EPD_W21_WriteCMD(0x45);     // set RAM y address start/end
	EPD_W21_WriteDATA(y_start%256);  //y address start2 
	EPD_W21_WriteDATA(y_start/256); //y address start1 
	EPD_W21_WriteDATA(y_end%256);  //y address end2 
	EPD_W21_WriteDATA(y_end/256); //y address end1   

	EPD_W21_WriteCMD(0x4E);   // set RAM x address count to 0;
	EPD_W21_WriteDATA(x_start);   //x start address
	EPD_W21_WriteCMD(0x4F);   // set RAM y address count to 0X127;    
	EPD_W21_WriteDATA(y_start%256); //y address start2
	EPD_W21_WriteDATA(y_start/256); //y address start1
		
	EPD_W21_WriteCMD(0x24);   //Write Black and White image to RAM
  for(i=0;i<PART_COLUMN*PART_LINE/8;i++)
   {                         
     EPD_W21_WriteDATA(datas[i]);
   } 
}
//Clock display
void EPD_Dis_Part_Time(unsigned int x_startA,unsigned int y_startA,const unsigned char * datasA,
	                       unsigned int x_startB,unsigned int y_startB,const unsigned char * datasB,
												 unsigned int x_startC,unsigned int y_startC,const unsigned char * datasC,
												 unsigned int x_startD,unsigned int y_startD,const unsigned char * datasD,
											   unsigned int x_startE,unsigned int y_startE,const unsigned char * datasE,
												 unsigned int PART_COLUMN,unsigned int PART_LINE
	                      )
{
	EPD_Dis_Part_RAM(x_startA,y_startA,datasA,PART_COLUMN,PART_LINE);
	EPD_Dis_Part_RAM(x_startB,y_startB,datasB,PART_COLUMN,PART_LINE);
	EPD_Dis_Part_RAM(x_startC,y_startC,datasC,PART_COLUMN,PART_LINE);
	EPD_Dis_Part_RAM(x_startD,y_startD,datasD,PART_COLUMN,PART_LINE);
	EPD_Dis_Part_RAM(x_startE,y_startE,datasE,PART_COLUMN,PART_LINE);
	EPD_Part_Update();
}												 




////////////////////////////////Other newly added functions////////////////////////////////////////////
//Display rotation 180 degrees initialization
void EPD_HW_Init_180(void)
{
	EPD_W21_RST_0;  // Module reset   
	delay_xms(10); //At least 10ms delay 
	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay 
	
	Epaper_READBUSY();   
	EPD_W21_WriteCMD(0x12);  //SWRESET
	Epaper_READBUSY();   
	
	EPD_W21_WriteCMD(0x3C); //BorderWavefrom
	EPD_W21_WriteDATA(0x05);
	
	EPD_W21_WriteCMD(0x01); //Driver output control      
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
	EPD_W21_WriteDATA(0x00); 

	EPD_W21_WriteCMD(0x11); //data entry mode       
	EPD_W21_WriteDATA(0x02);

	EPD_W21_WriteCMD(0x44); //set Ram-X address start/end position   
	EPD_W21_WriteDATA(EPD_WIDTH/8-1);    
	EPD_W21_WriteDATA(0x00);  

	EPD_W21_WriteCMD(0x45); //set Ram-Y address start/end position          
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00); 
  EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);  
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);

	
  EPD_W21_WriteCMD(0x18); //Read built-in temperature sensor
	EPD_W21_WriteDATA(0x80);	

	EPD_W21_WriteCMD(0x4E);   // set RAM x address count to 0;
	EPD_W21_WriteDATA(EPD_WIDTH/8-1);  
	EPD_W21_WriteCMD(0x4F);   // set RAM y address count to 0X199;    
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00);
  Epaper_READBUSY();
}
// GUI initialization
void EPD_HW_Init_GUI(void)
{
	EPD_W21_RST_0;  // Module reset   
	delay_xms(10); //At least 10ms delay 
	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay 
	
	Epaper_READBUSY();   
	EPD_W21_WriteCMD(0x12);  //SWRESET
	Epaper_READBUSY();   
		
	EPD_W21_WriteCMD(0x01); //Driver output control      
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)%256);    
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)/256);
	EPD_W21_WriteDATA(0x01);//Show mirror

	EPD_W21_WriteCMD(0x11); //data entry mode       
	EPD_W21_WriteDATA(0x01);

	EPD_W21_WriteCMD(0x44); //set Ram-X address start/end position   
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(EPD_WIDTH/8-1);    

	EPD_W21_WriteCMD(0x45); //set Ram-Y address start/end position          
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)%256);    	
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)/256);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00); 

	EPD_W21_WriteCMD(0x3C); //BorderWavefrom
	EPD_W21_WriteDATA(0x05);	
	  	
  EPD_W21_WriteCMD(0x18); //Read built-in temperature sensor
	EPD_W21_WriteDATA(0x80);	

	EPD_W21_WriteCMD(0x4E);   // set RAM x address count to 0;
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteCMD(0x4F);   // set RAM y address count to 0X199;    
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)%256);   
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)/256);
  Epaper_READBUSY();
	
}

//GUI display
void EPD_Display(unsigned char *Image)
{
	unsigned int Width, Height,i,j;
	Width = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
	Height = EPD_HEIGHT;

	EPD_W21_WriteCMD(0x24);
	for ( j = 0; j < Height; j++) {
			for ( i = 0; i < Width; i++) {
				 EPD_W21_WriteDATA(Image[i + j * Width]);
			}
	}
	EPD_Update();
}
//GUI display full refresh with 0x26 sync
// 全屏刷新并同步 0x26：0x24 和 0x26 都写入 Image，
// 使 0x26 成为"当前屏幕内容"，后续局部刷新有正确对比基准。
void EPD_Display_Full_Sync(unsigned char *Image)
{
	unsigned int Width, Height,i,j;
	Width = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
	Height = EPD_HEIGHT;

	/* 0x24 = 目标图像 */
	EPD_W21_WriteCMD(0x24);
	for ( j = 0; j < Height; j++) {
			for ( i = 0; i < Width; i++) {
				 EPD_W21_WriteDATA(Image[i + j * Width]);
			}
	}
	/* 0x26 = 目标图像（对比基准同步为当前屏幕） */
	EPD_W21_WriteCMD(0x26);
	for ( j = 0; j < Height; j++) {
			for ( i = 0; i < Width; i++) {
				 EPD_W21_WriteDATA(Image[i + j * Width]);
			}
	}
	EPD_Update();
}
//GUI display fast refresh (no flicker)
// 阅读器同款"不闪烁"刷新：0x26 = 当前屏幕内容(old)，0x24 = 新帧(new)，
// 用 0xFF 局部刷新波形只驱动差异像素，不做整屏黑白反刷 → 不闪烁。
// 调用前 0x26 需为当前屏幕内容（prev 帧），保证对比基准正确。
// 每 N 次后需用 EPD_Display_Full_Sync 全屏清一次残影。
void EPD_Display_FastSync(unsigned char *Image, const unsigned char *old_Image)
{
	unsigned int Width, Height,i,j;
	Width = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
	Height = EPD_HEIGHT;

	/* 局刷波形，降低残影 */
	EPD_W21_WriteCMD(0x3C);
	EPD_W21_WriteDATA(0x80);

	/* 0x26 = 当前屏幕内容（对比基准） */
	EPD_W21_WriteCMD(0x26);
	for ( j = 0; j < Height; j++) {
			for ( i = 0; i < Width; i++) {
				 EPD_W21_WriteDATA(old_Image[i + j * Width]);
			}
	}
	/* 0x24 = 目标图像 */
	EPD_W21_WriteCMD(0x24);
	for ( j = 0; j < Height; j++) {
			for ( i = 0; i < Width; i++) {
				 EPD_W21_WriteDATA(Image[i + j * Width]);
			}
	}
	/* 局部刷新波形（0xFF）：只驱动差异像素，不闪烁 */
	EPD_Part_Update();
}
//GUI display: erase then write (先擦后写)
// 两步局部刷新，避免残影：
// 1. 0x26=全白, 0x24=全白 → 0xFF 局部波形刷白（清掉旧文字）
// 2. 0x26=全白(当前屏), 0x24=新帧 → 0xFF 局部波形写新文字
// 每次写 0x26/0x24 前都重置 RAM 地址指针，避免数据错位。
void EPD_Display_EraseThenWrite(unsigned char *Image)
{
	unsigned int Width, Height,i,j;
	Width = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
	Height = EPD_HEIGHT;

	/* 局刷波形，避免整屏式残影 */
	EPD_W21_WriteCMD(0x3C);
	EPD_W21_WriteDATA(0x80);

	/* 第 1 步：把屏幕刷成全白（0x24=0x26=全白，局部波形）
	 * 先重置 RAM 指针，保证从 (0,199) 开始写 */
	EPD_W21_WriteCMD(0x4E);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteCMD(0x4F);
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)%256);
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)/256);

	EPD_W21_WriteCMD(0x26);
	for ( j = 0; j < Height; j++) {
			for ( i = 0; i < Width; i++) {
				 EPD_W21_WriteDATA(0xFF);
			}
	}
	EPD_W21_WriteCMD(0x4E);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteCMD(0x4F);
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)%256);
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)/256);

	EPD_W21_WriteCMD(0x24);
	for ( j = 0; j < Height; j++) {
			for ( i = 0; i < Width; i++) {
				 EPD_W21_WriteDATA(0xFF);
			}
	}
	EPD_Part_Update();

	/* 第 2 步：显示新内容（0x26=全白=当前屏, 0x24=新帧） */
	EPD_W21_WriteCMD(0x4E);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteCMD(0x4F);
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)%256);
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)/256);

	EPD_W21_WriteCMD(0x26);
	for ( j = 0; j < Height; j++) {
			for ( i = 0; i < Width; i++) {
				 EPD_W21_WriteDATA(0xFF);
			}
	}
	EPD_W21_WriteCMD(0x4E);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteCMD(0x4F);
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)%256);
	EPD_W21_WriteDATA((EPD_HEIGHT+24-1)/256);

	EPD_W21_WriteCMD(0x24);
	for ( j = 0; j < Height; j++) {
			for ( i = 0; i < Width; i++) {
				 EPD_W21_WriteDATA(Image[i + j * Width]);
			}
	}
	EPD_Part_Update();
}

/***********************************************************
						end file
***********************************************************/

