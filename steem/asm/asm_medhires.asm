; FILE: asm_medhires.asm
; MODULE: asm_draw
; DESCRIPTION: Various routines to draw the medium and high res ST screen to PC video memory.

; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
; !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! MEDIUM RES !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
cglobal draw_scanline_32_medres_pixelwise
cglobal draw_scanline_32_medres_pixelwise_400
cglobal draw_scanline_32_medres_4x
cglobal draw_scanline_32_medres_3x
cglobal draw_scanline_32_medres_2w


%macro GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES 0
  sub esi,4
  mov ebx,[esi]
%endmacro

; new version by Ant 14/1/2001
%macro CALC_COL_MEDRES 0
  test bh,80h
  jz short %%ccm_0x
%%ccm_1x:
  add ebx,ebx
  jnc short %%ccm_10
%%ccm_11:
  mov eax,[pal3]
  jmp short %%ccm_finished
%%ccm_10:
  mov eax,[pal2]
  jmp short %%ccm_finished
%%ccm_0x:
  add ebx,ebx
  jnc short %%ccm_00
%%ccm_01:
  mov eax,[pal1]
  jmp short %%ccm_finished
%%ccm_00:
  mov eax,[pal0]
%%ccm_finished:

%endmacro

%macro DRAWPIXEL_MEDRES_400 2 ;bpp, carelessly
  %if %1==4
    mov [edi],eax
    mov [edi+ecx],eax
  %endif
  add edi,%1
%endmacro

%macro DRAW_SCANLINE_PIXELWISE_MEDRES 1 ;bpp
  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 1,160
  GET_PC_DRAW_ADDR_INTO_EDI

  mov ebx,[p_border1]
  shr ebx,4  ;pixels of left border/16
  DRAW_BORDER %1,ebx,1

  mov ebx,[p_border1]
  and ebx,15
  jz %%finished_border_1
  mov eax,[pal0]
%%extra_pix_border_1:
  DRAWPIXEL %1,1
  DRAWPIXEL %1,0
  dec ebx
  jnz %%extra_pix_border_1
%%finished_border_1:

  mov ebx,[p_picture]
  add ebx,ebx
  mov [p_picture],ebx  ;double the low res pixels to get medres pixels

  or ebx,ebx
  jz near %%border_2

  mov eax,16
  mov ecx,[p_hscroll]  ;how many pixels to skip
  sub eax,ecx
  ;eax now contains how many pixels to draw in the first raster
  cmp eax,ebx           ;how many pixels to draw
  jl %%no_more_reduction
  mov eax,ebx           ;there's less pixels to draw than remaining in the first raster
%%no_more_reduction:

  cmp eax,16
  je %%middle_bit

  push eax ;store counter
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES ;also increments source address
  shl ebx,cl
   ;skip initial pixels

  pop ecx ;number of pixels to draw in first raster in ecx
  sub [p_picture],ecx   ;reduce future number of pixels to draw

%%next_left_pixel:
  CALC_COL_MEDRES
  DRAWPIXEL %1,0
  dec ecx
  jnz %%next_left_pixel

%%middle_bit:
  mov ecx,[p_picture] ;number of pixels left
  shr ecx,4           ;/16 to get number of full rasters

  jmp near %%next_raster
%%draw_raster:
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES ;also increments source address
  %rep 15
    CALC_COL_MEDRES
    DRAWPIXEL %1,1    ;draw pixel carelessly
  %endrep
  CALC_COL_MEDRES
  DRAWPIXEL %1,0    ;draw pixel carefully

%%next_raster:
  dec ecx
  jns near %%draw_raster

  mov ecx,[p_picture]
  and ecx,15          ;extra pixels
  jz %%border_2
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES ;also increments source address

%%next_right_pixel:
  CALC_COL_MEDRES
  DRAWPIXEL %1,0
  dec ecx
  jnz near %%next_right_pixel

