; FILE: asm_lowres.asm
; MODULE: asm_draw
; DESCRIPTION: Various routines to draw the lowres ST screen to PC video memory.

%macro DRAWPIXEL_LOWRES_DW 1 ;bpp
  %if %1==4
    mov [edi],eax      ; write colour to screen address
    mov [edi+4],eax
  %endif
  add edi,2*%1         ; next screen address
%endmacro

%macro DRAW_SCANLINE_PIXELWISE_LOWRES 2 ;bpp,dw
  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 0,160
  GET_PC_DRAW_ADDR_INTO_EDI

  mov ebx,[p_border1]
  shr ebx,4  ;pixels of left border/16
  DRAW_BORDER %1,ebx,%2

  mov ebx,[p_border1]
  and ebx,15
  jz short %%finished_border_1
  mov eax,[pal0]
%%extra_pix_border_1:

%if %2==0
  DRAWPIXEL %1,0
%elif %2==2
  DRAWPIXEL_LOWRES_4W %1
%else
  DRAWPIXEL_LOWRES_DW %1
%endif

  dec ebx
  jnz short %%extra_pix_border_1
%%finished_border_1:

  mov ebx,[p_picture]
  or ebx,ebx
  jz near %%border_2

  mov eax,16
  mov ecx,[p_hscroll]  ;how many pixels to skip
  sub eax,ecx
  ;eax now contains how many pixels to draw in the first raster
  cmp eax,ebx           ;how many pixels to draw
  jl short %%no_more_reduction
  mov eax,ebx           ;there's less pixels to draw than remaining in the first raster
%%no_more_reduction:

  cmp eax,16
  je near %%middle_bit

  push eax ;store counter
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address
  shl ebx,cl
  shl edx,cl  ;skip initial pixels

  pop ecx ;number of pixels to draw in first raster in ecx
  sub [p_picture],ecx   ;reduce future number of pixels to draw

%%next_left_pixel:

  CALC_COL_LOWRES
%if %2==0
  DRAWPIXEL %1,0
%elif %2==2
  DRAWPIXEL_LOWRES_4W %1
%else
  DRAWPIXEL_LOWRES_DW %1
%endif

  dec ecx
  jnz near %%next_left_pixel

%%middle_bit:
  mov ecx,[p_picture] ;number of pixels left
  shr ecx,4           ;/16 to get number of full rasters

  jmp near %%next_raster_lowres
%%draw_raster_lowres:
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address
  %rep 15

    CALC_COL_LOWRES
%if %2==0
    DRAWPIXEL %1,1 ;carelessly
%elif %2==2
  DRAWPIXEL_LOWRES_4W %1
%else
    DRAWPIXEL_LOWRES_DW %1
%endif

  %endrep

  CALC_COL_LOWRES
%if %2==0
  DRAWPIXEL %1,0 ;carefully
%elif %2==2
  DRAWPIXEL_LOWRES_4W %1
%else
  DRAWPIXEL_LOWRES_DW %1
%endif

%%next_raster_lowres:
  dec ecx
  jns near %%draw_raster_lowres

  mov ecx,[p_picture]
  and ecx,15          ;extra pixels
  jz near %%border_2
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address

%%next_right_pixel:

  CALC_COL_LOWRES
%if %2==0
  DRAWPIXEL %1,0 ;carefully
%elif %2==2
  DRAWPIXEL_LOWRES_4W %1
%else
  DRAWPIXEL_LOWRES_DW %1
%endif

  dec ecx
  jnz near %%next_right_pixel

%%border_2:
  mov ebx,[p_border2]
  shr ebx,4  ;pixels of right border/16
  DRAW_BORDER %1,ebx,%2

  mov ebx,[p_border2]
  and ebx,15
  jz short %%finished_border_2
  mov eax,[pal0]
%%extra_pix_border_2:

%if %2==0
  DRAWPIXEL %1,0 ;carefully
%elif %2==2
  DRAWPIXEL_LOWRES_4W %1
%else
  DRAWPIXEL_LOWRES_DW %1
