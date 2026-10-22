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

DOMAIN: Emu
FILE: dma.cpp
DESCRIPTION: The DMA (Direct Memory Access) chip was developed by Atari.
It uses two 16byte FIFO buffers to handle byte transfers between the 
drive  controller (floppy or hard disk) and memory so that the CPU is
freed from the task. Memory access is done by the MMU upon DMA request, 
as arbitrated by the GLUE. The DMA can't address the bus despite its name.
The CPU must free the bus for each 16-byte transfer. That's a lot of
transfers when you're using a hard drive, and it slows down the ST's CPU.
We don't very precisely emulate bus arbitration, unlike with the blitter 
emulation, because it's not as important. Disk has the highest bus priority
so that no byte is missed, in Steem there's no failure either.
We count DMA cycles only if the option is checked. In some previous versions
of Steem, nothing was counted and it broke nothing. In fact, counting
cycles at imprecise timings like we do is likelier to break programs!
Precision of transfer timing depends on the image type.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <diskman.h>
#include <iolist.h>


bool bReportCrc=false;


#define LOGSECTION LOGSECTION_DMA


void TDma::AddToFifo(BYTE const manager,BYTE const data) {
  // DISK -> FIFO -> RAM = 'read disk' = 'write dma'
  Fifo[BufferInUse][Fifo_idx[BufferInUse]++]=data;
  if(Fifo_idx[BufferInUse]==DMA_BUFFER_LEN)
    RequestTransfer(manager,BufferInUse);
}


void TDma::Drq(BYTE const manager) {
  sr|=SR_DRQ;
  if(mcr&CR_WRITE) // RAM -> disk/printer
  {
    /*_DRQ is driven by the peripheral with an
    open collector output. Only the peripherals that have
    been commanded by the host peripheral may drive
    _IRQ, _DRQ, or the data bus. A peripheral may not
    spontaneously gain the host's attention. */
#if defined(SSE_ACSI)
    if(manager==MNGR_ACSI)
      AcsiHdc[acsi_dev].DR=GetFifoByte(manager);
    else
#endif
      Fdc.dr=GetFifoByte(manager);
  }
  else // disk->RAM
  {
#if defined(SSE_ACSI)
    if(manager==MNGR_ACSI)
      AddToFifo(manager,AcsiHdc[acsi_dev].DR);
    else
#endif
      AddToFifo(manager,Fdc.dr);
  }
  sr&=~SR_DRQ;
}


BYTE TDma::GetFifoByte(BYTE const manager) {
  // RAM -> FIFO -> DISK = 'write disk' = 'read dma'
  // RAM -> FIFO -> LASER PRINTER
  BYTE data=0;
  if(!Fifo_idx[BufferInUse]) // no guarantee it's prefilled
    RequestTransfer(manager,BufferInUse);
  data=Fifo[BufferInUse][--Fifo_idx[BufferInUse]];
/*  Refill FIFO buffer as soon as it's empty.
    Laser printing: when a line is finished, this fetching moves the DMA pointer
    and the program sees it while the printer is in the margin, the program
    resets DMA (flushes FIFO) and changes the address before printing resumes.  */
  if(!Fifo_idx[BufferInUse] && Counter)
  {
    BufferInUse=!BufferInUse;
    RequestTransfer(manager,BufferInUse);
    Fifo_idx[BufferInUse]=DMA_BUFFER_LEN;
    BufferInUse=!BufferInUse;
  }
  return data;  
}


/*  "The count should indicate the number of 512 bytes chunks that 
    the DMA will have to transfer
    "This register is decrement by one each time 512 bytes has been
    transferred. When the sector count register reaches zero the DMA
    will stop to transfer data. Only the lower 8 bits are used."
    -> it's always 512 bytes, regardless of sector size
*/