%%border_2:
  mov ebx,[p_border2]
  shr ebx,4  ;pixels of right border/16
  DRAW_BORDER %1,ebx,1

  mov ebx,[p_border2]
  and ebx,15
  jz %%finished_border_2
  mov eax,[pal0]
%%extra_pix_border_2:
  DRAWPIXEL %1,1
  DRAWPIXEL %1,0
  dec ebx
  jnz %%extra_pix_border_2
%%finished_border_2:

  mov [_draw_dest_ad],edi  ;save dest ad - new!!!!!

  RESTORE_REGS

  leave
%endmacro


draw_scanline_32_medres_pixelwise:
  DRAW_SCANLINE_PIXELWISE_MEDRES 4
  ret

; ---------------------------------------------------------------------------
; ------------------------------ _400 --------------------------------------
; ---------------------------------------------------------------------------
%macro PUSHPIXEL_MEDRES 2 ;bpp,carelessly
  %if %1==4
    push eax      ;save colour on stack
    mov [edi],eax
    add edi,4
  %endif
%endmacro

%macro POPRASTER_MEDRES 1 ;bpp
  lea eax,[edi+ecx-16 * %1] ;look at next line
  %if %1==4
    %assign n 15
    %rep 15
      pop dword[eax+4*n]
      %assign n n-1
    %endrep
    pop dword[eax]
  %endif
%endmacro

%macro DRAW_SCANLINE_MEDRES_400 1 ;bpp
  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 0,160
  GET_PC_DRAW_ADDR_INTO_EDI_400
	DRAW_BORDER_400 %1,[border1]
  mov edx,[picture]
  jmp near %%next_raster
%%draw_raster:
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES
  %rep 15
    CALC_COL_MEDRES
    PUSHPIXEL_MEDRES %1,1 ;push pixel carelessly
  %endrep
  CALC_COL_MEDRES
  PUSHPIXEL_MEDRES %1,0  ;push pixel carefully
  POPRASTER_MEDRES %1
%%next_raster:
  dec edx  ;edx is counter here cos ecx is offset
  jns near %%draw_raster
	DRAW_BORDER_400 %1,[border2]
  RESTORE_REGS

  leave
%endmacro


draw_scanline_32_medres_400: ; Proc C border1:DWORD,picture:DWORD,border2:DWORD
  DRAW_SCANLINE_MEDRES_400 4
  ret


%macro DRAW_SCANLINE_MEDRES_PIXELWISE_400 1 ;bpp
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
  jz %%finished_border_1
  mov eax,[pal0]
%%extra_pix_border_1:
  DRAWPIXEL_LOWRES_400 %1
  dec ebx
  jnz %%extra_pix_border_1
%%finished_border_1:

  mov ebx,[p_picture]
  add ebx,ebx
  mov [p_picture],ebx
  or ebx,ebx
  jz near %%border_2

  mov eax,16
  sub eax,[p_hscroll]  ;how many pixels to skip
  ;eax now contains how many pixels to draw in the first raster
  cmp eax,ebx           ;how many pixels to draw
  jl %%no_more_reduction
  mov eax,ebx           ;there's less pixels to draw than remaining in the first raster
%%no_more_reduction:

  cmp eax,16
  je near %%middle_bit

  sub [p_picture],eax   ;reduce future number of pixels to draw
  mov [counter],eax ;store counter

  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES ;also increments source address
  mov ecx,[p_hscroll]          ;copy to ecx ready for shl
  shl ebx,cl
    ;skip initial pixels

  mov ecx,[_draw_line_length] ; offset for second line drawing offset
  push dword[counter]

%%next_left_pixel:
  CALC_COL_MEDRES
  DRAWPIXEL_MEDRES_400 %1,0
  dec dword[esp]
  jnz near %%next_left_pixel
  pop eax

%%middle_bit:
  mov eax,[p_picture] ;number of pixels left
  shr eax,4           ;/16 to get number of full rasters
  push eax

  jmp near %%next_raster