%endif

  dec ebx
  jnz short %%extra_pix_border_2
%%finished_border_2:

  mov [_draw_dest_ad],edi  ;save dest ad - new!!!!!

  RESTORE_REGS

  leave
%endmacro


draw_scanline_32_lowres_pixelwise:
  DRAW_SCANLINE_PIXELWISE_LOWRES 4,0
  ret


draw_scanline_32_lowres_pixelwise_dw:
  DRAW_SCANLINE_PIXELWISE_LOWRES 4,1
  ret


; ---------------------------------------------------------------------------
; ------------------------------ _400 --------------------------------------
; ---------------------------------------------------------------------------
%macro GET_PC_DRAW_ADDR_INTO_EDI_400 0
  mov edi,[_draw_dest_ad]
  mov ecx,[_draw_line_length] ; offset for second line drawing offset
%endmacro
; ---------------------------------------------------------------------------
%macro DRAW_BORDER_400 2 ;bpp,how_many
  mov eax,[pal0]
%ifnidni %2,ebx
  mov ebx,%2
%endif

  jmp near %%next
%%for:
  %if %1==4
    mov [edi],eax
    %assign n 1
    %rep 31
      mov [edi+4*n],eax
      %assign n n+1
    %endrep
    mov [edi+ecx],eax
    %assign n 1
    %rep 31
      mov [edi+ecx+4*n],eax
      %assign n n+1
    %endrep
  %endif
  add edi,32*%1
%%next:
  dec ebx
  jns near %%for
%endmacro

%macro DRAWPIXEL_LOWRES_400 1 ;bpp
  %if %1==4
    ;mov eax,0
    mov [edi],eax      ; write colour to screen address
    mov [edi+4],eax
    mov [edi+ecx],eax  ; write colour to screen address + draw_line_length
    mov [edi+ecx+4],eax
  %endif
  add edi,2*%1         ; next screen address
%endmacro

%macro PUSHPIXEL_LOWRES 1 ;bpp
  %if %1==4
    mov [edi],eax      ; write colour to screen address
    mov [edi+4],eax
    push eax
  %endif
  add edi,%1 * 2
%endmacro

%macro POPRASTER_LOWRES 1 ;bpp
  lea eax,[edi+ecx-16*2 * %1] ;look at next line
  %if %1==4
    %assign n 15
    %rep 15
      pop ebx
      mov [eax+8*n],ebx
      mov [eax+8*n+4],ebx
      %assign n n-1
    %endrep
    pop ebx
    mov [eax],ebx
    mov [eax+4],ebx
  %endif
%endmacro

%macro POPPIXEL_LOWRES 1 ;bpp
;  lea eax,[edi+ecx-16*2 * %1] ;look at next line
  %if %1==4
    pop ebx
    sub eax,8
    mov [eax],ebx
    mov [eax+4],ebx
  %endif
%endmacro




%macro DRAW_SCANLINE_LOWRES_PIXELWISE_400 1 ;bpp
  ;jmp %%bye
  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 1,160
  GET_PC_DRAW_ADDR_INTO_EDI_400

  mov ebx,[p_border1]
  shr ebx,4  ;pixels of left border/16
  DRAW_BORDER_400 %1,ebx

  mov ebx,[p_border1]
  and ebx,15
  jz short %%finished_border_1
  mov eax,[pal0]
%%extra_pix_border_1:
  DRAWPIXEL_LOWRES_400 %1
  dec ebx
  jnz short %%extra_pix_border_1
%%finished_border_1:

  mov ebx,[p_picture]
  or ebx,ebx
  jz near %%border_2

  mov eax,16
  sub eax,[p_hscroll]  ;how many pixels to skip
  ;eax now contains how many pixels to draw in the first raster
  cmp eax,ebx           ;how many pixels to draw
  jl short %%no_more_reduction
  mov eax,ebx           ;there's less pixels to draw than remaining in the first raster
