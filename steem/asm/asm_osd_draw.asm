%include "macros.asm"

%ifndef MINGW

%ifdef WIN32
segment .bss public align=4 class=bss use32
%else
segment .bss
%endif

%else
segment .bss
%endif
; ---------------------------------------------------------------------------
osd_x:                           resd 1
osd_y:                           resd 1
osd_xi:                          resd 1
osd_w:                           resd 1
osd_yi:                          resd 1
osd_h:                           resd 1
; ---------------------------------------------------------------------------
cextern draw_mem,draw_line_length
; ---------------------------------------------------------------------------
%ifndef MINGW

%ifdef WIN32
segment .text public align=1 class=code use32
%else
segment .text
%endif

%else
segment .text
%endif
; ---------------------------------------------------------------------------
cglobal osd_draw_char_clipped_32

%define RECT_LEFT 0
%define RECT_TOP 4
%define RECT_RIGHT 8
%define RECT_BOTTOM 12

cliphandle: ; Proc lpcliprect:DWORD  ;;;,lpxy:DWORD;;;

  mov ecx,osd_x           ;[ecx] faster than [osd_x]?
  mov dword[ecx+8],0
  mov dword[ecx+12],32
  mov dword[ecx+16],0
  mov dword[ecx+20],32

  mov edx,[esp+4]         ;lpcliprect

  mov eax,[edx+RECT_LEFT] ;x-min
  sub eax,[ecx]           ;osd_x
  jl short check_right
  ; off left
  cmp eax,32
  jl short not_too_far_off_left
  mov eax,-32
  ret                     ;error

not_too_far_off_left:
  mov [ecx+8],eax         ;osd_xi
  sub [ecx+12],eax        ;osd_w
  mov eax,[edx+RECT_LEFT]
  mov [ecx],eax           ;set osd_x to clipped position

check_right:
  mov eax,[ecx]           ;osd_x
  add eax,32              ;right edge
  sub eax,[edx+RECT_RIGHT]
  jle short check_top

  cmp eax,32
  jl short not_too_far_off_right
  mov eax,-32
  ret                     ;error

not_too_far_off_right:
  sub [ecx+12],eax        ;osd_w

check_top:
  mov eax,[edx+RECT_TOP]  ;y-min
  sub eax,[ecx+4]         ;osd_y
  jl short check_bottom
  ; off top
  cmp eax,32
  jl short not_too_far_off_top
  mov eax,-32
  ret                     ;error

not_too_far_off_top:
  mov [ecx+16],eax        ;start further down
  sub [ecx+20],eax        ;draw less lines
  mov eax,[edx+RECT_TOP]
  mov [ecx+4],eax         ;set osd_y to clipped position

check_bottom:
  mov eax,[ecx+4]         ;osd_y
  add eax,32              ;bottom edge
  sub eax,[edx+RECT_BOTTOM]
  jle short done_clipping

  cmp eax,32
  jl short not_too_far_off_bottom
  mov eax,-32
  ret                     ;error

not_too_far_off_bottom:
  sub [ecx+20],eax

done_clipping:
  xor eax,eax
  ret

; ---------------------------------------------------------------------------
%define source_data ebp+8
%define dest_screen ebp+12
%define xp          ebp+16
%define yp          ebp+20
%define line_length ebp+24
%define col         ebp+28
%define char_h			ebp+32
%define lpcliprect  ebp+36

%define d_ad        ebp-4
%define end_ad      ebp-8
%define advance     ebp-12

%macro OSD_DRAW_CHAR_CLIPPED 2 ;bpp
  enter 12,0

; source data format: 1 long mask, 1 long data x 32

  pushad

  mov eax,[xp]
  mov [osd_x],eax
  mov eax,[yp]
  mov [osd_y],eax

  push dword[lpcliprect]
  call cliphandle
  add esp,4

