/*---------------------------------------------------------------------------
PROJECT: Steem SSE
Atari ST emulator
Copyright (C) 2026 by Anthony Hayward and Russel Hayward + SSE

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see https://www.gnu.org/licenses/.

DOMAIN: I/O
FILE: printer.cpp
CONDITION: SSE_PRINTER or SSE_ACSI_LASER must be defined
DESCRIPTION: Transforming printer output into useful RTF or PBM files.
For the laser printer, code is shared between this file and acsi.cpp.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <computer.h>
#include <osd.h>
#include <debug.h>

#if defined(SSE_PRINTER) 
/* 
SMM804 9pin dot matrix printer emulation
It's an Epson-compatible printer by Atari
We don't emulate everything: horizontal tabs, different feed sizes, etc.
References: Epson ESC/P Manual, ST ASCII table, Windows-1252, RTF Specification
*/

#define TRACEP TRACE_LOG
#define LOGSECTION LOGSECTION_PORTS

TSMM804::TSMM804() {
  fp_rtf=fp_pbm=NULL;
  Reset();
}


TSMM804::~TSMM804() {
  ASSERTB(!pbm_descr.data);
#ifndef SSE_LEAN_AND_MEAN
  if(pbm_descr.data)
    free(pbm_descr.data);
#endif
}


void TSMM804::Bold(BYTE const p) {
  if(p==1||p=='1')
  {
    if(!bBold)
      fprintf(fp_rtf,"\\b ");
    bBold=true;
    TRACEP(" Bold on");
  }
  else if(p==0||p=='0')
  {
    if(bBold)
      fprintf(fp_rtf,"\\b0 ");
    bBold=false;
    TRACEP(" Bold off");
  }
}


void TSMM804::Italics(BYTE const p) {
  if(p==1||p=='1')
  {
    if(!bItalics)
      fprintf(fp_rtf,"\\i ");
    bItalics=true;
    TRACEP(" Italics on");
  }
  else if(p==0||p=='0')
  {
    if(bItalics)
      fprintf(fp_rtf,"\\i0 ");
    bItalics=false;
    TRACEP(" Italics off");
  }
}


void TSMM804::Underline(BYTE const p) {
  if(p==1||p=='1')
  {
    if(!bUnderline)
      fprintf(fp_rtf,"\\ul ");
    bUnderline=true;
    TRACEP(" Underline on");
  }
  else if(p==0||p=='0')
  {
    if(bUnderline)
      fprintf(fp_rtf,"\\ul0 ");
    bUnderline=false;
    TRACEP(" Underline off");
  }
}


void TSMM804::Close(FILE* const fp) {
  if(fp_rtf && (fp_rtf==fp||!fp))
  {
    fprintf(fp_rtf,"}"); // write final }
    fclose(fp_rtf);
    fp_rtf=NULL;
  }
  if(fp_pbm && (fp_pbm==fp||!fp))
  {
    pbm_save(&pbm_descr,fp_pbm);
    fclose(fp_pbm);
    fp_pbm=NULL;
    free(pbm_descr.data);
    pbm_descr.data=NULL;
    GraphicsIdx=0;
  }
}