%%no_more_reduction:

  cmp eax,16
  je near %%middle_bit

  sub [p_picture],eax   ;reduce future number of pixels to draw
  mov [counter],eax ;store counter

  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address
  mov ecx,[p_hscroll]          ;copy to ecx ready for shl
  shl ebx,cl
  shl edx,cl  ;skip initial pixels

  mov ecx,[_draw_line_length] ; offset for second line drawing offset
  push dword[counter]

%%next_left_pixel:
  CALC_COL_LOWRES
  DRAWPIXEL_LOWRES_400 %1
  dec dword[esp]
  jnz near %%next_left_pixel
  pop eax

%%middle_bit:
  mov eax,[p_picture] ;number of pixels left
  shr eax,4           ;/16 to get number of full rasters
  push eax

  jmp near %%next_raster

%%draw_raster:
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address
  %rep 16
    CALC_COL_LOWRES
    PUSHPIXEL_LOWRES %1
  %endrep
  POPRASTER_LOWRES %1
%%next_raster:
  dec dword[esp]
  jns near %%draw_raster

  mov eax,[p_picture]
  and eax,15          ;extra pixels
  jz near %%finished_picture
  mov dword[esp],eax
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address

%%next_right_pixel:
  CALC_COL_LOWRES
  DRAWPIXEL_LOWRES_400 %1
  dec dword[esp]
  jnz near %%next_right_pixel

%%finished_picture:
  pop eax  ;correct stack, discard 0-counter

%%border_2:
  mov ebx,[p_border2]

  shr ebx,4  ;pixels of right border/16
  DRAW_BORDER_400 %1,ebx

  mov ebx,[p_border2]
  and ebx,15
  jz near %%finished_border_2
  mov eax,[pal0]
%%extra_pix_border_2:
  DRAWPIXEL_LOWRES_400 %1
  dec ebx
  jnz near %%extra_pix_border_2
%%finished_border_2:

  mov [_draw_dest_ad],edi  ;save dest ad - new!!!!!

  RESTORE_REGS
%%bye:
  leave
%endmacro

draw_scanline_32_lowres_pixelwise_400: ; Proc C border1:DWORD,picture:DWORD,border2:DWORD
  DRAW_SCANLINE_LOWRES_PIXELWISE_400 4
  ret




; ---------------------------------------------------------------------------
; -------------------------------- 4x ---------------------------------------
; ---------------------------------------------------------------------------

%macro GET_PC_DRAW_ADDR_INTO_EDI_4X 0
  mov edi,[_draw_dest_ad]
  mov ecx,[_draw_line_length] ; offset for second line drawing offset
%endmacro

; ---------------------------------------------------------------------------
%macro DRAW_BORDER_4X 2 ;bpp,how_many
  mov eax,[pal0]
%ifnidni %2,ebx
  mov ebx,%2
%endif

  jmp near %%next
%%for:
  %if %1==4
    %assign n 0
    %rep (32+32)
      mov [edi+4*n],eax
      %assign n n+1
    %endrep
    %assign n 0
    %rep (32+32)
      mov [edi+ecx+4*n],eax
      %assign n n+1
    %endrep
    add ecx,[_draw_line_length]
    %assign n 0
    %rep (32+32)
      mov [edi+ecx+4*n],eax
      %assign n n+1
    %endrep
    add ecx,[_draw_line_length]
    %assign n 0
    %rep (32+32)
      mov [edi+ecx+4*n],eax
      %assign n n+1
    %endrep
    mov ecx,[_draw_line_length]
  %endif
  add edi,2*32*%1
%%next:
  dec ebx
  jns near %%for
%endmacro

%macro DRAWPIXEL_LOWRES_4X 1 ;bpp
  %if %1==4
    mov [edi],eax      ; write colour to screen address
    mov [edi+4],eax
    mov [edi+8],eax
    mov [edi+12],eax
    mov [edi+ecx],eax  ; write colour to screen address + draw_line_length
    mov [edi+ecx+4],eax
    mov [edi+ecx+8],eax
    mov [edi+ecx+12],eax
    add ecx,[_draw_line_length]
    mov [edi+ecx],eax  ; write colour to screen address + draw_line_length*2
    mov [edi+ecx+4],eax
    mov [edi+ecx+8],eax
    mov [edi+ecx+12],eax
    add ecx,[_draw_line_length]
    mov [edi+ecx],eax  ; write colour to screen address + draw_line_length*3
    mov [edi+ecx+4],eax
    mov [edi+ecx+8],eax
    mov [edi+ecx+12],eax
    mov ecx,[_draw_line_length]
  %endif
