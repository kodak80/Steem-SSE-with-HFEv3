segment .data

charset_blk:    INCBIN "./rc/charset.blk"

icon16_bmp:     INCBIN "./rc/icon16.bmp"

icon32_bmp:     INCBIN "./rc/icon32.bmp"

icon64_bmp:     INCBIN "./rc/icon64.bmp"

st_charset_bmp: INCBIN "./rc/st_chars_mono.bmp"

flags_bmp:      INCBIN "./rc/flags_256.bmp"


steem_new_txt:   INCBIN "./rc/steem_new.txt"
steem_new_txt_end:
align 16
HD6301V1ST_img:   INCBIN "./rc/HD6301V1ST.img"
HD6301V1ST_img_end:
align 16
HFE_boot_bin:   INCBIN "./rc/HFE_boot.bin"
HFE_boot_bin_end:
align 16
ym2149_fixed_vol_bin:   INCBIN "./rc/ym2149_fixed_vol.bin"
ym2149_fixed_vol_bin_end:
align 16

segment .text

%ifdef WIN32
%define UNDERSCORES 1
%endif

%ifdef UNDERSCORES

%macro cextern 1-*
%rep %0
extern _%1
%rotate 1
%endrep
%endmacro

%macro cglobal 1-*
%rep %0
%define %1 _%1
global _%1
%rotate 1
%endrep
%endmacro

%else

%macro cextern 1-*
%rep %0
extern %1
%define _%1 %1
%rotate 1
%endrep
%endmacro

%define cglobal global

%endif

cglobal Get_st_charset_bmp,Get_charset_blk,Get_tos_flags_bmp
cglobal Get_icon16_bmp,Get_icon32_bmp,Get_icon64_bmp
cglobal Get_steem_new_txt,Get_steem_new_txt_end
cglobal Get_HD6301V1ST_img,Get_HD6301V1ST_img_end
cglobal Get_HFE_boot_bin,Get_HFE_boot_bin_end
cglobal Get_ym2149_fixed_vol_bin,Get_ym2149_fixed_vol_bin_end

Get_st_charset_bmp:
  mov eax,st_charset_bmp
  ret

Get_charset_blk:
  mov eax,charset_blk
  ret

Get_tos_flags_bmp:
  mov eax,flags_bmp
  ret

Get_icon16_bmp:
  mov eax,icon16_bmp
  ret

Get_icon32_bmp:
  mov eax,icon32_bmp
  ret

Get_icon64_bmp:
  mov eax,icon64_bmp
  ret

Get_steem_new_txt:
  mov eax,steem_new_txt
  ret

Get_steem_new_txt_end:
  mov eax,steem_new_txt_end
  ret

Get_HD6301V1ST_img:
  mov eax,HD6301V1ST_img
  ret

Get_HD6301V1ST_img_end:
  mov eax,HD6301V1ST_img_end
  ret

Get_HFE_boot_bin:
  mov eax,HFE_boot_bin
  ret

Get_HFE_boot_bin_end:
  mov eax,HFE_boot_bin_end
  ret

Get_ym2149_fixed_vol_bin:
  mov eax,ym2149_fixed_vol_bin
  ret

Get_ym2149_fixed_vol_bin_end:
  mov eax,ym2149_fixed_vol_bin_end
  ret