%%draw_raster:
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES
  %rep 15
    CALC_COL_MEDRES
    PUSHPIXEL_MEDRES %1,1
  %endrep
  CALC_COL_MEDRES
  PUSHPIXEL_MEDRES %1,0
  POPRASTER_MEDRES %1
%%next_raster:
  dec dword[esp]
  jns near %%draw_raster

  mov eax,[p_picture]
  and eax,15          ;extra pixels
  jz %%finished_picture
  mov dword[esp],eax
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES ;also increments source address

%%next_right_pixel:
  CALC_COL_MEDRES
  DRAWPIXEL_MEDRES_400 %1,0
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

  leave
%endmacro


draw_scanline_32_medres_pixelwise_400: ; Proc C border1:DWORD,picture:DWORD,border2:DWORD
  DRAW_SCANLINE_MEDRES_PIXELWISE_400 4
  ret



; ---------------------------------------------------------------------------
; -------------------------------- 4x ---------------------------------------
; ---------------------------------------------------------------------------

%macro DRAWPIXEL_MEDRES_4X 2 ;bpp, carelessly
  %if %1==4
    mov [edi],eax
    mov [edi+4],eax
    mov [edi+ecx],eax               ; 2nd line
    mov [edi+ecx+4],eax
    add ecx,[_draw_line_length]
    mov [edi+ecx],eax               ; 3rd line
    mov [edi+ecx+4],eax
    add ecx,[_draw_line_length]
    mov [edi+ecx],eax               ; 4th line
    mov [edi+ecx+4],eax
    mov ecx,[_draw_line_length]     ; restore
  %endif
  add edi,%1*2
%endmacro


%macro PUSHPIXEL_MEDRES_2W 2 ;bpp,carelessly
  %if %1==4
    push eax      ;save colour on stack
    mov [edi],eax
    mov [edi+4],eax
    add edi,8
  %endif
%endmacro

%macro POPRASTER_MEDRES_4X 1 ;bpp
  lea eax,[edi+ecx-16 * %1*2] ;look at next line
  %if %1==4
    %assign n 15
    %rep 16
      pop ebx
      mov [eax+8*n],ebx
      mov [eax+8*n+4],ebx
      push eax
      add eax,[_draw_line_length]
      mov [eax+8*n],ebx
      mov [eax+8*n+4],ebx
      add eax,[_draw_line_length]
      mov [eax+8*n],ebx
      mov [eax+8*n+4],ebx
      pop eax
      %assign n n-1
    %endrep
  %endif
%endmacro


%macro DRAW_MEDRES_4X 1 ;bpp
  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 1,160
  GET_PC_DRAW_ADDR_INTO_EDI_400

  mov ebx,[p_border1]
  shr ebx,4  ;pixels of left border/16
  DRAW_BORDER_4X %1,ebx

  mov ebx,[p_border1]
  and ebx,15
  jz %%finished_border_1
  mov eax,[pal0]
%%extra_pix_border_1:
  DRAWPIXEL_LOWRES_4X %1
  dec ebx
  jnz %%extra_pix_border_1
%%finished_border_1:

  mov ebx,[p_picture]
  add ebx,ebx
  mov [p_picture],ebx
  or ebx,ebx
  jz near %%border_2

  mov eax,16
  sub eax,[p_hscroll]  ;how many pixels to skip
  ;eax now contains how many pixels to draw in the first raster
  cmp eax,ebx           ;how many pixels to draw
  jl %%no_more_reduction
  mov eax,ebx           ;there's less pixels to draw than remaining in the first raster
%%no_more_reduction:

  cmp eax,16
  je near %%middle_bit

  sub [p_picture],eax   ;reduce future number of pixels to draw
  mov [counter],eax ;store counter

  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES ;also increments source address
  mov ecx,[p_hscroll]          ;copy to ecx ready for shl
  shl ebx,cl
    ;skip initial pixels

  mov ecx,[_draw_line_length] ; offset for second line drawing offset
  push dword[counter]

