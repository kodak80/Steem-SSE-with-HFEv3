/*---------------------------------------------------------------------------
FILE: draw_c_osd_blueize_line.cpp
MODULE: draw_c
DESCRIPTION: C++ OSD drawing routine.
---------------------------------------------------------------------------*/

//void osd_blueize_line_16_555(int x,int y,int w)

  DWORD*dadd=draw_mem+y*draw_line_length+x;
  for(int n=w;n>0;n-=2)
  {
    OSD_DRAWPIXEL(col_blue);
    OSD_DRAWPIXEL(col_blue); // plain as with assembly routine
  }