void TSMM804::HandleByte(BYTE Byte) {
  // create RTF file on first printer use of this Steem session
  if(fp_rtf==NULL)
  {
    EasyStr Path=STPort[TSTPort::PARALLEL].File;
    RemoveFileNameFromPath(Path,WITH_SLASH);
    Path+="SMM804_";
    Path+=(EasyStr("00000")+SSEConfig.PageRtf++).Rights(5);
    Path+=".rtf";
    fp_rtf=fopen(Path,"wb");
    TRACEB("%s %p\n",CHECKPATH(Path.Text),fp_rtf);
    if(!fp_rtf)
      return;
    StartRtf(fp_rtf);
    fprintf(fp_rtf,"\\f2 \\fs16");
  }
#ifndef SSE_NO_OSD
#ifdef WIN32
  if(!OsdControl.bPrinting)
  {
    UPDATE_STATUS_BAR_PART(SB_PART_ICONS);
    SetTimer(StemWin,STATUSBAR_TIMER_ID,3000,NULL); //3s
  }
  OsdControl.bPrinting=true;
#elif defined(SSE_OSD_DEBUGINFO)
  TRACE_OSD_DBG("PRT");
#endif
#endif
  char nl[]="\\par "; // \\sb0 \\sb0 = default
  if(ReceivingCommand && CommandParameter<=32)
  {
    Command[CommandParameter]=Byte;
    if(!CommandParameter)
    {
      TRACEP("\nESC/P %c %d",Byte,Byte);
      switch(Byte) {
      case 13:
      case '!': // Global formatting
      case '-': // Apply/cancel underlining
      case '3': // Specify line feed of n/180 inch
      case 'A': // Specify line feed of n/60 inch
      case 'C': // Page length in lines
      case 'J': // Forward paper feed
      case 'N': // Set bottom margin
      case 'R': // Select international character set
      case 'S': // Superscript/subscript
      case 'W': // Specify double-width characters
      case 'X': // Specify character size
      case 'a': // Specify alignment
      case 'k': // Select font
      case 'r': // Select colour
      case 't': // Select character code table
      case 'w':
        nParameters=1;
        break;
      case '$': // Specify absolute horizontal position
      case '\\': // Specify relative horizontal position
      case 'i': // Advanced commands
      case 'K': // 8-dot single-density bit image
      case 'L': // 8-dot double-density bit image
      case 'Y': // 8-dot double-speed double-density bit image
      case 'Z': // 8-dot quadruple-density bit image
        nParameters=2;
        break;
      case '*': // Select bit image
        nParameters=3;
        break;
      case 'B': // Set vertical tabs
        nParameters=16; // max
        break;
      case 'D': // Set horizontal tabs
        nParameters=32; // max
        break;
      default:
        nParameters=0;
      }//sw
    }
    else
    {
      TRACEP(" %d",Byte);
    }
    if(CommandParameter==nParameters
      || (Command[0]=='B'||Command[0]=='D')
      && (Byte==0||Byte<Command[CommandParameter]))
    {
      ReceivingCommand=false;
      switch(Command[0]) {
      case '!':
        Bold( (Byte&(BIT_4|BIT_3)) ? '1' : '0');
        Italics( (Byte&BIT_6) ? '1' : '0');
        Underline( (Byte&BIT_7) ? '1' : '0');
        break; //todo -> little f(p) for biu
      case '-':
        Underline(Command[1]);
        break;
      case '4':
        Italics('1');
        break;
      case '5':
        Italics('0');
        break;
      case '@':
        Reset();
        TRACEP(" Reset");
        break;
      case 'B': // used by First Word
        TRACEP(" Vertical tabs");
        for(int i=0;i<CommandParameter;i++)
          VTab[i]=((Command[1]>0) ? Command[i+1] : 0);
        break;
      case 'D': // not used (so far), not implemented
        TRACEP(" Horizontal tabs");
      /*for(int i=0;i<CommandParameter;i++)
          HTab[i]=((Command[1]>0) ? Command[i+1] : 0);*/
        break;
      case 'E':
      case 'G':
        Bold('1');
        break;
      case 'F':
      case 'H':
        Bold('0');
        break;
      case 'R':
        International=Byte;
        TRACEP(" International set");
        break;
      case 'S':
        if(Command[1]==0 || Command[1]=='0')
        {
          if(!bSuperscript)
            fprintf(fp_rtf,"{\\super ");
          bSuperscript=true;
          TRACEP(" Superscript on");
        }
        else
        {
          if(!bSubscript)
            fprintf(fp_rtf,"{\\sub ");
          bSubscript=true;
          TRACEP(" Subscript on");
        }
        break;
      case 'T':
        if(bSubscript)
          fprintf(fp_rtf,"}");
        if(bSuperscript)
          fprintf(fp_rtf,"}");
        bSubscript=bSuperscript=false;
        TRACEP(" Sub/Superscript off");
        break;
      case 'K':
      case 'L': // this is the one used by Degas to print a picture
      case 'Y': // this is the one used by TOS to print the screen (alt-help or menu)
      case 'Z':
        GraphicColumns=Command[2]*256+Command[1]; // n1 + n2 * 256 bytes
        GraphicsIdx=pbm_descr.width*8*GraphicBand*2;
        TRACEP(" Graphics band %d %d columns",GraphicBand,GraphicColumns);
        GraphicBand++;
        break;
      case 'r':
        Colour=Byte;
        TRACEP(" Colour");
        break;
      }//sw
      TRACEP("\n");
    }
    CommandParameter++;
  }
  else if(GraphicColumns>0) // we're printing graphics
  {
    // low-level transpose, bytes are 8pixel-high columns
    int bits=Byte;
    for(int r=0;r<8;r++)
    {
      if(!pbm_descr.data)
      {
        // create PBM file on first graphics printer use and allocate memory
        EasyStr Path=ParallelPort.File;
        RemoveFileNameFromPath(Path,WITH_SLASH);
        Path+="SMM804_";
        Path+=(EasyStr("00000")+SSEConfig.PagePbm++).Rights(5);
        Path+=".pbm";
        fp_pbm=fopen(Path,"wb");
        TRACEB("%s %p\n",CHECKPATH(Path.Text),fp_pbm);
        pbm_descr.data=(BYTE*)malloc(pbm_descr.size);
        ZeroMemory(pbm_descr.data,pbm_descr.size);
      }
      else if((DWORD)((GraphicsIdx/8)+(r*2+1)*pbm_descr.width)>=pbm_descr.size)
      {
        TRACEP("grx ovf\n");
        Close(fp_pbm);
      }
      else if(bits&0x80) // pixel on
      {
        pbm_descr.data[(GraphicsIdx/8)+r*2*pbm_descr.width/8]|=(0x80>>(GraphicsIdx%8));
        pbm_descr.data[(GraphicsIdx/8)+(r*2+1)*pbm_descr.width/8] // double
          |=(0x80>>(GraphicsIdx%8));
      }
      else // pixel off
      {
        pbm_descr.data[(GraphicsIdx/8)+r*2*pbm_descr.width/8]&=~(0x80>>(GraphicsIdx%8));
        pbm_descr.data[(GraphicsIdx/8)+(r*2+1)*pbm_descr.width/8] // double
          &=~(0x80>>(GraphicsIdx%8));
      }
      bits<<=1;
    }//nxt
    GraphicsIdx++;
    GraphicColumns--;
  }
  else if(Byte==10&&LastChar==13 || Byte==13&&LastChar==10)
  {
    TRACEP("LF %d VPos %d\n",Byte,VPos);
    Byte=0; // arbitrary
  }
  else if(Byte==10||Byte==13)
  { // most printers do the same on CR or LF or CR LF or LF CR
    //HPos=0;
    VPos++;
    TRACEP("LF %d VPos %d\n",Byte,VPos);
    fprintf(fp_rtf,"%s",nl);
  }
  else
  {
#if defined(SSE_ENABLE_TRACE_LOG)
    BYTE oldByte=Byte;
#endif
    bool Ignore=(Byte<32);
    switch(Byte) { // long switch, execution should be fast, not that it matters
    case 9: // HT
      Ignore=false; //?
      break;
    case 11: // VT
    {
      TRACEP("VT %d ->",VPos);
      //HPos=0;
      int i;
      for(i=0;i<16;i++)
      {
        if(VPos<VTab[i])
        {
          for(;VPos<VTab[i];VPos++)
            FPUTC(Byte,fp_rtf);
          break;
        }
      }//nxt
      if(VPos<VTab[i])
      {
        if(!(VTab[0]))
        {
          FPUTC(Byte,fp_rtf);
          VPos++;
        }
        else
        {
          fprintf(fp_rtf,"\\page ");
          VPos=0;
        }
      }
      TRACEP("%d\n",VPos);
      break;
    }
    case 12: // FF
      TRACEP("FF\n");
      if(pbm_descr.data)
        Close(fp_pbm);
      fprintf(fp_rtf,"\\page ");
      VPos=0;
      break;
    case 27: // ESC
      ReceivingCommand=true;
      CommandParameter=0;
      break;
    // International character set
    case 0x23:
      switch(International) {
      case UK:
        Byte=0xa3;
        break;
      case SPI: 
        Byte='P';  // Pt, not on 1252
        break;
      }
      break;
    case 0x24:
      switch(International) {
      case SW: case NW:
        Byte=0xa4; 
        break;
      }
      break;
    case 0x40:
      switch(International) {
      case FR:
        Byte=0xe0;
        break;
      case DE: case LEG:
        Byte=0xa7;
        break;
      case SW: case NW: case DKII:
        Byte=0xc9;
        break;
      case SPII: case LAT:
        Byte=0xe1;
        break;
      }
      break;
    case 0x5b:
      switch(International) {
      case FR: case IT: case LEG:
        Byte=0xb0;
        break;
      case DE: case SW:
        Byte=0xc4;
        break;
      case DKI: case NW: case DKII:
        Byte=0xc6;
        break;
      case SPI: case SPII: case LAT:
        Byte=0xa1;
        break;
      }
      break;
    case 0x5c:
      switch(International) {
      case FR:
        Byte=0xe7;
        break;
      case DE: case SW:
        Byte=0xd6;
        break;
      case DKI: case NW: case DKII:
        Byte=0xd8;
        break;
      case SPI: case SPII: case LAT:
        Byte=0xd1;
        break;
      case JP:
        Byte=0xa5;
        break;
      case KO:
        Byte='W'; // wuan, not on 1252
        break;
      case LEG:
        Byte=0xb4;
        break;
      default:
        FPUTC(Byte,fp_rtf); // twice
      }
      break;
    case 0x5d:
      switch(International) {
      case FR:
        Byte=0xa7;
        break;
      case DE:
        Byte=0xdc;
        break;
      case DKI: case SW: case NW: case DKII:
        Byte=0xc5;
        break;
      case IT:
        Byte=0xe9;
        break;
      case SPI: case SPII: case LAT:
        Byte=0xbf;
        break;
      case LEG:
        Byte=0x22;
        break;
      }
      break;
    case 0x5e:
      switch(International) {
      case SW: case NW: case DKII:
        Byte=0xdc;
        break;
      case SPII: case LAT:
        Byte=0xe9;
        break;
      case LEG:
        Byte=0xb6;
        break;
      }
      break;
    case 0x60:
      switch(International) {
      case SW: case NW: case DKII:
        Byte=0xe9;
        break;
      case IT:
        Byte=0xf9;
        break;
      case LAT:
        Byte=0xfc;
        break;
      }
      break;
    case 0x7b:
      switch(International) {
      case FR:
        Byte=0xe9;
        break;
      case DE: case SW:
        Byte=0xe4;
        break;
      case DKI: case NW: case DKII:
        Byte=0xe6;
        break;
      case IT:
        Byte=0xe0;
        break;
      case SPI:
        Byte=0xa8;
        break;
      case SPII: case LAT:
        Byte=0xed;
        break;
      case LEG:
        Byte=0xa9;
        break;
      default:
        FPUTC('\\',fp_rtf);
      }
      break;
    case 0x7c:
      switch(International) {
      case FR:
        Byte=0xf9;
        break;
      case DE: case SW:
        Byte=0xf6;
        break;
      case DKI: case NW: case DKII:
        Byte=0xf8;
        break;
      case IT:
        Byte=0xf2;
        break;
      case SPI: case SPII: case LAT:
        Byte=0xf1;
        break;
      case LEG:
        Byte=0xae;
        break;
      }
      break;
    case 0x7d:
      switch(International) {
      case FR: case IT:
        Byte=0xe8;
        break;
      case DE:
        Byte=0xfc;
        break;
      case DKI: case SW: case NW: case DKII:
        Byte=0xe5;
        break;
      case SPII: case LAT:
        Byte=0xf3;
        break;
      case LEG:
        Byte=0x86;
        break;
      default:
        FPUTC('\\',fp_rtf);
      }
      break;
    case 0x7e:
      switch(International) {
      case FR:
        Byte=0xa8;
        break;
      case DE:
        Byte=0xdf;
        break;
      case SW: case NW: case DKII:
        Byte=0xfc;
        break;
      case IT:
        Byte=0xec;
        break;
      case SPII: case LAT:
        Byte=0xFA;
        break;
      case LEG:
        Byte=0x99;
        break;
      }
      break;
    // convert extended ASCII to code page 1252 when possible
    // when we found no 1252 equivalent it defaults to space
    // ST-specific characters (different from PC ASCII) are indicated
    case 128:
      Byte=0xc7;
      break;
    case 129:
      Byte=0xfc;
      break;
    case 130:
      Byte=0xe9;
      break;
    case 131:
      Byte=0xe2;
      break;
    case 132:
      Byte=0xe4;
      break;
    case 133:
      Byte=0xe0;
      break;
    case 134:
      Byte=0xe5;
      break;
    case 135:
      Byte=0xe7;
      break;
    case 136:
      Byte=0xea;
      break;
    case 137:
      Byte=0xeb;
      break;
    case 138:
      Byte=0xe8;
      break;
    case 139:
      Byte=0xef;
      break;
    case 140:
      Byte=0xee;
      break;
    case 141:
      Byte=0xec;
      break;
    case 142:
      Byte=0xc4;
      break;
    case 143:
      Byte=0xc5;
      break;
    case 144:
      Byte=0xc9;
      break;
    case 145:
      Byte=0xe6;
      break;
    case 146:
      Byte=0xc6;
      break;
    case 147:
      Byte=0xf4;
      break;
    case 148:
      Byte=0xf6;
      break;
    case 149:
      Byte=0xf2;
      break;
    case 150:
      Byte=0xfb;
      break;
    case 151:
      Byte=0xf9;
      break;
    case 152:
      Byte=0xff;
      break;
    case 153:
      Byte=0xd6;
      break;
    case 154:
      Byte=0xdc;
      break;
    case 155:
      Byte=0xa2;
      break;
    case 156:
      Byte=0xa3;
      break;
    case 157:
      Byte=0xa5;
      break;
    case 158:
      Byte=0xdf;
      break;
    case 159:
      Byte=0x83;
      break;
    case 160:
      Byte=0xe1;
      break;
    case 161:
      Byte=0xed;
      break;
    case 162:
      Byte=0xf3;
      break;
    case 163:
      Byte=0xfa;
      break;
    case 164:
      Byte=0xf1;
      break;
    case 165:
      Byte=0xd1;
      break;
    case 166:
      Byte=0xaa;
      break;
    case 167:
      Byte=0xba;
      break;
    case 168:
      Byte=0xbf;
      break;
    case 169:
      Byte=0x3f; //?
      break;
    case 170:
      Byte=0xac;
      break;
    case 171:
      Byte=0xbd;
      break;
    case 172:
      Byte=0xbc;
      break;
    case 173:
      Byte=0xa1;
      break;
    case 174:
      Byte=0xab;
      break;
    case 175:
      Byte=0xbb;
      break;
    case 176:
      Byte=0xe3;
      break;
    case 177:
      Byte=0xf5;
      break;
    case 178:
      Byte=0xd8;
      break;
    case 179:
      Byte=0xf8;
      break;
    case 180:
      Byte=0x9c;
      break;
    case 181:
      Byte=0x8c;
      break;
    case 182:
      Byte=0xc0;
      break;
    case 183:
      Byte=0xc3;
      break;
    case 184:
      Byte=0xd5;
      break;
    case 185:
      Byte=0xa8;
      break;
    case 186:
      Byte=0xb4;
      break;
    case 187:
      Byte=0x86;
      break;
    case 188:
      Byte=0xb6;
      break;
    case 189:
      Byte=0xa9;
      break;
    case 190:
      Byte=0xae;
      break;
    case 191:
      Byte=0x99;
      break;
    case 221:
      Byte=0xa7;
      break;
    case 230:
      Byte=0xb5;
      break;
    case 241:
      Byte=0xb1;
      break;
    case 246:
      Byte=0xf7;
      break;
    case 248:
      Byte=0xb0;
      break;
    case 250:
      Byte=0xb7; //?
      break;
    case 253:
      Byte=0xb2;
      break;
    case 254:
      Byte=0xb3;
      break;
    case 255:
      Byte=0xaf; //?
      break;
    default:
      if(Byte>127)
        Byte=' '; // space not garbage on unknown extended character
    }//sw
#if defined(SSE_ENABLE_TRACE_LOG)
    if(Byte!=oldByte)
      TRACEP("[%d->%d]",oldByte,Byte);
    if(GraphicColumns<=0)
      TRACEP((Byte<' '||Ignore) ? (char*)"(%d) " : (char*)"%c",Byte);
#endif
    //TRACEP("%c(%d) ",Byte,Byte);
    if(Ignore)
    {
    }
    else if(!Colour)
      FPUTC(Byte,fp_rtf);
    else // 'Light'
      fprintf(fp_rtf,"\\cf16 %c\\cf0 ",Byte); // grey for all
  }
  LastChar=Byte;
}