;  mov dword[osd_xi],0  ;tes
;  mov dword[osd_w],32  ;   ting
;  mov dword[osd_yi],0  ;        without
;  mov dword[osd_h],32  ;                cliphandle
;  mov eax,0

  cmp eax,0
  jne near %%the_end           ;error exit

  mov ebx,[osd_x]
  mov eax,[osd_y]         ;y position
  mul dword[line_length]  ;times bytes per line of dest
  %if %1==2
    add ebx,ebx
  %elif %1==3
    add ebx,ebx
    add ebx,[osd_x]
  %elif %1==4
    shl ebx,2
  %endif

  add eax,ebx
  add eax,[dest_screen]     ;eax now has pixel address
  mov [d_ad],eax            ;dest address
  mov edi,eax               ;use edi as draw pointer

  mov eax,[osd_yi]          ;number of lines to skip in pattern
  shl eax,3                 ;*8 bytes per line
  add eax,[source_data]     ;yeilds source data
  mov esi,eax               ;esi is source

  mov ecx,[line_length]
  mov ebx,[osd_w]           ;number of pixels per line
  %if %1==4
    shl ebx,2
  %endif
  sub ecx,ebx               ;number of bytes to advance after end of line
  mov [advance],ecx         ;into variable "advance"

  %if %2==0
    mov ebx,[col]             ;use ebx as colour
  %endif

%%draw_line_clipped:
  mov ecx,[osd_xi]          ;how much to knock off start
  mov eax,[esi]             ;mask
  shl eax,cl                ;skip the first few
  mov edx,[esi+4]           ;data
  shl edx,cl                ;skip the first few

  add esi,8

  mov ecx,[osd_w]  ;counter
%%draw_pixel:
  add eax,eax
  jnc short %%eax_no_carry
  %if %2
    push eax
    %if %1==2
      call darken_pixel_to_eax_16
    %elif %1==3
      call darken_pixel_to_eax_24
    %elif %1==4
      call darken_pixel_to_eax_32
    %endif
  %endif
  %if %1==4
    %if %2==0
      mov dword[edi],0
    %else
      mov dword[edi],eax
    %endif
  %endif
  %if %2
    pop eax
  %endif

%%eax_no_carry:

  add edx,edx
  jnc short %%edx_no_carry
  %if %2
    push eax
    %if %1==4
      call lighten_pixel_to_ebx_32
    %endif
  %endif
  %if %1==4
    mov [edi],ebx
  %endif
  %if %2
    pop eax
  %endif

%%edx_no_carry:

  %if %1==1
    inc edi
  %else
    add edi,%1
  %endif
  loop %%draw_pixel

  add edi,dword[advance] ;point to start of next line
  dec dword[char_h]
  jnle short %%draw_line_clipped

%%the_end:
  popad
  leave

%endmacro


osd_draw_char_clipped_32: ; Proc C source_data:DWORD, dest_screen:DWORD,xp:DWORD,yp:DWORD,line_length:DWORD,col:DWORD,lpcliprect:DWORD
  OSD_DRAW_CHAR_CLIPPED 4,0
  ret

; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
cglobal osd_draw_char_32

%macro GENERATE_LABEL 4
osd_draw_char_pixel_%1_%2_%3_%4:
%endmacro


%macro DARKEN_PIXEL_24_32 1
  mov eax,[edi]

  mov ebx,eax
  and ebx,0x0000ff
  cmp ebx,0x80
  jg  short %%no_overflow_b
  xor ebx,ebx
  jmp short %%done_b
%%no_overflow_b:
  sub ebx,0x80
%%done_b:
  and eax,0xffff00
  or eax,ebx

  mov ebx,eax
  and ebx,0x00ff00
  cmp ebx,0x8000
  jg  short %%no_overflow_g
  xor ebx,ebx
  jmp short %%done_g
%%no_overflow_g:
  sub ebx,0x8000
%%done_g:
  and eax,0xff00ff
  or eax,ebx

  mov ebx,eax
  and ebx,0xff0000
  cmp ebx,0x800000
  jg  short %%no_overflow_r
  xor ebx,ebx
  jmp short %%done_r
%%no_overflow_r:
  sub ebx,0x800000
%%done_r:
  and eax,0x00ffff
  or eax,ebx