%%next_left_pixel:
  CALC_COL_MEDRES
  DRAWPIXEL_MEDRES_4X %1,0
  dec dword[esp]
  jnz near %%next_left_pixel
  pop eax

%%middle_bit:
  mov eax,[p_picture] ;number of pixels left
  shr eax,4           ;/16 to get number of full rasters
  push eax

  jmp near %%next_raster

%%draw_raster:
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES
  %rep 15
    CALC_COL_MEDRES
    PUSHPIXEL_MEDRES_2W %1,1
  %endrep
  CALC_COL_MEDRES
  PUSHPIXEL_MEDRES_2W %1,0
  POPRASTER_MEDRES_4X %1
%%next_raster:
  dec dword[esp]
  jns near %%draw_raster

  mov eax,[p_picture]
  and eax,15          ;extra pixels
  jz %%finished_picture
  mov dword[esp],eax
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES ;also increments source address

%%next_right_pixel:
  CALC_COL_MEDRES
  DRAWPIXEL_MEDRES_4X %1,0
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
%endmacro



draw_scanline_32_medres_4x: ; Proc C border1:DWORD,picture:DWORD,border2:DWORD
  DRAW_MEDRES_4X 4
  ret


; ---------------------------------------------------------------------------
; -------------------------------- 3x ---------------------------------------
; ---------------------------------------------------------------------------


%macro DRAWPIXEL_MEDRES_3X 2 ;bpp, carelessly
  %if %1==4
    mov [edi],eax
    mov [edi+4],eax
    mov [edi+ecx],eax               ; 2nd line
    mov [edi+ecx+4],eax
    add ecx,[_draw_line_length]
    mov [edi+ecx],eax
    mov [edi+ecx+4],eax
    mov ecx,[_draw_line_length]     ; restore
  %endif
  add edi,%1*2
%endmacro


%macro POPRASTER_MEDRES_3X 1 ;bpp
  lea eax,[edi+ecx-16 * %1*2] ;look at next line
  %if %1==4
    %assign n 15
    %rep 16
      pop ebx
      mov [eax+8*n],ebx
      mov [eax+8*n+4],ebx
      push eax
      add eax,[_draw_line_length]
      mov [eax+8*n],ebx
      mov [eax+8*n+4],ebx
      pop eax
      %assign n n-1
    %endrep
  %endif
%endmacro


%macro DRAW_MEDRES_3X 1 ;bpp
  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 1,160
  GET_PC_DRAW_ADDR_INTO_EDI_400

  mov ebx,[p_border1]
  shr ebx,4  ;pixels of left border/16
  DRAW_BORDER_4X %1,ebx

  mov ebx,[p_border1]
  and ebx,15
  jz %%finished_border_1
  mov eax,[pal0]
%%extra_pix_border_1:
  DRAWPIXEL_LOWRES_4X %1
  dec ebx
  jnz %%extra_pix_border_1
%%finished_border_1:

  mov ebx,[p_picture]
  add ebx,ebx
  mov [p_picture],ebx
  or ebx,ebx
  jz near %%border_2

  mov eax,16
  sub eax,[p_hscroll]  ;how many pixels to skip
  ;eax now contains how many pixels to draw in the first raster
  cmp eax,ebx           ;how many pixels to draw
  jl %%no_more_reduction
  mov eax,ebx           ;there's less pixels to draw than remaining in the first raster
%%no_more_reduction:

  cmp eax,16
  je near %%middle_bit

  sub [p_picture],eax   ;reduce future number of pixels to draw
  mov [counter],eax ;store counter

  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES ;also increments source address
  mov ecx,[p_hscroll]          ;copy to ecx ready for shl
  shl ebx,cl
    ;skip initial pixels

  mov ecx,[_draw_line_length] ; offset for second line drawing offset
  push dword[counter]

%%next_left_pixel:
  CALC_COL_MEDRES
  DRAWPIXEL_MEDRES_3X %1,0
  dec dword[esp]
  jnz near %%next_left_pixel
  pop eax