void TSMM804::Reset() {
  Close();
  ZeroMemory(this,sizeof(TSMM804));
  pbm_descr.width=1280;
  pbm_descr.height=1696;
  pbm_descr.size=(pbm_descr.width*pbm_descr.height)/8;
}


#undef TRACEP

#endif//#if defined(SSE_PRINTER)


#if defined(SSE_ACSI_LASER)

TSLM804Param defaultParam;
#if 0 // BCC can't do that now that DU16 has constructors
TSLM804Param defaultParam={
  sizeof(TSLM804Param)-1,//23,//BYTE nbytes; 
  3180,//DU16 pageh;
  2400,//DU16 pagew;
  60,//DU16 marginh;
  72,//DU16 marginw;
  0,//BYTE feed;
  300,//DU16 resh;
  300,//DU16 resw;
  50,//?? BYTE timeout;
  50,//?? DU16 scantime;
  0,//DU16 pagecount;
  0,//DU16 inputcap;
  0,//DU16 outputcap;
  0//BYTE duplex;
};
#endif

TSLM804::TSLM804() {
  Reset();
}


void TSLM804::Reset() {
  ZeroMemory(this,sizeof(TSLM804));
#if 1 // for BCC
//  TSLM804Param defaultParam;
  ZeroMemory(&defaultParam,sizeof(TSLM804Param));
  defaultParam.nbytes=sizeof(TSLM804Param)-1,//23;
  defaultParam.pageh.d16=3180;
  defaultParam.pagew.d16=2400;
  defaultParam.marginh.d16=60;
  defaultParam.marginw.d16=72;
  defaultParam.resh.d16=defaultParam.resw.d16=300;
  defaultParam.timeout=50;
  defaultParam.scantime.d16=50;
#endif
  TSLM804Param::copy_config_to_buffer(&defaultParam,controller_buffer);
  TSLM804Param::copy_buffer_to_config(&Param,controller_buffer);
}


