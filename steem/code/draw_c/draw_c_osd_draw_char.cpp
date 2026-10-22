/*---------------------------------------------------------------------------
FILE: draw_c_osd_draw_char.cpp
MODULE: draw_c
DESCRIPTION: C++ OSD drawing routine.
---------------------------------------------------------------------------*/

DWORD* dadd=(DWORD*)dst+y*l+x,*daddl=dadd;
DWORD dw0,dw1;

for (int yy=h;yy>0;yy--){
  dw0=*source_ad;
  dw1=*(source_ad+1);
  source_ad+=2;
#if 1 // let compiler optimise (or not)
  for(DWORD bit=BIT_31;bit;bit>>=1) //not int!
  {
    OSD_PIXEL(bit); 
  }
#else
  OSD_PIXEL(BIT_31);
  OSD_PIXEL(BIT_30);
  OSD_PIXEL(BIT_29);
  OSD_PIXEL(BIT_28);
  OSD_PIXEL(BIT_27);
  OSD_PIXEL(BIT_26);
  OSD_PIXEL(BIT_25);
  OSD_PIXEL(BIT_24);
  OSD_PIXEL(BIT_23);
  OSD_PIXEL(BIT_22);
  OSD_PIXEL(BIT_21);
  OSD_PIXEL(BIT_20);
  OSD_PIXEL(BIT_19);
  OSD_PIXEL(BIT_18);
  OSD_PIXEL(BIT_17);
  OSD_PIXEL(BIT_16);
  OSD_PIXEL(BIT_15);
  OSD_PIXEL(BIT_14);
  OSD_PIXEL(BIT_13);
  OSD_PIXEL(BIT_12);
  OSD_PIXEL(BIT_11);
  OSD_PIXEL(BIT_10);
  OSD_PIXEL(BIT_9);
  OSD_PIXEL(BIT_8);
  OSD_PIXEL(BIT_7);
  OSD_PIXEL(BIT_6);
  OSD_PIXEL(BIT_5);
  OSD_PIXEL(BIT_4);
  OSD_PIXEL(BIT_3);
  OSD_PIXEL(BIT_2);
  OSD_PIXEL(BIT_2);
  OSD_PIXEL(BIT_1);
  OSD_PIXEL(BIT_0);
#endif
  daddl+=l;
  dadd=daddl;
}

