BITS 64

segment .data

align 16
charset_blk:    INCBIN "./rc/charset.blk"
align 16
icon16_bmp:     INCBIN "./rc/icon16.bmp"
align 16
icon32_bmp:     INCBIN "./rc/icon32.bmp"
align 16
icon64_bmp:     INCBIN "./rc/icon64.bmp"
align 16
st_charset_bmp: INCBIN "./rc/st_chars_mono.bmp"
align 16
flags_bmp:      INCBIN "./rc/flags_256.bmp"
align 16
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
  mov rax,st_charset_bmp
  ret

Get_charset_blk:
  mov rax,charset_blk
  ret

Get_tos_flags_bmp:
  mov rax,flags_bmp
  ret

Get_icon16_bmp:
  mov rax,icon16_bmp
  ret

Get_icon32_bmp:
  mov rax,icon32_bmp
  ret

Get_icon64_bmp:
  mov rax,icon64_bmp
  ret
  
Get_steem_new_txt:
  mov rax,steem_new_txt
  ret

Get_steem_new_txt_end:
  mov rax,steem_new_txt_end
  ret

Get_HD6301V1ST_img:
  mov rax,HD6301V1ST_img
  ret

Get_HD6301V1ST_img_end:
  mov rax,HD6301V1ST_img_end
  ret

Get_HFE_boot_bin:
  mov rax,HFE_boot_bin
  ret

Get_HFE_boot_bin_end:
  mov rax,HFE_boot_bin_end
  ret

Get_ym2149_fixed_vol_bin:
  mov rax,ym2149_fixed_vol_bin
  ret

Get_ym2149_fixed_vol_bin_end:
  mov rax,ym2149_fixed_vol_bin_end
  ret