void TDma::IncAddress() {
  bReportCrc=false;
  if(Counter&0xFF) 
  {
    dma_address+=2;
    ByteCount+=2;
    if(ByteCount>=SECTOR_SIZE)
    {
#if defined(SSE_STATS)
      if(DiskEmu.LastManager!=MNGR_ACSI)
      {
        if((mcr&CR_WRITE))
          Stats.nSectorW[DRIVE]++;
        else
          Stats.nSectorR[DRIVE]++;
        if(CURRENT_TRACK>StatsStatic.maxTrack[DRIVE])
          StatsStatic.maxTrack[DRIVE]=CURRENT_TRACK;
        if(CURRENT_SIDE>StatsStatic.maxSide[DRIVE])
          StatsStatic.maxSide[DRIVE]=CURRENT_SIDE;
        if(Fdc.sr>StatsStatic.maxSector[DRIVE])
          StatsStatic.maxSector[DRIVE]=Fdc.sr;
        bReportCrc=true;
      }
#endif
      ByteCount=0;
      Counter--;
      if(Counter&0xFF)
        sr|=SR_COUNT;  // DMA sector count not 0
      else
        sr&=~SR_COUNT;
    }
  }
}


void TDma::RequestTransfer(BYTE const manager,BYTE const bufnum) {
  Request=true; 
  if(bufnum!=BufferInUse)
  {
    //ASSERT(mcr&CR_WRITE);
    if(SHOULD_DUMP_BYTES)
      TRACE_LOG("(%d) ",!BufferInUse);
    Fifo_idx[!BufferInUse]=DMA_BUFFER_LEN;
    BufferInUse=!BufferInUse;
    TransferBytes(manager);
    BufferInUse=!BufferInUse;
  }
  else
  {
    BufferInUse=!BufferInUse; // toggle 16byte buffer
    if((mcr&CR_WRITE)&&Fifo_idx[BufferInUse]==DMA_BUFFER_LEN)
    {
      // already done
    }
    else
    {
      Fifo_idx[BufferInUse]= (mcr&CR_WRITE) ? DMA_BUFFER_LEN : 0;
      TransferBytes(manager);
    }
  }
}