%%bye:
  add edi,4*%1         ; next screen address
%endmacro

%macro PUSHPIXEL_LOWRES_4X 1 ;bpp
  %if %1==4
    mov [edi],eax      ; write colour to screen address
    mov [edi+4],eax
    mov [edi+8],eax
    mov [edi+12],eax
    push eax
  %endif
  add edi,%1 * 4
%endmacro

%macro POPRASTER_LOWRES_4X 1 ;bpp
  lea eax,[edi+ecx-16*4 * %1] ;look at next line
  %if %1==4
    %assign n 15
    %rep 16
      pop ebx
      mov [eax+16*n],ebx
      mov [eax+16*n+4],ebx
      mov [eax+16*n+8],ebx
      mov [eax+16*n+12],ebx
      push eax
      add eax,[_draw_line_length]
      mov [eax+16*n],ebx
      mov [eax+16*n+4],ebx
      mov [eax+16*n+8],ebx
      mov [eax+16*n+12],ebx
      add eax,[_draw_line_length]
      mov [eax+16*n],ebx
      mov [eax+16*n+4],ebx
      mov [eax+16*n+8],ebx
      mov [eax+16*n+12],ebx
      pop eax
      %assign n n-1
    %endrep
  %endif
%endmacro


%macro DRAW_LOWRES_4X 1 ;bpp

  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 1,160
  GET_PC_DRAW_ADDR_INTO_EDI_4X

  mov ebx,[p_border1]
  shr ebx,4  ;pixels of left border/16
  DRAW_BORDER_4X %1,ebx

  mov ebx,[p_border1]
  and ebx,15
  jz short %%finished_border_1
  mov eax,[pal0]
%%extra_pix_border_1:
  DRAWPIXEL_LOWRES_4X %1
  dec ebx
  jnz short %%extra_pix_border_1
%%finished_border_1:

  mov ebx,[p_picture]
  or ebx,ebx
  jz near %%border_2

  mov eax,16
  sub eax,[p_hscroll]  ;how many pixels to skip
  ;eax now contains how many pixels to draw in the first raster
  cmp eax,ebx           ;how many pixels to draw
  jl short %%no_more_reduction
  mov eax,ebx           ;there's less pixels to draw than remaining in the first raster
%%no_more_reduction:

  cmp eax,16
  je near %%middle_bit

  sub [p_picture],eax   ;reduce future number of pixels to draw
  mov [counter],eax ;store counter

  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address
  mov ecx,[p_hscroll]          ;copy to ecx ready for shl
  shl ebx,cl
  shl edx,cl  ;skip initial pixels

  mov ecx,[_draw_line_length] ; offset for second line drawing offset
  push dword[counter]

%%next_left_pixel:
  CALC_COL_LOWRES
  DRAWPIXEL_LOWRES_4X %1
  dec dword[esp]
  jnz near %%next_left_pixel
  pop eax

%%middle_bit:
  mov eax,[p_picture] ;number of pixels left
  shr eax,4           ;/16 to get number of full rasters
  push eax

  jmp near %%next_raster

%%draw_raster:
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address
  %rep 16
    CALC_COL_LOWRES
    PUSHPIXEL_LOWRES_4X %1
  %endrep
  POPRASTER_LOWRES_4X %1

%%next_raster:
  dec dword[esp]
  jns near %%draw_raster

  mov eax,[p_picture]
  and eax,15          ;extra pixels
  jz near %%finished_picture
  mov dword[esp],eax
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address

%%next_right_pixel:
  CALC_COL_LOWRES
  DRAWPIXEL_LOWRES_4X %1
  dec dword[esp]
  jnz near %%next_right_pixel