int TSLM804::bytes_per_page() {
  int b=(Param.pagew.d16*Param.pageh.d16)/8;
  return b;
}


// The SLM804 has a 2byte FIFO for DMA data, this must be emulated

void TSLM804::fill_fifo() {
  for(int i=0;i<2;i++)
  {
    Dma.Drq(MNGR_ACSI);
    fifo[i]=AcsiHdc[ACSI_ID_LASER].DR;
  }
}


BYTE TSLM804::get_fifo_byte() {
  BYTE x;
  x=fifo[0];
  fifo[0]=fifo[1];
#if defined(SSE_ENABLE_TRACE_LOG)
  if(!Dma.Counter) no_dma++;
#endif
  Dma.Drq(MNGR_ACSI);
  fifo[1]=AcsiHdc[ACSI_ID_LASER].DR;
  page_bytes++;
  return x;
}


void  TSLM804Param::copy_config_to_buffer(TSLM804Param* const config,BYTE* const buffer) {
  buffer[1]=config->pageh.d8[HI];
  buffer[2]=config->pageh.d8[LO];
  buffer[3]=config->pagew.d8[HI];
  buffer[4]=config->pagew.d8[LO];
  buffer[5]=config->marginh.d8[HI];
  buffer[6]=config->marginh.d8[LO];
  buffer[7]=config->marginw.d8[HI];
  buffer[8]=config->marginw.d8[LO];
  buffer[9]=config->feed;
  buffer[10]=config->resh.d8[HI];
  buffer[11]=config->resh.d8[LO];
  buffer[12]=config->resw.d8[HI];
  buffer[13]=config->resw.d8[LO];
  buffer[14]=config->timeout;
  buffer[15]=config->scantime.d8[HI];
  buffer[16]=config->scantime.d8[LO];
  buffer[17]=config->pagecount.d8[HI];
  buffer[18]=config->pagecount.d8[LO];
  buffer[19]=config->inputcap.d8[HI];
  buffer[20]=config->inputcap.d8[LO];
  buffer[21]=config->outputcap.d8[HI];
  buffer[22]=config->outputcap.d8[LO];
  buffer[23]=config->duplex;
  for(int i=24;i<=NLASER_PARAMETERS;buffer[i++]=0);
}