void TDma::TransferBytes(BYTE const manager) { //Execute the DMA transfer. Count cycles if needed.
  //if(IMAGE_SCP) TRACE_LOG("pos %d\n",ImageSCP[DRIVE].Position);
  //ASSERT( Request );
  //ASSERT(!(mcr&CR_RESERVED)); // reserved bits
  BUS_SAVE; // like for the blitter, we must save cpu's internals (TODO)
#if defined(SSE_STATS) || defined(SSE_DEBUGGER)
/*  Computing the checksum if it's bootsector.
    We do it here in DMA because it should work with native, Pasti, CAPS,
    MFM. Both sides for Freeboot.
*/
  if(Fdc.cr==0x80 && !Fdc.tr && Fdc.sr==1 && !(mcr&CR_WRITE))
  {
    for(int i=0;i<DMA_BUFFER_LEN;i+=2)
    {
      FloppyDrive[DRIVE].SectorChecksum+=Fifo[!BufferInUse][i]<<8; 
      FloppyDrive[DRIVE].SectorChecksum+=Fifo[!BufferInUse][i+1]; 
    }
  }
#endif

  if(SHOULD_DUMP_BYTES)
  {
    //TRACE_LOG("(%d) ",BufferInUse);
#ifdef SSE_ACSI
    if(manager==MNGR_ACSI)
      TRACE_LOG("%s %06X: ",(mcr&CR_WRITE)?"from":"to",dma_address);
    else
#endif
    {
      TRACE_LOG3(PRICV " ",ABSOLUTE_SYS_TIME-Dma.last_act);
      TRACE_LOG("(%d-%02d-%02d) %s %06X: ",CURRENT_SIDE,CURRENT_TRACK/*DiskEmu.track*/,
                Fdc.CommandType()==2 ? Fdc.sr : 0,(mcr&CR_WRITE) ? "from" : "to",dma_address);
    }
  }
  Dma.last_act=A_S_T; // for timing above // TODO remove?

#ifdef SSE_421R0 // clarity
  bool count_cycles=(OPTION_COUNT_DMA_CYCLES!=0);
#else
  bool count_cycles=(OPTION_COUNT_DMA_CYCLES!=0)
    && ((!(manager==MNGR_ACSI) && ADAT) // floppy select
#if defined(SSE_ACSI)
    || (ACSI_EMU_ON && (manager==MNGR_ACSI) && !DiskMan.bTurboDrive) // HD select
#endif
#if defined(SSE_ACSI_LASER)
     || acsi_dev==ACSI_ID_LASER&&OPTION_LASER&&(manager==MNGR_ACSI)
#endif
    );
#endif

  BYTE save_blitter_request=Blitter.Request;
  Blitter.Request=false; // disk has higher priority (Antiques MFM)
  
  if(count_cycles)
    CPU_BUS_IDLE(4); // is there arbitration delay?

  bool bWriteRAM=!(mcr&CR_WRITE);
  for(int i=0;i<8;i++) // burst, 8 word packets
  {
    if(bWriteRAM && DMA_ADDRESS_IS_VALID_W) // disk -> RAM
    {
      dbush=Fifo[!BufferInUse][i*2];
      dbusl=Fifo[!BufferInUse][i*2+1];
#if defined(SSE_DEBUGGER_FAKE_IO)
      DiskEmu.crc16.Add(dbush);
      DiskEmu.crc16.Add(dbusl);
#endif
      if(count_cycles)
      {
        iabus=dma_address; //temp
//        ASSERT(!Stvl.busy); // with CAPS disks
        DMA_BUS_ACCESS_WRITE;
      }
      else
        SafeDPoke(dma_address,dbus);
    }
    else if(!bWriteRAM && DMA_ADDRESS_IS_VALID_R) // RAM -> disk
    {
      if(count_cycles)
      {
        iabus=dma_address; //temp
        DMA_BUS_ACCESS_READ;
      }
      else
      {
        dbush=SafePeek(dma_address);
        dbusl=SafePeek(dma_address+1);
      }
      Fifo[BufferInUse][DMA_BUFFER_LEN-2-i*2]=dbusl; // reverse order!
      Fifo[BufferInUse][DMA_BUFFER_LEN-2-i*2+1]=dbush;
    }
    IncAddress();
  }//nxt

#if defined(SSE_ENABLE_TRACE_LOG)
  // TODO for better performance, we could prepare TRACE lines to limit calls to TRACE_LOG
  if(SHOULD_DUMP_BYTES)
  {
    if(!((bWriteRAM&&DMA_ADDRESS_IS_VALID_W)||(!bWriteRAM&&DMA_ADDRESS_IS_VALID_R)))
    {
      TRACE_LOG("DMA address not valid\n");
    }
    else
    {
      BYTE BufferToTrace=(bWriteRAM)?(!BufferInUse):(BufferInUse);
      for(int i=0;i<16;i++) // hex
      {
        TRACE_LOG("%02X ",Fifo[BufferToTrace][(bWriteRAM)?i:15-i]);
      }
      for(int i=0;i<16;i++) // text
      {
        BYTE b=Fifo[BufferToTrace][(bWriteRAM)?i:15-i];
        TRACE_LOG("%c",(b>=0x30&&b<='z')?b:'.');
      }
    }
    TRACE_LOG("(%d)\n",ByteCount);
  }
  if(bReportCrc)
  {
    TRACE_LOG("%s %04X\n","CRC",DiskEmu.crc16.crc);
    DiskEmu.crc16.Reset();
    DiskEmu.crc16.Add(0xFB);
  }
#endif

  Blitter.Request=save_blitter_request;
  if(count_cycles)
    CPU_BUS_IDLE(4); // is there arbitration delay?
#if defined(SSE_DEBUGGER)
  // write the DMA transfer in history
  pc_history_y[pc_history_idx]=scan_y;
  pc_history_c[pc_history_idx]=LINECYCLES;
  pc_history[pc_history_idx++]=MAGIC_HIST_DMA;
  if(pc_history_idx>=HISTORY_SIZE)
    pc_history_idx=0;
  if(!count_cycles) for(int i=0;i<8;i++) // for Debugger monitor, intercept DMA traffic
  {
    if(!(mcr&0x100)&& DMA_ADDRESS_IS_VALID_W) // disk -> RAM
    {DEBUG_CHECK_WRITE_W(dma_address-DMA_BUFFER_LEN+i*2)}
    else if((mcr&0x100) && DMA_ADDRESS_IS_VALID_R) // RAM -> disk
    {DEBUG_CHECK_READ_W(dma_address-DMA_BUFFER_LEN+i*2)}
  }
#endif
  BUS_RESTORE;
  Request=FALSE;
}

#undef LOGSECTION