%%finished_picture:
  pop eax  ;correct stack, discard 0-counter

%%border_2:
  mov ebx,[p_border2]

  shr ebx,4  ;pixels of right border/16
  DRAW_BORDER_4X %1,ebx

  mov ebx,[p_border2]
  and ebx,15
  jz near %%finished_border_2
  mov eax,[pal0]
%%extra_pix_border_2:
  DRAWPIXEL_LOWRES_4X %1
  dec ebx
  jnz near %%extra_pix_border_2
%%finished_border_2:

  mov [_draw_dest_ad],edi  ;save dest ad - new!!!!!

  RESTORE_REGS

  leave
%%bye:
%endmacro



draw_scanline_32_lowres_4x: ; Proc C border1:DWORD,picture:DWORD,border2:DWORD
  DRAW_LOWRES_4X 4
  ret


; ---------------------------------------------------------------------------
; -------------------------------- 3X ---------------------------------------
; ---------------------------------------------------------------------------


; ---------------------------------------------------------------------------
%macro DRAW_BORDER_3X 2 ;bpp,how_many
  mov eax,[pal0]
%ifnidni %2,ebx
  mov ebx,%2
%endif

  jmp near %%next
%%for:
  %if %1==4
    %assign n 0
    %rep (32+16)
      mov [edi+4*n],eax
      %assign n n+1
    %endrep
    %assign n 0
    %rep (32+16)
      mov [edi+ecx+4*n],eax
      %assign n n+1
    %endrep
    add ecx,[_draw_line_length]
    %assign n 0
    %rep (32+16)
      mov [edi+ecx+4*n],eax
      %assign n n+1
    %endrep
    mov ecx,[_draw_line_length]
  %endif
  add edi,(16+32)*%1
%%next:
  dec ebx
  jns near %%for
%endmacro

%macro DRAWPIXEL_LOWRES_3X 1 ;bpp
  %if %1==4
    mov [edi],eax      ; write colour to screen address
    mov [edi+4],eax
    mov [edi+8],eax
    mov [edi+ecx],eax  ; write colour to screen address + draw_line_length
    mov [edi+ecx+4],eax
    mov [edi+ecx+8],eax
    add ecx,[_draw_line_length]
    mov [edi+ecx],eax  ; write colour to screen address + draw_line_length*2
    mov [edi+ecx+4],eax
    mov [edi+ecx+8],eax
    mov ecx,[_draw_line_length]
  %endif
%%bye:
  add edi,3*%1         ; next screen address
%endmacro

%macro PUSHPIXEL_LOWRES_3X 1 ;bpp
  %if %1==4
    mov [edi],eax      ; write colour to screen address
    mov [edi+4],eax
    mov [edi+8],eax
    push eax
  %endif
  add edi,%1 * 3
%endmacro

%macro POPRASTER_LOWRES_3X 1 ;bpp
  lea eax,[edi+ecx-16*3 * %1] ;look at next line
  %if %1==4
    %assign n 15
    %rep 16
      pop ebx
      ;xor ebx,ebx
      mov [eax+12*n],ebx
      mov [eax+12*n+4],ebx
      mov [eax+12*n+8],ebx
      push eax
      add eax,[_draw_line_length]
      mov [eax+12*n],ebx
      mov [eax+12*n+4],ebx
      mov [eax+12*n+8],ebx
      pop eax
      %assign n n-1
    %endrep
  %endif
%endmacro




%macro DRAW_LOWRES_3X 1 ;bpp
  ;jmp %%bye
  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 1,160
  GET_PC_DRAW_ADDR_INTO_EDI_4X

  mov ebx,[p_border1]
  shr ebx,4  ;pixels of left border/16
  DRAW_BORDER_3X %1,ebx

  mov ebx,[p_border1]
  and ebx,15
  jz short %%finished_border_1
  mov eax,[pal0]
%%extra_pix_border_1:
  DRAWPIXEL_LOWRES_3X %1
  dec ebx
  jnz short %%extra_pix_border_1