%%middle_bit:
  mov eax,[p_picture] ;number of pixels left
  shr eax,4           ;/16 to get number of full rasters
  push eax

  jmp near %%next_raster

%%draw_raster:
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES
  %rep 15
    CALC_COL_MEDRES
    PUSHPIXEL_MEDRES_2W %1,1
  %endrep
  CALC_COL_MEDRES
  PUSHPIXEL_MEDRES_2W %1,0
  POPRASTER_MEDRES_3X %1
%%next_raster:
  dec dword[esp]
  jns near %%draw_raster

  mov eax,[p_picture]
  and eax,15          ;extra pixels
  jz %%finished_picture
  mov dword[esp],eax
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES ;also increments source address

%%next_right_pixel:
  CALC_COL_MEDRES
  DRAWPIXEL_MEDRES_3X %1,0
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
%endmacro



draw_scanline_32_medres_3x: ; Proc C border1:DWORD,picture:DWORD,border2:DWORD
  DRAW_MEDRES_3X 4
  ret



; ---------------------------------------------------------------------------
; -------------------------------- 2w ---------------------------------------
; ---------------------------------------------------------------------------

%macro DRAWPIXEL_MEDRES_2W 2 ;bpp, carelessly
  %if %1==4
    mov [edi],eax
    mov [edi+4],eax
  %endif
  add edi,%1*2
%endmacro




%macro DRAW_MEDRES_2W 1 ;bpp
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
  jz %%finished_border_1
  mov eax,[pal0]
%%extra_pix_border_1:
  DRAWPIXEL_LOWRES_4W %1
  dec ebx
  jnz %%extra_pix_border_1
%%finished_border_1:

  mov ebx,[p_picture]
  add ebx,ebx
  mov [p_picture],ebx
  or ebx,ebx
  jz near %%border_2

  mov eax,16
  sub eax,[p_hscroll]  ;how many pixels to skip
  ;eax now contains how many pixels to draw in the first raster
  cmp eax,ebx           ;how many pixels to draw
  jl %%no_more_reduction
  mov eax,ebx           ;there's less pixels to draw than remaining in the first raster
%%no_more_reduction:

  cmp eax,16
  je near %%middle_bit

  sub [p_picture],eax   ;reduce future number of pixels to draw
  mov [counter],eax ;store counter

  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES ;also increments source address
  mov ecx,[p_hscroll]          ;copy to ecx ready for shl
  shl ebx,cl
    ;skip initial pixels

  ;mov ecx,[_draw_line_length] ; offset for second line drawing offset
  push dword[counter]

%%next_left_pixel:
  CALC_COL_MEDRES
  DRAWPIXEL_MEDRES_2W %1,0
  dec dword[esp]
  jnz near %%next_left_pixel
  pop eax

%%middle_bit:
  mov eax,[p_picture] ;number of pixels left
  shr eax,4           ;/16 to get number of full rasters
  push eax

  jmp near %%next_raster

%%draw_raster:
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES

  %rep 16
    CALC_COL_MEDRES
    DRAWPIXEL_MEDRES_2W %1,1   ; don't care
  %endrep

%%next_raster:
  dec dword[esp]
  jns near %%draw_raster

  mov eax,[p_picture]
  and eax,15          ;extra pixels
  jz %%finished_picture
  mov dword[esp],eax
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES ;also increments source address

%%next_right_pixel:
  CALC_COL_MEDRES
  DRAWPIXEL_MEDRES_2W %1,0
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
%endmacro



draw_scanline_32_medres_2w : ;Proc C border1:DWORD,picture:DWORD,border2:DWORD
  DRAW_MEDRES_2W 4
  ret




; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
; !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! HIGH RES !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
cglobal draw_scanline_32_hires

%macro GET_BLACK_AND_WHITE_INTO_EBX_EDX 0
  xor ebx,ebx
  xor edx,edx
  mov eax,[_STpal]
  and eax,1
  jnz short %%ebx_black
  not ebx
  jmp short %%end
