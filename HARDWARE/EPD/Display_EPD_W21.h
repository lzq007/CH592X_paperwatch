#ifndef _DISPLAY_EPD_W21_H_
#define _DISPLAY_EPD_W21_H_


#define EPD_WIDTH   192 
#define EPD_HEIGHT  176
#define EPD_ARRAY  EPD_WIDTH*EPD_HEIGHT/8  


//Full screen refresh display
void EPD_HW_Init(void); 
void EPD_HW_Init_180(void);	
void EPD_WhiteScreen_ALL(const unsigned char *datas);
void EPD_WhiteScreen_White(void);
void EPD_WhiteScreen_Black(void);
void EPD_DeepSleep(void);
//Partial refresh display
void EPD_SetRAMValue_BaseMap(const unsigned char * datas);
void EPD_Dis_PartAll(const unsigned char * datas);
void EPD_Dis_PartAll_GUI(const unsigned char * datas);
void EPD_PartRefresh_GUI(const unsigned char * new_img, const unsigned char * old_img);
void EPD_PartRefresh_Window(unsigned int x, unsigned int y, const unsigned char * new_img, const unsigned char * old_img, unsigned int x_len, unsigned int y_len);
void EPD_PartRefresh_WindowEraseThenWrite(unsigned int x, unsigned int y, const unsigned char *new_img, unsigned int x_len, unsigned int y_len);
void EPD_PartRefresh_WindowExample(unsigned int x, unsigned int y,
                                   const unsigned char *new_img,
                                   unsigned int x_len, unsigned int y_len);
void EPD_PartRefresh_WindowsExample(const unsigned char *new_img,
                                    const unsigned int *x,
                                    const unsigned int *y,
                                    const unsigned int *x_len,
                                    const unsigned int *y_len,
                                    unsigned int count);
void EPD_Dis_Part(unsigned int x_start,unsigned int y_start,const unsigned char * datas,unsigned int PART_COLUMN,unsigned int PART_LINE);
void EPD_Dis_Part_Time(unsigned int x_startA,unsigned int y_startA,const unsigned char * datasA,
	                       unsigned int x_startB,unsigned int y_startB,const unsigned char * datasB,
												 unsigned int x_startC,unsigned int y_startC,const unsigned char * datasC,
												 unsigned int x_startD,unsigned int y_startD,const unsigned char * datasD,
											   unsigned int x_startE,unsigned int y_startE,const unsigned char * datasE,
												 unsigned int PART_COLUMN,unsigned int PART_LINE
	                      );												 
 
//GUI display					 
void EPD_HW_Init_GUI(void); 
void EPD_Display(unsigned char *Image);
void EPD_Display_Full_Sync(unsigned char *Image);
void EPD_Display_FastSync(unsigned char *Image, const unsigned char *old_Image);
void EPD_Display_EraseThenWrite(unsigned char *Image); 
												 
#endif