%%finished_border_1:

  mov ebx,[p_picture]
  or ebx,ebx
  jz near %%border_2

  mov eax,16
  sub eax,[p_hscroll]  ;how many pixels to skip
  ;eax now contains how many pixels to draw in the first raster
  cmp eax,ebx           ;how many pixels to draw
  jl short %%no_more_reduction
  mov eax,ebx           ;there's less pixels to draw than remaining in the first raster
%%no_more_reduction:

  cmp eax,16
  je near %%middle_bit

  sub [p_picture],eax   ;reduce future number of pixels to draw
  mov [counter],eax ;store counter

  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address
  mov ecx,[p_hscroll]          ;copy to ecx ready for shl
  shl ebx,cl
  shl edx,cl  ;skip initial pixels

  mov ecx,[_draw_line_length] ; offset for second line drawing offset
  push dword[counter]

%%next_left_pixel:
  CALC_COL_LOWRES
  DRAWPIXEL_LOWRES_3X %1
  dec dword[esp]
  jnz near %%next_left_pixel
  pop eax

%%middle_bit:
  mov eax,[p_picture] ;number of pixels left
  shr eax,4           ;/16 to get number of full rasters
  push eax

  jmp near %%next_raster

%%draw_raster:
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address
  %rep 16
    CALC_COL_LOWRES
    PUSHPIXEL_LOWRES_3X %1
  %endrep
  POPRASTER_LOWRES_3X %1

%%next_raster:
  dec dword[esp]
  jns near %%draw_raster

  mov eax,[p_picture]
  and eax,15          ;extra pixels
  jz near %%finished_picture
  mov dword[esp],eax
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address

%%next_right_pixel:
  CALC_COL_LOWRES
  DRAWPIXEL_LOWRES_3X %1
  dec dword[esp]
  jnz near %%next_right_pixel

%%finished_picture:
  pop eax  ;correct stack, discard 0-counter

%%border_2:
  mov ebx,[p_border2]

  shr ebx,4  ;pixels of right border/16
  DRAW_BORDER_3X %1,ebx

  mov ebx,[p_border2]
  and ebx,15
  jz near %%finished_border_2
  mov eax,[pal0]
%%extra_pix_border_2:
  DRAWPIXEL_LOWRES_3X %1
  dec ebx
  jnz near %%extra_pix_border_2
%%finished_border_2:

  mov [_draw_dest_ad],edi  ;save dest ad - new!!!!!

  RESTORE_REGS

  leave
%%bye:
%endmacro


draw_scanline_32_lowres_3x: ; Proc C border1:DWORD,picture:DWORD,border2:DWORD
  DRAW_LOWRES_3X 4
  ret



; ---------------------------------------------------------------------------
; -------------------------------- 4w ---------------------------------------
; ---------------------------------------------------------------------------


%macro DRAW_BORDER_4W 2 ;bpp,how_many
  mov eax,[pal0]
%ifnidni %2,ebx
  mov ebx,%2
%endif

  jmp near %%next
%%for:
  %if %1==4
    mov [edi],eax
    %assign n 1
    %rep (31+32)
      mov [edi+4*n],eax
      %assign n n+1
    %endrep
    ;mov [edi+ecx],eax
  %endif
  add edi,2*32*%1
%%next:
  dec ebx
  jns near %%for
%endmacro

%macro DRAWPIXEL_LOWRES_4W 1 ;bpp
  %if %1==4
    mov [edi],eax      ; write colour to screen address
    mov [edi+4],eax
    mov [edi+8],eax
    mov [edi+12],eax
  %endif
%%bye:
  add edi,4*%1         ; next screen address
%endmacro

%macro POPRASTER_LOWRES_4W 1 ;bpp
  lea eax,[edi+ecx-16*4 * %1] ;look at next line
  %if %1==4
    %assign n 15
    %rep 16
      pop ebx
      mov [eax+16*n],ebx
      mov [eax+16*n+4],ebx
      mov [eax+16*n+8],ebx
      mov [eax+16*n+12],ebx
      %assign n n-1
    %endrep
  %endif