%endmacro


darken_pixel_to_eax_32:
  DARKEN_PIXEL_24_32 4
  ret



%macro LIGHTEN_PIXEL_24_32 1
  mov ebx,[edi]

  mov eax,ebx
  and eax,0x0000ff
  cmp eax,0x0000ff-0x80
  jl  short %%no_overflow_b
  mov eax,0x0000ff
  jmp short %%done_b
%%no_overflow_b:
  add eax,0x80
%%done_b:
  and ebx,0xffff00
  or ebx,eax

  mov eax,ebx
  and eax,0x00ff00
  cmp eax,0x00ff00-0x8000
  jl  short %%no_overflow_g
  mov eax,0x00ff00
  jmp short %%done_g
%%no_overflow_g:
  add eax,0x8000
%%done_g:
  and ebx,0xff00ff
  or ebx,eax

  mov eax,ebx
  and eax,0xff0000
  cmp eax,0xff0000-0x800000
  jl  short %%no_overflow_r
  mov eax,0xff0000
  jmp short %%done_r
%%no_overflow_r:
  add eax,0x800000
%%done_r:
  and ebx,0x00ffff
  or ebx,eax
%endmacro


lighten_pixel_to_ebx_32:
  LIGHTEN_PIXEL_24_32 4
  ret

%macro OSD_DRAW_CHAR 3 ;bpp,rainbow,transparent
  enter 12,0

; source data format: 1 long mask, 1 long data x 32

  pushad

  mov ebx,[xp]             ;x position
  mov eax,[yp]             ;y position
  mul dword[line_length]   ;times bytes per line of dest
  %if %1==4
    shl ebx,2
  %endif

  add eax,ebx
  add eax,[dest_screen]    ;eax now has pixel address
  mov [d_ad],eax           ;dest address
  mov edi,eax              ;use edi as draw pointer
  mov esi,[source_data]    ;esi is source

  mov ecx,[line_length]
  sub ecx,31*%1            ;number of bytes to advance after end of line
  mov [advance],ecx        ;into variable "advance"

  mov ebx,eax              ;eax is source address
  mov eax,[line_length]
	mul dword[char_h]
  add eax,ebx              ;eax points to end of char as dest address
  mov [end_ad],eax

  %if %3==0 || %1==1
    %if %2
      mov edx,[col]
      mov eax,[edx]          ;get current colour
      mov ebx,eax
      %if %1==2
        call advance_colour_16
      %elif %1==3 || %1==4
        call advance_colour_24
      %endif
      mov [edx],ebx ;update colour for next time
      mov ebx,eax   ;use ebx as colour
    %else
      mov ebx,[col]   ;use ebx as colour
    %endif
    xor eax,eax              ;use eax to store 0
  %endif

%%draw_line:
  mov ecx,[esi]            ;mask
  mov edx,[esi+4]          ;data
  add esi,8


  %assign n 1
  %assign numreps 32
  %rep numreps
    add ecx,ecx
    jnc short .ecx_no_carry
    %if %3
      %if %1==4
        call darken_pixel_to_eax_32
      %endif
    %endif
    %if %1==4
      mov [edi],eax
    %endif
.ecx_no_carry:
    add edx,edx
    %if %1==1 && %3 && n>1
      add edx,edx
    %endif
    jnc short .edx_no_carry
    %if %3
      %if %1==4
        call lighten_pixel_to_ebx_32
      %endif
    %endif
    %if %1==4
      mov [edi],ebx
    %endif
.edx_no_carry:

    %if n<32
      GENERATE_LABEL %1,%2,%3,n
      %assign n n+1
      add edi,%1
    %endif
  %endrep

  %if %2
      call advance_colour_24
  %endif

  add edi,[advance] ;point to start of next line
  cmp edi,[end_ad]
  jl near %%draw_line

  popad
  leave
%endmacro


osd_draw_char_32: ; Proc C source_data:DWORD, dest_screen:DWORD,x:DWORD,y:DWORD,line_length:DWORD,col:DWORD
  OSD_DRAW_CHAR 4,0,0
  ret

; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
cglobal osd_draw_char_transparent_32
cglobal osd_draw_char_clipped_transparent_32


osd_draw_char_transparent_32: ; Proc C source_data:DWORD, dest_screen:DWORD,x:DWORD,y:DWORD,line_length:DWORD,col:DWORD
  OSD_DRAW_CHAR 4,0,1
  ret

osd_draw_char_clipped_transparent_32:
  OSD_DRAW_CHAR_CLIPPED 4,1
  ret


%undef source_data
%undef dest_screen
%undef xp
%undef yp
%undef line_length
%undef col
%undef lpcliprect
%undef char_h

%undef d_ad
%undef end_ad
%undef advance
; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
cglobal osd_blueize_line_32

%define x1    ebp+8
%define y     ebp+12
%define w     ebp+16


osd_blueize_line_32: ; Proc C x1:DWORD, y:DWORD, w:DWORD
  push ebp
  mov ebp,esp

  pushad
  mov eax,[y]
  mul dword[_draw_line_length]
  add eax,[_draw_mem]

  mov ebx,[x1]
  lea eax,[eax+ebx*4]

  mov ecx,[w]
  cmp ecx,0
  jle short enough_32

blueize_32:
  mov ebx,[eax]
  and ebx,0FEFE00h
  shr ebx,1
  mov bl,0D3h
  mov [eax],ebx
  add eax,4
  loop blueize_32

enough_32:
  popad
  leave
  ret

%undef x1
%undef y
%undef w
; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
cglobal osd_black_box_32;

%define dest_screen ebp+8
%define x           ebp+12
%define y           ebp+16
%define w           ebp+20
%define h           ebp+24
%define line_length ebp+28
%define col 				  ebp+32

%macro DRAWBLACKPIXEL 1
  %if %1==4
    mov [eax],edx
  %endif
%endmacro

%macro DRAW_BLACK_BOX 1
  push ebp
  mov ebp,esp

  pushad

  dec word[w]
  js short %%finished_black_rect		; jump signed (negative)
  jz short %%finished_black_rect		

	; get w offset into ebx
  mov ax,[w]       
	dec ax						;to start of right line
  cwde							;sign extend
  mov ebx,eax				;to ebx
  %if %1==4
    shl ebx,2
  %endif

	; get x position into ecx
  mov ax,[x]        ;x position
  cwde              ;sign extend
  mov ecx,eax       ;to ecx
  %if %1==4
    shl ecx,2
  %endif

  mov ax,[y]              ;y position
  cwde                    ;to long
  mul dword[line_length]  ;times bytes per line of dest
  add eax,ecx				  ;add x position
  add eax,[dest_screen]   ;eax now has pixel address

  xor edx,edx				  ;edx contains colour

  dec word[h]
  js short %%finished_black_rect		; jump signed (negative)

	mov edi,eax					;save address
  mov cx,[w]
%%top_rect_line:
  DRAWBLACKPIXEL %1
  add eax,%1
  dec cx
  ja short %%top_rect_line
  add edi,[line_length]

  dec word[h]
  jz short %%do_bottom_line
  js short %%finished_black_rect		; jump signed (negative)

  mov cx,[h]
%%middle_lines:
	mov eax,edi
  DRAWBLACKPIXEL %1
  add eax,ebx
  DRAWBLACKPIXEL %1
  add edi,[line_length]
  dec cx
  ja short %%middle_lines

%%do_bottom_line:
	mov eax,edi
	mov cx,[w]
%%bottom_rect_line:
  DRAWBLACKPIXEL %1
  add eax,%1
  dec cx
  ja short %%bottom_rect_line

%%finished_black_rect:
  popad
  leave
%endmacro


osd_black_box_32: ; Proc C dest_screen:DWORD,x:WORD,y:WORD,w:WORD,h:WORD,line_length:DWORD
  DRAW_BLACK_BOX 4
  ret

%undef dest_screen
%undef x
%undef y
%undef w
%undef h
%undef line_length 
%undef col