void  TSLM804Param::copy_buffer_to_config(TSLM804Param *config,BYTE *buffer) {
  config->pageh.d8[HI]=buffer[1];
  config->pageh.d8[LO]=buffer[2];
  config->pagew.d8[HI]=buffer[3];
  config->pagew.d8[LO]=buffer[4];
  config->marginh.d8[HI]=buffer[5];
  config->marginh.d8[LO]=buffer[6];
  config->marginw.d8[HI]=buffer[7];
  config->marginw.d8[LO]=buffer[8];
  config->feed=buffer[9];
  config->resh.d8[HI]=buffer[10];
  config->resh.d8[LO]=buffer[11];
  config->resw.d8[HI]=buffer[12];
  config->resw.d8[LO]=buffer[13];
  config->timeout=buffer[14];
  config->scantime.d8[HI]=buffer[15];
  config->scantime.d8[LO]=buffer[16];
  config->pagecount.d8[HI]=buffer[17];
  config->pagecount.d8[LO]=buffer[18];
  config->inputcap.d8[HI]=buffer[19];
  config->inputcap.d8[LO]=buffer[20];
  config->outputcap.d8[HI]=buffer[21];
  config->outputcap.d8[LO]=buffer[22];
  config->duplex=buffer[23];
}

#endif//#if defined(SSE_ACSI_LASER)

#undef LOGSECTION
#undef TRACEP