%endmacro


%macro DRAW_LOWRES_4W 1 ;bpp
  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 1,160
  GET_PC_DRAW_ADDR_INTO_EDI

  mov ebx,[p_border1]
  shr ebx,4  ;pixels of left border/16
  DRAW_BORDER_4W %1,ebx

  mov ebx,[p_border1]
  and ebx,15
  jz short %%finished_border_1
  mov eax,[pal0]
%%extra_pix_border_1:
  DRAWPIXEL_LOWRES_4W %1
  dec ebx
  jnz short %%extra_pix_border_1
%%finished_border_1:

  mov ebx,[p_picture]
  or ebx,ebx
  jz near %%border_2

  mov eax,16
  sub eax,[p_hscroll]  ;how many pixels to skip
  ;eax now contains how many pixels to draw in the first raster
  cmp eax,ebx           ;how many pixels to draw
  jl short %%no_more_reduction
  mov eax,ebx           ;there's less pixels to draw than remaining in the first raster
%%no_more_reduction:

  cmp eax,16
  je near %%middle_bit

  sub [p_picture],eax   ;reduce future number of pixels to draw
  mov [counter],eax ;store counter

  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address
  mov ecx,[p_hscroll]          ;copy to ecx ready for shl
  shl ebx,cl
  shl edx,cl  ;skip initial pixels

;  mov ecx,[_draw_line_length] ; offset for second line drawing offset
  push dword[counter]

%%next_left_pixel:
  CALC_COL_LOWRES
  DRAWPIXEL_LOWRES_4W %1
  dec dword[esp]
  jnz near %%next_left_pixel
  pop eax

%%middle_bit:
  mov eax,[p_picture] ;number of pixels left
  shr eax,4           ;/16 to get number of full rasters
  push eax

  jmp near %%next_raster

%%draw_raster:
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address
  %rep 16
    CALC_COL_LOWRES
    DRAWPIXEL_LOWRES_4W %1
    ;PUSHPIXEL_LOWRES_4X %1
  %endrep
  ;POPRASTER_LOWRES_4W %1

%%next_raster:
  dec dword[esp]
  jns near %%draw_raster

  mov eax,[p_picture]
  and eax,15          ;extra pixels
  jz near %%finished_picture
  mov dword[esp],eax
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address

%%next_right_pixel:
  CALC_COL_LOWRES
  DRAWPIXEL_LOWRES_4W %1
  dec dword[esp]
  jnz near %%next_right_pixel

%%finished_picture:
  pop eax  ;correct stack, discard 0-counter

%%border_2:
  mov ebx,[p_border2]

  shr ebx,4  ;pixels of right border/16
  DRAW_BORDER_4W %1,ebx

  mov ebx,[p_border2]
  and ebx,15
  jz near %%finished_border_2
  mov eax,[pal0]
%%extra_pix_border_2:
  DRAWPIXEL_LOWRES_4W %1
  dec ebx
  jnz near %%extra_pix_border_2
%%finished_border_2:

  mov [_draw_dest_ad],edi  ;save dest ad - new!!!!!

  RESTORE_REGS

  leave
%%bye:
%endmacro



draw_scanline_32_lowres_4w: ; Proc C border1:DWORD,picture:DWORD,border2:DWORD
  ;DRAW_LOWRES_4W 4
  DRAW_SCANLINE_PIXELWISE_LOWRES 4,2
  ret


; ---------------------------------------------------------------------------
; -------------------------------- 3w ---------------------------------------
; ---------------------------------------------------------------------------

%macro DRAW_BORDER_3W 2 ;bpp,how_many
  mov eax,[pal0]
%ifnidni %2,ebx
  mov ebx,%2
%endif

  jmp near %%next
%%for:
  %if %1==4
    mov [edi],eax
    %assign n 1
    %rep (31+16)
      mov [edi+4*n],eax
      %assign n n+1
    %endrep
    ;mov [edi+ecx],eax
  %endif
  add edi,(16+32)*%1