%%ebx_black:
  not edx
%%end:
%endmacro

%macro GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_HIRES 0
  ; Get ST screen memory into regs
  sub esi,2
  mov ax,[esi]
%endmacro


%macro DRAW_BORDER_HIRES 2 ;bpp,how_many
  mov eax,%2

  push edx           ; save edx
  xor edx,edx        ; hires border is always black

  jmp short %%next
%%for:
  %if %1==4
    mov [edi],edx
    %assign n 1
    %rep 15
      mov [edi+4*n],edx
      %assign n n+1
    %endrep
  %endif
  add edi,16*%1
%%next:
  dec eax
  jns short %%for
  pop edx                    ; restore edx
%endmacro


%macro DRAWPIXEL_HIRES 2 ;bpp,carelessly
  %if %1==4
    jnc short %%no_carry
    mov [edi],ebx
    jmp short %%endtest
%%no_carry:
    mov [edi],edx
%%endtest:
    add edi,4
  %endif
%endmacro

%macro DRAW_SCANLINE_HIRES 1 ;bpp
  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 0,80
  GET_PC_DRAW_ADDR_INTO_EDI
  GET_BLACK_AND_WHITE_INTO_EBX_EDX
  DRAW_BORDER_HIRES %1,[border1]
  mov ecx,[picture]
  jmp near %%next_raster
%%draw_raster:
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_HIRES
  %rep 15
    add ax,ax
    DRAWPIXEL_HIRES %1,1
  %endrep
  add ax,ax
  DRAWPIXEL_HIRES %1,0
%%next_raster:
  dec ecx
  jns near %%draw_raster
  DRAW_BORDER_HIRES %1,[border2]
  RESTORE_REGS

  leave
%endmacro


draw_scanline_32_hires: ; Proc C border1:DWORD,picture:DWORD,border2:DWORD
  DRAW_SCANLINE_HIRES 4
  ret


; ---------------------------------------------------------------------------
; -------------------------------- 4x ---------------------------------------
; ---------------------------------------------------------------------------


cglobal draw_scanline_32_hires_4x,draw_scanline_32_hires_4x_scan


%macro DRAW_BORDER_HIRES_4X 2 ;bpp,how_many
  mov eax,%2

  push edx           ; save edx
  ;xor edx,edx        ; hires border is always black
  mov edx,[_draw_line_length] ; offset for second line drawing offset

  jmp %%next
%%for:
  %if %1==4
    %assign n 0
    %rep 16
      mov [edi+8*n],dword 0 ;edx
      mov [edi+8*n+4],dword 0 ; ,0 ;edx
      mov [edi+ecx+8*n],dword 0 ; ,0 ;edx
      mov [edi+ecx+8*n+4],dword 0 ; ,0 ;edx
      %assign n n+1
    %endrep
  %endif
  add edi,16*2*%1
%%next:
  dec eax
  jns %%for
  pop edx                    ; restore edx
%endmacro

%macro DRAWPIXEL_HIRES_2W 2 ;bpp,carelessly
  %if %1==4
    jnc short %%no_carry
    mov [edi],ebx
    mov [edi+4],ebx
    jmp short %%endtest
%%no_carry:
    mov [edi],edx
    mov [edi+4],edx
%%endtest:
    add edi,8
  %endif
%endmacro


%macro DRAW_SCANLINE_HIRES_4X 1 ;bpp
  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 0,80
  GET_PC_DRAW_ADDR_INTO_EDI
  GET_BLACK_AND_WHITE_INTO_EBX_EDX
  DRAW_BORDER_HIRES_4X %1,[border1]
  mov ecx,[picture]
  jmp %%next_raster
%%draw_raster:
  push edi
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_HIRES
  %rep 16
    add ax,ax
    DRAWPIXEL_HIRES_2W %1,1
  %endrep
  ; do second line
  pop eax   ; edi before raster
  push edi
  mov edi,eax
  add edi,[_draw_line_length]
  mov ax,[esi] ;=GET_SCREEN_DATA_INTO_REGS
  %rep 16
    add ax,ax
    DRAWPIXEL_HIRES_2W %1,1
  %endrep
  pop edi
