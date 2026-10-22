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
FILE: device_map.h
DESCRIPTION: Declarations for device read/write through the bus.
---------------------------------------------------------------------------*/

#pragma once
#ifndef IORW_DECLA_H
#define IORW_DECLA_H

#include <conditions.h>
#include <data_union.h>

/*
               The ST I/O space ranges from ff 0000 to ff  ffff,  with
          MC68000 and MC6800 peripheral internal registers starting at
          ff fa00 and ff fc00 respectively.

          
          An MC68000 peripheral is the MFP 68901
          Another is the Floating Point Coprocessor 68881 (not emulated)
          MC6800 peripherals are MC6850 ACIA chips and the Ricoh Real Time Clock
          Lower addresses are mapped to the other chips (MMU, GLUE, DMA, PSG...),
          essentially by the GLUE and the MMU.
          Exception: the BLiTTER does its own decoding.

          Here is an edited table with the STF/STE mapping

Address Description                                                      Space
-------+-----+-----------------------------------------------------+----------
##############ST MMU Controller                                    ###########
-------+-----+-----------------------------------------------------+----------
$FF8001|byte |MMU memory configuration                  BIT 3 2 1 0|R/W
       |     |Bank 0                                        | | | ||
       |     |00 - 128k ------------------------------------+-+ | ||
       |     |01 - 512k ------------------------------------+-+ | ||
       |     |10 - 2m --------------------------------------+-+ | ||
       |     |11 - reserved --------------------------------+-' | ||
       |     |Bank 1                                            | ||
       |     |00 - 128k ----------------------------------------+-+|
       |     |01 - 512k ----------------------------------------+-+|
       |     |10 - 2m ------------------------------------------+-+|
       |     |11 - reserved ------------------------------------+-'|
-------+-----+-----------------------------------------------------+----------
##############Video Chips                                          ###########
-------+-----+-----------------------------------------------------+----------
$FF8201|byte |Video screen memory position (High byte)             |R/W
$FF8203|byte |Video screen memory position (Mid byte)              |R/W
$FF820D|byte |Video screen memory position (Low byte)              |R/W  (STe)
$FF8205|byte |Video address pointer (High byte)                    |R STE: R/W
$FF8207|byte |Video address pointer (Mid byte)                     |R STE: R/W
$FF8209|byte |Video address pointer (Low byte)                     |R STE: R/W
$FF820F|byte |Width of a scanline (width in words-1)               |R/W  (STe)
$FF8265|byte |Horizontal scroll register (0-15)                    |R/W  (STe)
-------+-----+-----------------------------------------------------+----------
$FF820A|byte |Video synchronization mode                    BIT 1 0|R/W
       |     |0 - 60hz, 1 - 50hz -------------------------------+ ||
       |     |0 - internal, 1 - external sync --------------------'|
-------+-----+-----------------------------------------------------+----------
       |     |                                BIT 11111198 76543210|
       |     |                                    543210           |
       |     |                     ST color value .....RRr .GGr.BBb|
       |     |                    STe color value ....rRRR gGGGbBBB|
$FF8240|word |Video palette register 0              Lowercase = LSB|R/W
    :  |  :  |  :      :       :     :                             | :
$FF825E|word |Video palette register 15                            |R/W
-------+-----+-----------------------------------------------------+----------
$FF8260|byte |Shifter resolution                            BIT 1 0|R/W
       |     |00 320x200x4 bitplanes (16 colors) ---------------+-+|
       |     |01 640x200x2 bitplanes (4 colors) ----------------+-+|
       |     |10 640x400x1 bitplane  (1 colors) ----------------+-'|
-------+-----+-----------------------------------------------------+----------
$FF827E|???? |STACY Display Driver                                 |???(STACY)
-------+-----+-----------------------------------------------------+----------
##############DMA/WD1772 Disk controller                           ###########
-------+-----+-----------------------------------------------------+----------
$FF8600|     |Reserved                                             |
$FF8602|     |Reserved                                             |
$FF8604|word |FDC access/sector count                              |R/W
$FF8606|word |DMA mode/status                             BIT 2 1 0|R
       |     |Condition of FDC DATA REQUEST signal -----------' | ||
       |     |0 - sector count null,1 - not null ---------------' ||
       |     |0 - no error, 1 - DMA error ------------------------'|
$FF8606|word |DMA mode/status                 BIT 8 7 6 . 4 3 2 1 .|W
       |     |0 - read FDC/HDC,1 - write ---------' | | | | | | |  |
       |     |0 - HDC access,1 - FDC access --------' | | | | | |  |
       |     |0 - DMA on,1 - no DMA ------------------' | | | | |  |
       |     |Reserved ---------------------------------' | | | |  |
       |     |0 - FDC reg,1 - sector count reg -----------' | | |  |
       |     |0 - FDC access,1 - HDC access ----------------' | |  |
       |     |0 - pin A1 low, 1 - pin A1 high ----------------' |  |
       |     |0 - pin A0 low, 1 - pin A0 high ------------------'  |
$FF8609|byte |DMA base and counter (High byte)                     |R/W
$FF860B|byte |DMA base and counter (Mid byte)                      |R/W
$FF860D|byte |DMA base and counter (Low byte)                      |R/W
-------+-----+-----------------------------------------------------+----------
##############YM2149 Sound Chip                                    ###########
-------+-----+-----------------------------------------------------+----------
$FF8800|byte |Read data/Register select                            |R/W
       |     |0 Channel A Freq Low              BIT 7 6 5 4 3 2 1 0|
       |     |1 Channel A Freq High                     BIT 3 2 1 0|
       |     |2 Channel B Freq Low              BIT 7 6 5 4 3 2 1 0|
       |     |3 Channel B Freq High                     BIT 3 2 1 0|
       |     |4 Channel C Freq Low              BIT 7 6 5 4 3 2 1 0|
       |     |5 Channel C Freq High                     BIT 3 2 1 0|
       |     |6 Noise Freq                          BIT 5 4 3 2 1 0|
       |     |7 Mixer Control                   BIT 7 6 5 4 3 2 1 0|
       |     |  Port B IN/OUT (1=Output) -----------' | | | | | | ||
       |     |  Port A IN/OUT ------------------------' | | | | | ||
       |     |  Channel C Noise (1=Off) ----------------' | | | | ||
       |     |  Channel B Noise --------------------------' | | | ||
       |     |  Channel A Noise ----------------------------' | | ||
       |     |  Channel C Tone (0=On) ------------------------' | ||
       |     |  Channel B Tone ---------------------------------' ||
       |     |  Channel A Tone -----------------------------------'|
       |     |8 Channel A Amplitude Control           BIT 4 3 2 1 0|
       |     |  Fixed/Variable Level (0=Fixed) -----------' | | | ||
       |     |  Amplitude level control --------------------+-+-+-'|
       |     |9 Channel B Amplitude Control           BIT 4 3 2 1 0|
       |     |  Fixed/Variable Level ---------------------' | | | ||
       |     |  Amplitude level control --------------------+-+-+-'|
       |     |10 Channel C Amplitude Control          BIT 4 3 2 1 0|
       |     |  Fixed/Variable Level ---------------------' | | | ||
       |     |  Amplitude level control --------------------+-+-+-'|
       |     |11 Envelope Period High           BIT 7 6 5 4 3 2 1 0|
       |     |12 Envelope Period Low            BIT 7 6 5 4 3 2 1 0|
       |     |13 Envelope Shape                         BIT 3 2 1 0|
       |     |  Continue -----------------------------------' | | ||
       |     |  Attack ---------------------------------------' | ||
       |     |  Alternate --------------------------------------' ||
       |     |  Hold ---------------------------------------------'|
       |     |   00xx - \____________________________________      |
       |     |   01xx - /|___________________________________      |
       |     |   1000 - \|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\      |
       |     |   1001 - \____________________________________      |
       |     |   1010 - \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\      |
       |     |   1011 - \|-----------------------------------      |
       |     |   1100 - /|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/      |
       |     |   1101 - /------------------------------------      |
       |     |   1110 - /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/      |
       |     |   1111 - /|___________________________________      |
       |     |14 Port A                         BIT 7 6 5 4 3 2 1 0|
       |     |  Centronics strobe ----------------------' | | | | ||
       |     |  RS-232 DTR output ------------------------' | | | ||
       |     |  RS-232 RTS output --------------------------' | | ||
       |     |  Drive select 1 -------------------------------' | ||
       |     |  Drive select 0 ---------------------------------' ||
       |     |  Drive side select --------------------------------'|
       |     |15 Port B (Parallel port)                            |
$FF8802|byte |Write data                                           |W
       |     +-----------------------------------------------------+
-------+-----+-----------------------------------------------------+----------
##############DMA Sound System                                     ###########
-------+-----+-----------------------------------------------------+----------
$FF8901|byte |DMA Control Register              BIT 7 . 5 4 . . 1 0|R/W
       |     |Loop replay buffer -------------------------------' ||     (STe)
       |     |DMA Replay on --------------------------------------'|     (STe)
-------+-----+-----------------------------------------------------+----------
$FF8903|byte |Frame start address (high byte)                      |R/W  (STe)
$FF8905|byte |Frame start address (mid byte)                       |R/W  (STe)
$FF8907|byte |Frame start address (low byte)                       |R/W  (STe)
$FF8909|byte |Frame address counter (high byte)                    |R    (STe)
$FF890B|byte |Frame address counter (mid byte)                     |R    (STe)
$FF890D|byte |Frame address counter (low byte)                     |R    (STe)
$FF890F|byte |Frame end address (high byte)                        |R/W  (STe)
$FF8911|byte |Frame end address (mid byte)                         |R/W  (STe)
$FF8913|byte |Frame end address (low byte)                         |R/W  (STe)
-------+-----+-----------------------------------------------------+----------
$FF8921|byte |Sound mode control                BIT 7 6 . . . . 1 0|R/W  (STe)
       |     |0 - Stereo, 1 - Mono -----------------'           | ||
       |     |Frequency control bits                            | ||
       |     |00 - 6258hz frequency (STe only) -----------------+-+|
       |     |01 - 12517hz frequency ---------------------------+-+|
       |     |10 - 25033hz frequency ---------------------------+-+|
       |     |11 - 50066hz frequency ---------------------------+-'|
       |     |Samples are always signed. In stereo mode, data is   |
       |     |arranged in pairs with high pair the left channel,low|
       |     |pair right channel. Sample length MUST be even in    |
       |     |either mono or stereo mode.                          |
       |     |Example: 8 bit Stereo : LRLRLRLRLRLRLRLR             |
-------+-----+-----------------------------------------------------+----------
##############STe Microwire Controller (STe/TT only!)              ###########
-------+-----+-----------------------------------------------------+----------
$FF8922|byte |Microwire data register                              |R/W  (Mwr)
$FF8924|byte |Microwire mask register                              |R/W  (Mwr)
       |     +-----------------------------------------------------+
       |     |Volume/tone controller commands         (Address %10)|
       |     |Master Volume                           10 011 DDDDDD|
       |     |Left Volume                             10 101 .DDDDD|
       |     |Right Volume                            10 100 .DDDDD|
       |     |Treble                                  10 010 ..DDDD|
       |     |Bass                                    10 001 ..DDDD|
       |     |Mixer                                   10 000 ....DD|
       |     +-----------------------------------------------------+
       |     |Volume/tone controller values                        |
       |     |Master Volume     : 0-40   (0 -80dB, 40=0dB)         |
       |     |Left/Right Volume : 0-20    (0 80dB, 20=0dB)         |
       |     |Treble/bass       : 0-12 (0 -12dB, 12 +12dB)         |
       |     |Mixer             : 0-3 (0 -12dB, 1 mix PSG)         |
       |     |                    (2 don't mix,3 reserved)         |
       |     +-----------------------------------------------------+
       |     |Procedure: Set mask register to $7ff. Read data      |
       |     |register and save original value.Write data register.|
       |     |Compare data register with original value, repeat    |
       |     |until data register returns to original value to     |
       |     |ensure data has been sent over the interface.        |
       |     +-----------------------------------------------------+
       |     |Interrupts: Timer A can be set to interrupt at the   |
       |     |end of a frame. Alternatively, the GPI7 (MFP mono    |
       |     |detect) can be used to generate interrupts thereby   |
       |     |freeing up Timer A. In this case, the active edge    |
       |     |$FFFA03 must be set by or-ing the active edge of     |
       |     |$FFFA03 with the contents of $FF8260:                |
       |     |$FF8260 - 2 (mono)     or.b  #$80 with edge          |
       |     |$FF8260 - 0,1 (colour) and.b #$7F with edge          |
       |     |This will generate an interrupt at the START of a    |
       |     |frame, instead of at the end as with Timer A. To     |
       |     |generate an interrupt at the END of a frame, simply  |
       |     |reverse the edge values.                             |
-------+-----+-----------------------------------------------------+----------
##############Blitter (Mega ST + STE     )                         ###########
-------+-----+-----------------------------------------------------+----------
$FF8A00|word |Halftone-RAM, Word 0                                 |R/W (Blit)
    :  |  :  |    :     :     :  :                                 | :     :
$FF8A1E|word |Halftone-RAM, Word 15                                |R/W (Blit)
$FF8A20|word |Source X Increment                      (signed,even)|R/W (Blit)
$FF8A22|word |Source Y Increment                      (signed,even)|R/W (Blit)
$FF8A24|long |Source Address Register                 (24 bit,even)|R/W (Blit)
$FF8A28|word |Endmask 1                     (First write of a line)|R/W (Blit)
$FF8A2A|word |Endmask 2                     (All other line writes)|R/W (Blit)
$FF8A2C|word |Endmask 3                      (Last write of a line)|R/W (Blit)
$FF8A2E|word |Destination X Increment                 (signed,even)|R/W (Blit)
$FF8A30|word |Destination Y Increment                 (signed,even)|R/W (Blit)
$FF8A32|long |Destination Address Register            (24 bit,even)|R/W (Blit)
$FF8A36|word |Words per Line in Bit-Block                 (0=65536)|R/W (Blit)
$FF8A38|word |Lines per Bit-Block                         (0=65536)|R/W (Blit)
$FF8A3A|byte |Halftone Operation Register                   BIT 1 0|R/W (Blit)
       |     |00 - All ones ------------------------------------+-+|
       |     |01 - Halftone ------------------------------------+-+|
       |     |10 - Source --------------------------------------+-+|
       |     |11 - Source AND Halftone -------------------------+-'|
$FF8A3B|byte |Logical Operation Register                BIT 3 2 1 0|R/W (Blit)
       |     |0000 All zeros -------------------------------+-+-+-+|
       |     |0001 Source AND destination ------------------+-+-+-+|
       |     |0010 Source AND NOT destination --------------+-+-+-+|
       |     |0011 Source ----------------------------------+-+-+-+|
       |     |0100 NOT source AND destination --------------+-+-+-+|
       |     |0101 Destination -----------------------------+-+-+-+|
       |     |0110 Source XOR destination ------------------+-+-+-+|
       |     |0111 Source OR destination -------------------+-+-+-+|
       |     |1000 NOT source AND NOT destination ----------+-+-+-+|
       |     |1001 NOT source XOR destination --------------+-+-+-+|
       |     |1010 NOT destination -------------------------+-+-+-+|
       |     |1011 Source OR NOT destination ---------------+-+-+-+|
       |     |1100 NOT source ------------------------------+-+-+-+|
       |     |1101 NOT source OR destination ---------------+-+-+-+|
       |     |1110 NOT source OR NOT destination -----------+-+-+-+|
       |     |1111 All ones --------------------------------+-+-+-'|
$FF8A3C|byte |Line Number Register              BIT 7 6 5 . 3 2 1 0|R/W (Blit)
       |     |BUSY ---------------------------------' | |   | | | ||
       |     |0 - Share bus, 1 - Hog bus -------------' |   | | | ||
       |     |SMUDGE mode ------------------------------'   | | | ||
       |     |Halftone line number -------------------------+-+-+-'|
$FF8A3D|byte |SKEW Register                     BIT 7 6 . . 3 2 1 0|R/W (Blit)
       |     |Force eXtra Source Read --------------' |     | | | ||
       |     |No Final Source Read -------------------'     | | | ||
       |     |Source skew ----------------------------------+-+-+-'|
-------+-----+-----------------------------------------------------+----------
##############Zilog 8530 SCC (MSTe/TT/F030)                        ###########
-------+-----+-----------------------------------------------------+----------
$FF8C81|byte |Channel A - Control Register                         |R/W  (SCC)
$FF8C83|byte |Channel A - Data Register                            |R/W  (SCC)
$FF8C85|byte |Channel B - Control Register                         |R/W  (SCC)
$FF8C87|byte |Channel B - Data Register                            |R/W  (SCC)
-------+-----+-----------------------------------------------------+----------
##############VME Bus System Control Unit (MSTe/TT)                ###########
-------+-----+-----------------------------------------------------+----------
$FF8E01|byte |VME sys_mask                      BIT 7 6 5 4 . 2 1 .|R/W  (VME)
$FF8E03|byte |VME sys_stat                      BIT 7 6 5 4 . 2 1 .|R    (VME)
       |     |_SYSFAIL in VMEBUS -------------------' | | |   | |  |program
       |     |MFP ------------------------------------' | |   | |  |autovec
       |     |SCC --------------------------------------' |   | |  |autovec
       |     |VSYNC --------------------------------------'   | |  |program
       |     |HSYNC ------------------------------------------' |  |program
       |     |System software INT ------------------------------'  |program
       |     +-----------------------------------------------------+
       |     |Reading sys_mask resets pending int-bits in sys_stat,|
       |     |so read sys_stat first.                              |
-------+-----+-----------------------------------------------------+----------
$FF8E05|byte |VME sys_int                                     BIT 0|R/W  (VME)
       |     |Setting bit 0 to 1 forces an INT of level 1. INT must|Vector $64
       |     |be enabled in sys_mask to use it.                    |
-------+-----+-----------------------------------------------------+----------
$FF8E0D|byte |VME vme_mask                      BIT 7 6 5 4 3 2 1 .|R/W  (VME)
$FF8E0F|byte |VME vme_stat                      BIT 7 6 5 4 3 2 1 .|R    (VME)
       |     |_IRQ7 from VMEBUS --------------------' | | | | | |  |program
       |     |_IRQ6 from VMEBUS/MFP ------------------' | | | | |  |program
       |     |_IRQ5 from VMEBUS/SCC --------------------' | | | |  |program
       |     |_IRQ4 from VMEBUS --------------------------' | | |  |program
       |     |_IRQ3 from VMEBUS/soft -----------------------' | |  |prog/autov
       |     |_IRQ2 from VMEBUS ------------------------------' |  |program
       |     |_IRQ1 from VMEBUS --------------------------------'  |program
       |     +-----------------------------------------------------+
       |     |MFP-int and SCC-int are hardwired to the VME-BUS-ints|
       |     |(or'ed). Reading vme_mask resets pending int-bits in |
       |     |vme_stat, so read vme_stat first.                    |
-------+-----+-----------------------------------------------------+----------
##############Mega STe Cache/Processor Control                     ###########
-------+-----+-----------------------------------------------------+----------
$FF8E21|byte |Mega STe Cache/Processor Control           BIT 15-1 0|R/W (MSTe)
       |     |Cache enable lines (set all to 1 to enable) -----'  ||
       |     |CPU Speed (0 - 8mhz, 1 - 16mhz) --------------------'|
-------+-----+-----------------------------------------------------+----------
##############STe/F030 Extended Joystick/Lightpen Ports            ###########
-------+-----+-----------------------------------------------------+----------
$FF9200|word |Fire buttons 1-4                          Bit 3 2 1 0|R    (Ext)
       |     |Pause/F0 -------------------------------------' | | ||
       |     |F1 ---------------------------------------------' | ||
       |     |F2 -----------------------------------------------' ||
       |     |Option/F3 ------------------------------------------'|
$FF9202|word |Read Mask (0 - pin read)                             |W    (Ext)
$FF9202|word |Joystick Inputs                   BIT 7 6 5 4 3 2 1 0|R    (Ext)
       |     |Controller 1 pin 4 -------------------' | | | | | | ||
       |     |Controller 1 pin 3 ---------------------' | | | | | ||
       |     |Controller 1 pin 2 -----------------------' | | | | ||
       |     |Controller 1 pin 1 -------------------------' | | | ||
       |     |Controller 0 pin 4 ---------------------------' | | ||
       |     |Controller 0 pin 3/Paddle 1 Trigger ------------' | ||
       |     |Controller 0 pin 2/Paddle 0 Trigger --------------' ||
       |     |Controller 0 pin 1 ---------------------------------'|
       |     |                            BIT 15 14 13 12 11 10 9 8|
       |     |Controller 1 pin 14 ------------'   |  |  |  |  | | ||
       |     |Controller 1 pin 13 ----------------'  |  |  |  | | ||
       |     |Controller 1 pin 12 -------------------'  |  |  | | ||
       |     |Controller 1 pin 11 ----------------------'  |  | | ||
       |     |Controller 0 pin 14 -------------------------'  | | ||
       |     |Controller 0 pin 13 ----------------------------' | ||
       |     |Controller 0 pin 12 ------------------------------' ||
       |     |Controller 0 pin 11 --------------------------------'|
$FF9210|word |X Paddle 0 Position               BIT 7 6 5 4 3 2 1 0|R    (Ext)
$FF9212|word |Y Paddle 0 Position               BIT 7 6 5 4 3 2 1 0|R    (Ext)
$FF9214|word |X Paddle 1 Position               BIT 7 6 5 4 3 2 1 0|R    (Ext)
$FF9216|word |Y Paddle 1 Position               BIT 7 6 5 4 3 2 1 0|R    (Ext)
$FF9220|word |Lightpen X-Position           BIT 9 8 7 6 5 4 3 2 1 0|R    (Ext)
$FF9222|word |Lightpen Y-Position           BIT 9 8 7 6 5 4 3 2 1 0|R    (Ext)
-------+-----+-----------------------------------------------------+----------
##############MFP 68901 - Multi Function Peripheral Chip           ###########
-------+-----+-----------------------------------------------------+----------
       |     |     MFP Master Clock is 2,457,600 cycles/second     |
-------+-----+-----------------------------------------------------+----------
$FFFA01|byte |Parallel Port Data Register                          |R/W
-------+-----+-----------------------------------------------------+----------
$FFFA03|byte |Active Edge Register              BIT 7 6 5 4 . 2 1 0|R/W
       |     |Monochrome monitor detect ------------' | | | | | | ||
       |     |RS-232 Ring indicator ------------------' | | | | | ||
       |     |FDC/HDC interrupt ------------------------' | | | | ||
       |     |Keyboard/MIDI interrupt --------------------' | | | ||
       |     |Reserved -------------------------------------' | | ||
       |     |RS-232 CTS (input) -----------------------------' | ||
       |     |RS-232 DCD (input) -------------------------------' ||
       |     |Centronics busy ------------------------------------'|
       |     +-----------------------------------------------------+
       |     |       When port bits are used for input only:       |
       |     |0 - Interrupt on pin high-low conversion             |
       |     |1 - Interrupt on pin low-high conversion             |
-------+-----+-----------------------------------------------------+----------
$FFFA05|byte |Data Direction                    BIT 7 6 5 4 3 2 1 0|R/W
       |     |0 - In, 1 - Out ----------------------+-+-+-+-+-+-+-'|
-------+-----+-----------------------------------------------------+----------
$FFFA07|byte |Interrupt Enable A                BIT 7 6 5 4 3 2 1 0|R/W
$FFFA0B|byte |Interrupt Pending A               BIT 7 6 5 4 3 2 1 0|R/W
$FFFA0F|byte |Interrupt In-service A            BIT 7 6 5 4 3 2 1 0|R/W
$FFFA13|byte |Interrupt Mask A                  BIT 7 6 5 4 3 2 1 0|R/W
       |     |MFP Address                           | | | | | | | ||
       |     |$13C GPI7-Monochrome Detect ----------' | | | | | | ||
       |     |$138   RS-232 Ring Detector ------------' | | | | | ||
       |     |$134 (STe sound)    Timer A --------------' | | | | ||
       |     |$130    Receive buffer full ----------------' | | | ||
       |     |$12C          Receive error ------------------' | | ||
       |     |$128      Send buffer empty --------------------' | ||
       |     |$124             Send error ----------------------' ||
       |     |$120 (HBL)          Timer B ------------------------'|
       |     |1 - Enable Interrupt            0 - Disable Interrupt|
-------+-----+-----------------------------------------------------+----------
$FFFA09|byte |Interrupt Enable B                BIT 7 6 5 4 3 2 1 0|R/W
$FFFA0D|byte |Interrupt Pending B               BIT 7 6 5 4 3 2 1 0|R/W
$FFFA11|byte |Interrupt In-service B            BIT 7 6 5 4 3 2 1 0|R/W
$FFFA15|byte |Interrupt Mask B                  BIT 7 6 5 4 3 2 1 0|R/W
       |     |MFP Address                           | | | | | | | ||
       |     |$11C                FDC/HDC ----------' | | | | | | ||
       |     |$118          Keyboard/MIDI ------------' | | | | | ||
       |     |$114 (200hz clock)  Timer C --------------' | | | | ||
       |     |$110 (USART timer)  Timer D ----------------' | | | ||
       |     |$10C           Blitter done ------------------' | | ||
       |     |$108     RS-232 CTS - input --------------------' | ||
       |     |$104     RS-232 DCD - input ----------------------' ||
       |     |$100        Centronics Busy ------------------------'|
       |     |1 - Enable Interrupt            0 - Disable Interrupt|
-------+-----+-----------------------------------------------------+----------
$FFFA17|byte |Vector Register                   BIT 7 6 5 4 3 . . .|R/W
       |     |Vector Base Offset -------------------+-+-+-' |      |
       |     |1 - *Software End-interrupt mode -------------+      |
       |     |0 - Automatic End-interrupt mode -------------'      |
       |     |* - Default operating mode                           |
-------+-----+-----------------------------------------------------+----------
$FFFA19|byte |Timer A Control                         BIT 4 3 2 1 0|R/W
$FFFA1B|byte |Timer B Control                         BIT 4 3 2 1 0|R/W
       |     |Reset (force output low) -------------------' | | | ||
       |     +----------------------------------------------+-+-+-++
       |     |0000 - Timer stop, no function executed              |
       |     |0001 - Delay mode, divide by 4                       |
       |     |0010 -     :           :     10                      |
       |     |0011 -     :           :     16                      |
       |     |0100 -     :           :     50                      |
       |     |0101 -     :           :     64                      |
       |     |0110 -     :           :     100                     |
       |     |0111 - Delay mode, divide by 200                     |
       |     |1000 - Event count mode                              |
       |     |1xxx - Pulse extension mode, divide as above         |
       |     +-----------------------------------------------------+
$FFFA1F|byte |Timer A Data                                         |R/W
$FFFA21|byte |Timer B Data                                         |R/W
-------+-----+-----------------------------------------------------+----------
$FFFA1D|byte |Timer C & D Control                 BIT 6 5 4 . 2 1 0|R/W
       |     |                                        Timer   Timer|
       |     |                                          C       D  |
       |     +-----------------------------------------------------+
       |     |000 - Timer stop                                     |
       |     |001 - Delay mode, divide by 4                        |
       |     |010 -      :           :    10                       |
       |     |011 -      :           :    16                       |
       |     |100 -      :           :    50                       |
       |     |101 -      :           :    64                       |
       |     |110 -      :           :    100                      |
       |     |111 - Delay mode, divide by 200                      |
       |     +-----------------------------------------------------+
$FFFA23|byte |Timer C Data                                         |R/W
$FFFA25|byte |Timer D Data                                         |R/W
-------+-----+-----------------------------------------------------+----------
$FFFA27|byte |Sync Character                                       |R/W
$FFFA29|byte |USART Control                     BIT 7 6 5 4 3 2 1 .|R/W
       |     |Clock divide (1 - div by 16) ---------' | | | | | | ||
       |     |Word Length 00 - 8 bits ----------------+-+ | | | | ||
       |     |            01 - 7 bits ----------------+-+ | | | | ||
       |     |            10 - 6 bits ----------------+-+ | | | | ||
       |     |            11 - 5 bits ----------------+-' | | | | ||
       |     |Bits Stop Start Format                      | | | | ||
       |     |00     0    0   Synchronous ----------------+-+ | | ||
       |     |01     1    1   Asynchronous ---------------+-+ | | ||
       |     |10     1    1.5 Asynchronous ---------------+-+ | | ||
       |     |11     1    2   Asynchronous ---------------+-' | | ||
       |     |Parity (0 - ignore parity bit) -----------------' | ||
       |     |Parity (0 - odd parity,1 - even) -----------------' ||
       |     |Unused ---------------------------------------------'|
$FFFA2B|byte |Receiver Status                   BIT 7 6 5 4 3 2 1 0|R/W
       |     |Buffer full --------------------------' | | | | | | ||
       |     |Overrun error --------------------------' | | | | | ||
       |     |Parity error -----------------------------' | | | | ||
       |     |Frame error --------------------------------' | | | ||
       |     |Found - Search/Break detected ----------------' | | ||
       |     |Match/Character in progress --------------------' | ||
       |     |Synchronous strip enable -------------------------' ||
       |     |Receiver enable bit --------------------------------'|
$FFFA2D|byte |Transmitter Status                BIT 7 6 5 4 3 2 1 0|R/W
       |     |Buffer empty -------------------------' | | | | | | ||
       |     |Underrun error -------------------------' | | | | | ||
       |     |Auto turnaround --------------------------' | | | | ||
       |     |End of transmission ------------------------' | | | ||
       |     |Break ----------------------------------------' | | ||
       |     |High bit ---------------------------------------' | ||
       |     |Low bit ------------------------------------------' ||
       |     |Transmitter enable ---------------------------------'|
$FFFA2F|byte |USART data                                           |R/W
-------+-----+-----------------------------------------------------+----------
##############Floating Point Coprocessor (CIR Interface in MSTe)   ###########
-------+-----+-----------------------------------------------------+----------
$FFFA40|word |FP_Stat    Response-Register                         |??? (MSTe)
$FFFA42|word |FP_Ctl     Control-Register                          |??? (MSTe)
$FFFA44|word |FP_Save    Save-Register                             |??? (MSTe)
$FFFA46|word |FP_Restor  Restore-Register                          |??? (MSTe)
$FFFA48|word |                                                     |??? (MSTe)
$FFFA4A|word |FP_Cmd     Command-Register                          |??? (MSTe)
$FFFA4E|word |FP_Ccr     Condition-Code-Register                   |??? (MSTe)
$FFFA50|long |FP_Op      Operand-Register                          |??? (MSTe)
$FFFA54|word |FP_Selct   Register Select                           |??? (MSTe)
$FFFA58|long |FP_Iadr    Instruction Address                       |??? (MSTe)
$FFFA5C|long |           Operand Address                           |??? (MSTe)
-------+-----+-----------------------------------------------------+----------
##############6850 ACIA I/O Chips                                  ###########
-------+-----+-----------------------------------------------------+----------
$FFFC00|byte |Keyboard ACIA control             BIT 7 6 5 4 3 2 1 0|W
       |     |Rx Int enable (1 - enable) -----------' | | | | | | ||
       |     |Tx Interrupts                           | | | | | | ||
       |     |00 - RTS low, Tx int disable -----------+-+ | | | | ||
       |     |01 - RTS low, Tx int enable ------------+-+ | | | | ||
       |     |10 - RTS high, Tx int disable ----------+-+ | | | | ||
       |     |11 - RTS low, Tx int disable,           | | | | | | ||
       |     |     Tx a break onto data out ----------+-' | | | | ||
       |     |Settings                                    | | | | ||
       |     |000 - 7 bit, even, 2 stop bit --------------+-+-+ | ||
       |     |001 - 7 bit, odd, 2 stop bit ---------------+-+-+ | ||
       |     |010 - 7 bit, even, 1 stop bit --------------+-+-+ | ||
       |     |011 - 7 bit, odd, 1 stop bit ---------------+-+-+ | ||
       |     |100 - 8 bit, 2 stop bit --------------------+-+-+ | ||
       |     |101 - 8 bit, 1 stop bit --------------------+-+-+ | ||
       |     |110 - 8 bit, even, 1 stop bit --------------+-+-+ | ||
       |     |111 - 8 bit, odd, 1 stop bit ---------------+-+-' | ||
       |     |Clock divide                                      | ||
       |     |00 - Normal --------------------------------------+-+|
       |     |01 - Div by 16 -----------------------------------+-+|
       |     |10 - Div by 64 -----------------------------------+-+|
       |     |11 - Master reset --------------------------------+-'|
$FFFC00|byte |Keyboard ACIA control             BIT 7 6 5 4 3 2 1 0|R
       |     |Interrupt request --------------------' | | | | | | ||
       |     |Parity error ---------------------------' | | | | | ||
       |     |Rx overrun -------------------------------' | | | | ||
       |     |Framing error ------------------------------' | | | ||
       |     |CTS ------------------------------------------' | | ||
       |     |DCD --------------------------------------------' | ||
       |     |Tx data register empty ---------------------------' ||
       |     |Rx data register full ------------------------------'|
$FFFC02|byte |Keyboard ACIA data                                   |R/W
$FFFC04|byte |MIDI ACIA control                 BIT 7 6 5 4 3 2 1 0|W
       |     |Rx Int enable (1 - enable) -----------' | | | | | | ||
       |     |Tx Interrupts                           | | | | | | ||
       |     |00 - RTS low, Tx int disable -----------+-+ | | | | ||
       |     |01 - RTS low, Tx int enable ------------+-+ | | | | ||
       |     |10 - RTS high, Tx int disable ----------+-+ | | | | ||
       |     |11 - RTS low, Tx int disable,           | | | | | | ||
       |     |     Tx a break onto data out ----------+-' | | | | ||
       |     |Settings                                    | | | | ||
       |     |000 - 7 bit, even, 2 stop bit --------------+-+-+ | ||
       |     |001 - 7 bit, odd, 2 stop bit ---------------+-+-+ | ||
       |     |010 - 7 bit, even, 1 stop bit --------------+-+-+ | ||
       |     |011 - 7 bit, odd, 1 stop bit ---------------+-+-+ | ||
       |     |100 - 8 bit, 2 stop bit --------------------+-+-+ | ||
       |     |101 - 8 bit, 1 stop bit --------------------+-+-+ | ||
       |     |110 - 8 bit, even, 1 stop bit --------------+-+-+ | ||
       |     |111 - 8 bit, odd, 1 stop bit ---------------+-+-' | ||
       |     |Clock divide                                      | ||
       |     |00 - Normal --------------------------------------+-+|
       |     |01 - Div by 16 -----------------------------------+-+|
       |     |10 - Div by 64 -----------------------------------+-+|
       |     |11 - Master reset --------------------------------+-'|
$FFFC04|byte |MIDI ACIA control                 BIT 7 6 5 4 3 2 1 0|R
       |     |Interrupt request --------------------' | | | | | | ||
       |     |Parity error ---------------------------' | | | | | ||
       |     |Rx overrun -------------------------------' | | | | ||
       |     |Framing error ------------------------------' | | | ||
       |     |CTS ------------------------------------------' | | ||
       |     |DCD --------------------------------------------' | ||
       |     |Tx data register empty ---------------------------' ||
       |     |Rx data register full ------------------------------'|
$FFFC06|byte |MIDI ACIA data                                       |R/W
-------+-----+-----------------------------------------------------+----------
##############Realtime Clock                                       ###########
-------+-----+-----------------------------------------------------+----------
$FFFC21|byte |S_Units                                              |???
$FFFC23|byte |S_Tens                                               |???
$FFFC25|byte |M_Units                                              |???
$FFFC27|byte |M_Tens                                               |???
$FFFC29|byte |H_Units                                              |???
$FFFC2B|byte |H_Tens                                               |???
$FFFC2D|byte |Weekday                                              |???
$FFFC2F|byte |Day_Units                                            |???
$FFFC31|byte |Day_Tens                                             |???
$FFFC33|byte |Mon_Units                                            |???
$FFFC35|byte |Mon_Tens                                             |???
$FFFC37|byte |Yr_Units                                             |???
$FFFC39|byte |Yr_Tens                                              |???
$FFFC3B|byte |Cl_Mod                                               |???
$FFFC3D|byte |Cl_Test                                              |???
$FFFC3F|byte |Cl_Reset                                             |???
-------+-----+-----------------------------------------------------+----------

*/

WORD io_read(MEM_ADDRESS addr);
#ifdef DEBUG_BUILD
BYTE io_read_b(MEM_ADDRESS addr);
#endif
void io_write(MEM_ADDRESS addr, DU16 uio_src_w);
void io_write_b(MEM_ADDRESS addr,BYTE io_src_b);
void io_write_w(MEM_ADDRESS addr,WORD io_src_w);

#endif//IORW_DECLA_H