%%next:
  dec ebx
  jns near %%for
%endmacro

%macro DRAWPIXEL_LOWRES_3W 1 ;bpp
  %if %1==4
    mov [edi],eax      ; write colour to screen address
    mov [edi+4],eax
    mov [edi+8],eax
  %endif
%%bye:
  add edi,3*%1         ; next screen address
%endmacro


%macro DRAW_LOWRES_3W 1 ;bpp
  ;jmp %%bye
  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 1,160
  GET_PC_DRAW_ADDR_INTO_EDI

  mov ebx,[p_border1]
  shr ebx,4  ;pixels of left border/16
  DRAW_BORDER_3W %1,ebx

  mov ebx,[p_border1]
  and ebx,15
  jz short %%finished_border_1
  mov eax,[pal0]
%%extra_pix_border_1:
  DRAWPIXEL_LOWRES_3W %1
  dec ebx
  jnz short %%extra_pix_border_1
%%finished_border_1:

  mov ebx,[p_picture]
  or ebx,ebx
  jz near %%border_2

  mov eax,16
  sub eax,[p_hscroll]  ;how many pixels to skip
  ;eax now contains how many pixels to draw in the first raster
  cmp eax,ebx           ;how many pixels to draw
  jl short %%no_more_reduction
  mov eax,ebx           ;there's less pixels to draw than remaining in the first raster
%%no_more_reduction:

  cmp eax,16
  je near %%middle_bit

  sub [p_picture],eax   ;reduce future number of pixels to draw
  mov [counter],eax ;store counter

  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address
  mov ecx,[p_hscroll]          ;copy to ecx ready for shl
  shl ebx,cl
  shl edx,cl  ;skip initial pixels

;  mov ecx,[_draw_line_length] ; offset for second line drawing offset
  push dword[counter]

%%next_left_pixel:
  CALC_COL_LOWRES
  DRAWPIXEL_LOWRES_3W %1
  dec dword[esp]
  jnz near %%next_left_pixel
  pop eax

%%middle_bit:
  mov eax,[p_picture] ;number of pixels left
  shr eax,4           ;/16 to get number of full rasters
  push eax

  jmp near %%next_raster

%%draw_raster:
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address
  %rep 16
    CALC_COL_LOWRES
    DRAWPIXEL_LOWRES_3W %1
  %endrep

%%next_raster:
  dec dword[esp]
  jns near %%draw_raster

  mov eax,[p_picture]
  and eax,15          ;extra pixels
  jz near %%finished_picture
  mov dword[esp],eax
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA ;also increments source address

%%next_right_pixel:
  CALC_COL_LOWRES
  DRAWPIXEL_LOWRES_3W %1
  dec dword[esp]
  jnz near %%next_right_pixel

%%finished_picture:
  pop eax  ;correct stack, discard 0-counter

%%border_2:
  mov ebx,[p_border2]

  shr ebx,4  ;pixels of right border/16
  DRAW_BORDER_3W %1,ebx

  mov ebx,[p_border2]
  and ebx,15
  jz near %%finished_border_2
  mov eax,[pal0]
%%extra_pix_border_2:
  DRAWPIXEL_LOWRES_3W %1
  dec ebx
  jnz near %%extra_pix_border_2
%%finished_border_2:

  mov [_draw_dest_ad],edi  ;save dest ad - new!!!!!

  RESTORE_REGS

  leave
%%bye:
%endmacro



draw_scanline_32_lowres_3w: ; Proc C border1:DWORD,picture:DWORD,border2:DWORD
  DRAW_LOWRES_3W 4
  ret


; SSE_VID_SINGLEPIX TODO

draw_scanline_32_lowres_pixelwise_dw_sp:
  ret

draw_scanline_32_lowres_pixelwise_400_sp:
  ret

draw_scanline_32_lowres_3x_sp:
  ret

draw_scanline_32_lowres_4x_dp:
  ret

draw_scanline_32_lowres_3w_sp:
  ret

draw_scanline_32_lowres_4w_dp:
  ret