%%next_raster:
  dec ecx
  jns %%draw_raster
  DRAW_BORDER_HIRES_4X %1,[border2]
  RESTORE_REGS
  leave
%endmacro



draw_scanline_32_hires_4x: ; Proc C border1:DWORD,picture:DWORD,border2:DWORD
  DRAW_SCANLINE_HIRES_4X 4
  ret





; ---------------------------------------------------------------------------
; -------------------------------- 2w ---------------------------------------
; ---------------------------------------------------------------------------


cglobal draw_scanline_32_hires_2w


%macro DRAW_BORDER_HIRES_2W 2 ;bpp,how_many
  mov eax,%2
  jmp %%next
%%for:
  %if %1==4
    %assign n 0
    %rep 16
      mov [edi+8*n],dword 0            ; possibly C++ intrinsics faster
      mov [edi+8*n+4],dword 0
      %assign n n+1
    %endrep
  %endif
  add edi,16*2*%1
%%next:
  dec eax
  jns %%for
  ;pop edx                    ; restore edx
%endmacro



%macro DRAW_SCANLINE_HIRES_2W 1 ;bpp
  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 0,80
  GET_PC_DRAW_ADDR_INTO_EDI
  GET_BLACK_AND_WHITE_INTO_EBX_EDX
  DRAW_BORDER_HIRES_2W %1,[border1]
  mov ecx,[picture]
  jmp %%next_raster
%%draw_raster:
  push edi
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_HIRES
  %rep 16
    add ax,ax
    DRAWPIXEL_HIRES_2W %1,1
  %endrep
%%next_raster:
  dec ecx
  jns %%draw_raster
  DRAW_BORDER_HIRES_2W %1,[border2]
  RESTORE_REGS
  leave
%endmacro



draw_scanline_32_hires_2w: ; Proc C border1:DWORD,picture:DWORD,border2:DWORD
  DRAW_SCANLINE_HIRES_2W 4
  ret



%macro DRAW_SCANLINE_HIRES_4X_SCAN 1 ;bpp
  push ebp
  mov ebp,esp

  SAVE_REGS
  GET_START 0,80
  GET_PC_DRAW_ADDR_INTO_EDI
  GET_BLACK_AND_WHITE_INTO_EBX_EDX
  DRAW_BORDER_HIRES_4X %1,[border1]
  mov ecx,[picture]
  jmp %%next_raster
%%draw_raster:
  push edi
  GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_HIRES
  %rep 16
    add ax,ax
    DRAWPIXEL_HIRES_2W %1,1
  %endrep
  ; do second line
  pop eax   ; edi before raster
  push edi
  mov edi,eax
  add edi,[_draw_line_length]
  ; second line is black
  ;DRAW_BORDER_HIRES_2W %1,1
;  ;mov ax,[esi] ;=GET_SCREEN_DATA_INTO_REGS
   xor eax,eax
  %rep 16
   ;add eax,eax
   ;DRAWPIXEL_HIRES_2W %1,1
  %endrep
  DRAW_BORDER_HIRES_2W %1,2
  pop edi
%%next_raster:
  dec ecx
  jns %%draw_raster
  DRAW_BORDER_HIRES_4X %1,[border2]
  RESTORE_REGS
  leave
%endmacro

draw_scanline_32_hires_4x_scan: ; Proc C border1:DWORD,picture:DWORD,border2:DWORD
  DRAW_SCANLINE_HIRES_4X_SCAN 4
  ret

; SSE_VID_SINGLEPIX TODO

draw_scanline_32_medres_3x_sp:
  ret

draw_scanline_32_medres_4x_sp:
  ret

draw_scanline_32_hires_4x_sp:
  ret

draw_scanline_32_medres_2w_sp:
  ret

draw_scanline_32_hires_2w_sp:
  ret
