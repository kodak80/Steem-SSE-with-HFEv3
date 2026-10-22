/*---------------------------------------------------------------------------
FILE: draw_c_lowres_scanline.cpp
MODULE: draw_c
DESCRIPTION: Low res C++ drawing routine
---------------------------------------------------------------------------*/

//void draw_scanline_lowres_pixelwise_8(int border1,int picture,int border2,int hscroll){
  int n;
  WORD w[4], mask;
  MEM_ADDRESS source;
  GET_START(0,160)
  if(border1)
  {
    DRAW_BORDER_PIXELS(border1)
  }
  if(picture)
  {
    n=16-hscroll;
    if(picture<n)
      n=picture;
    if(n<16)
    { // draw a bit of a raster (hscroll)
      picture-=n;
      GET_SCREEN_DATA_INTO_REGS_AND_INC_SA
      mask=0x8000>>hscroll;
      for(;n>0;n--)
      {
        CALC_COL_LOWRES_AND_DRAWPIXEL(mask);
        mask>>=1;
      }
    }
    for(n=(picture>>4);n>0;n--)
    { // draw rasters
      GET_SCREEN_DATA_INTO_REGS_AND_INC_SA
      for(mask=BIT_15;mask;mask>>=1)
      {
        CALC_COL_LOWRES_AND_DRAWPIXEL(mask);
      }
    }
    picture&=0x0F;
    if(picture)
    { // draw a bit of a raster
      GET_SCREEN_DATA_INTO_REGS_AND_INC_SA
      mask=0x8000;
      for(;picture>0;picture--)
      {
        CALC_COL_LOWRES_AND_DRAWPIXEL(mask);
        mask>>=1;
      }
    }
  }
  if(border2)
  {
    DRAW_BORDER_PIXELS(border2)
  }
