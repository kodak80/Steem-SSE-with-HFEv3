/*
68K-Disassembler
by edwardOpich3

downloaded from:
https://github.com/edwardOpich3/68K-Disassembler/blob/master/68K_Disassembler

modded for Steem SSE beta
*/ 

#include <pch.h> //SSE
#pragma hdrstop //SSE

#pragma warning (disable : 4100 4244) //SSE

#ifdef SSE_BETA

#include "Disassembler.h"

char Disassembler::hexArray[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
#ifdef SSE_BUILD
char* Disassembler::condArray[] = { "t", "f", "hi", "ls", "cc", "cs", "ne", "eq", "vc", "vs", "pl", "mi", "ge", "lt", "gt", "le" };
#else
std::string Disassembler::condArray[] = { "t", "f", "hi", "ls", "cc", "cs", "ne", "eq", "vc", "vs", "pl", "mi", "ge", "lt", "gt", "le" };
#endif

int data = 0;

const int SIZE_1 = 0xC0;
const int MODE_1 = 0x38;
const int REG_1 = 0x07;

#ifdef SSE_BUILD
EasyStr Disassembler::parseInstruction(char* code, int& offset)
#else
std::string Disassembler::parseInstruction(char* code, int& offset)
#endif
{
	data = 0;
#ifdef SSE_BUILD
	EasyStr line = "";
#else
	std::string line = "";
#endif
	int size = 0;

	// ori, andi, subi, addi, eori, cmpi, btst, bchg, bclr, bset, movep
	if (((unsigned char)code[offset] & 0xF0) == 0x00)
	{
		// ori
		if ((unsigned char)code[offset] == 0x00)
		{
			line = "ori";
			if ((unsigned char)code[offset + 1] == 0x3C)
			{
				data = 1;

				line += ".b #$";
				line += parseByte((unsigned char)code[offset + 3]);
				line += ",ccr";
			}

			else if ((unsigned char)code[offset + 1] == 0x7C)
			{
				data = 2;

				line += ".w #$";
				line += parseByte((unsigned char)code[offset + 2]);
				line += parseByte((unsigned char)code[offset + 3]);
				line += ",sr";
			}

			else
			{
				if ((size = parseSize(code, offset, 0x00, line)) == -1)
				{
					return line;
				}

				line += "#$";

				data = size;

				for (int i = 0; i < size; i++)
				{
					if (data == 1)
					{
						i++;
					}
					line += parseByte((unsigned char)code[offset + (2 + i)]);
				}

				line += ",";

				if (parseDestOperand(code, offset, 0x00, size, line) == -1)
				{
					return line;
				}
			}
		}

		// andi
		else if ((unsigned char)code[offset] == 0x02)
		{
			line = "andi";

			if ((unsigned char)code[offset + 1] == 0x3C)	// ANDI to CCR; immediate byte
			{
				data = 1;

				line += ".b #$";
				line += parseByte((unsigned char)code[offset + 3]);

				line += ",ccr";
			}
			else if ((unsigned char)code[offset + 1] == 0x7C)	// ANDI to SR; immediate word
			{
				data = 2;

				line += ".w #$";
				line += parseByte((unsigned char)code[offset + 2]);
				line += parseByte((unsigned char)code[offset + 3]);

				line += ",sr";
			}

			else
			{
				if ((size = parseSize(code, offset, 0x02, line)) == -1)
				{
					return line;
				}

				line += "#$";

				data = size;

				for (int i = 0; i < size; i++)
				{
					if (data == 1)
					{
						i++;
					}
					line += parseByte((unsigned char)code[offset + (2 + i)]);
				}

				line += ",";

				if (parseDestOperand(code, offset, 0x02, size, line) == -1)
				{
					return line;
				}
			}
		}

		// subi
		else if ((unsigned char)code[offset] == 0x04)
		{
			line = "subi";

			if ((size = parseSize(code, offset, 0x04, line)) == -1)
			{
				return line;
			}

			line += "#$";

			data = size;

			for (int i = 0; i < size; i++)
			{
				if (data == 1)
				{
					i++;
				}
				line += parseByte((unsigned char)code[offset + (2 + i)]);
			}

			line += ",";

			if (parseDestOperand(code, offset, 0x04, size, line) == -1)
			{
				return line;
			}
		}

		// addi
		else if ((unsigned char)code[offset] == 0x06)
		{
			line = "addi";

			if ((size = parseSize(code, offset, 0x06, line)) == -1)
			{
				return line;
			}

			line += "#$";

			data = size;

			for (int i = 0; i < size; i++)
			{
				if (data == 1)
				{
					i++;
				}
				line += parseByte((unsigned char)code[offset + (2 + i)]);
			}

			line += ",";

			if (parseDestOperand(code, offset, 0x06, size, line) == -1)
			{
				return line;
			}
		}

		// eori
		else if ((unsigned char)code[offset] == 0x0A)
		{
			line = "eori";
			if ((unsigned char)code[offset + 1] == 0x3C)
			{
				data = 1;

				line += ".b #$";
				line += parseByte((unsigned char)code[offset + 3]);
				line += ",ccr";
			}

			else if ((unsigned char)code[offset + 1] == 0x7C)
			{
				data = 2;

				line += ".w #$";
				line += parseByte((unsigned char)code[offset + 2]);
				line += parseByte((unsigned char)code[offset + 3]);
				line += ",sr";
			}

			else
			{
				if ((size = parseSize(code, offset, 0x0A, line)) == -1)
				{
					return line;
				}

				line += "#$";

				data = size;

				for (int i = 0; i < size; i++)
				{
					if (data == 1)
					{
						i++;
					}
					line += parseByte((unsigned char)code[offset + (2 + i)]);
				}

				line += ",";

				if (parseDestOperand(code, offset, 0x0A, size, line) == -1)
				{
					return line;
				}
			}
		}

		// cmpi
		else if ((unsigned char)code[offset] == 0x0C)
		{
			line = "cmpi";

			if ((size = parseSize(code, offset, 0x0C, line)) == -1)
			{
				return line;
			}

			line += "#$";

			data = size;

			for (int i = 0; i < size; i++)
			{
				if (data == 1)
				{
					i++;
				}
				line += parseByte((unsigned char)code[offset + (2 + i)]);
			}

			line += ",";

			if (parseDestOperand(code, offset, 0x0C, size, line) == -1)
			{
				return line;
			}
		}

		// btst, bchg, bclr, bset (immediate)
		else if ((unsigned char)code[offset] == 0x08)
		{
			// Figure out which instruction it is
			if ((((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x00)
			{
				line = "btst";
			}
			else if ((((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x01)
			{
				line = "bchg";
			}
			else if ((((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x02)
			{
				line = "bclr";
			}
			else if ((((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x03)
			{
				line = "bset";
			}

			line += " #$";

			data = 2;

			// Size is byte by default
			line += parseByte((unsigned char)code[offset + 3]);

			line += ",";

			if (parseDestOperand(code, offset, 0x08, 1, line) == -1)
			{
				return line;
			}
		}

		// btst, bchg, bclr, bset (data register), and movep
		else if ((unsigned char)code[offset] < 0x10 && (((unsigned char)code[offset] & 0x01) >> 0) == 0x01)
		{
			// Figure out which instruction we're dealing with

			// btst, bchg, bclr, bset
			if ((((unsigned char)code[offset + 1] & 0x38) >> 3) != 0x01)
			{
				if ((((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x00)
				{
					line = "btst";
				}
				else if ((((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x01)
				{
					line = "bchg";
				}
				else if ((((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x02)
				{
					line = "bclr";
				}
				else if ((((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x03)
				{
					line = "bset";
				}

				data = 0;

				line += " d";

				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];

				line += ",";

				if (parseDestOperand(code, offset, 0x08, 1, line) == -1)
				{
					return line;
				}
			}

			// movep
			else if ((((unsigned char)code[offset + 1] & 0x38) >> 3) == 0x01)
			{
				line = "movep";

				// Get size
				if ((((unsigned char)code[offset + 1] & 0x40) >> 6) == 0x00)
				{
					line += ".w ";
				}
				else if ((((unsigned char)code[offset + 1] & 0x40) >> 6) == 0x01)
				{
					line += ".l ";
				}

				data = 2;

				// Get the order of operands

				// Memory to register
				if ((((unsigned char)code[offset + 1] & 0x80) >> 7) == 0x00)
				{
					line += "$";
					line += parseByte((unsigned char)code[offset + 2]);
					line += parseByte((unsigned char)code[offset + 3]);

					line += "(a";
					line += hexArray[(unsigned char)code[offset + 1] & 0x07];
					line += "),";

					line += "d";
					line += hexArray[(unsigned char)code[offset] & 0x0E];
				}

				// Register to memory
				else if ((((unsigned char)code[offset + 1] & 0x80) >> 7) == 0x01)
				{
					line += "d";
					line += hexArray[(unsigned char)code[offset] & 0x0E];

					line += ",";

					line += "$";
					line += parseByte((unsigned char)code[offset + 2]);
					line += parseByte((unsigned char)code[offset + 3]);

					line += "(a";
					line += hexArray[(unsigned char)code[offset + 1] & 0x07];
					line += ")";
				}
			}
		}
	}

	// movea, move
	else if ((unsigned char)code[offset] < 0x40)
	{
		// Determine which one we're using via complex bit manipulation

		// movea
		if ((((unsigned char)code[offset] & 0x01) << 2) + (((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x01)
		{
			line = "movea";
		}

		// move
		else if ((((unsigned char)code[offset] & 0x01) << 2) + (((unsigned char)code[offset + 1] & 0xC0) >> 6) != 0x01)
		{
			line = "move";
		}

		// Get size
		if ((((unsigned char)code[offset] & 0x30) >> 4) == 0x01)
		{
			line += ".b ";
			size = 1;
		}
		else if ((((unsigned char)code[offset] & 0x30) >> 4) == 0x03)
		{
			line += ".w ";
			size = 2;
		}
		else if ((((unsigned char)code[offset] & 0x30) >> 4) == 0x02)
		{
			line += ".l ";
			size = 4;
		}

		// Source Operand
		if (parseSourceOperand(code, offset, 0x10, size, line) == -1)
		{
			return line;
		}

		line += ",";

		if (parseDestOperand(code, offset, 0x10, size, line) == -1)
		{
			return line;
		}
	}

	// move from sr, move to ccr, move to sr, negx, clr, neg, not, ext...
	// nbcd, swap, pea, illegal, tas, tst, trap, link, unlk, move usp...
	// reset, nop, stop, rte, rts, trapv, rtr, jsr, jmp, movem, lea, chk
	else if (((unsigned char)code[offset] & 0xF0) == 0x40)
	{
		// move from sr, move to ccr, and move to sr
		if ((unsigned char)code[offset] < 0x48 && (((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x03 && ((unsigned char)code[offset] & 0x01) == 0x00)
		{
			line = "move";

			// move from sr
			if (((unsigned char)code[offset] & 0x06) >> 1 == 0x00)
			{
				line += " sr,";

				if (parseDestOperand(code, offset, 0x40, 2, line) == -1)
				{
					return line;
				}
			}

			// move to ccr
			else if (((unsigned char)code[offset] & 0x06) >> 1 == 0x02)
			{
				line += " ";

				if (parseSourceOperand(code, offset, 0x44, 1, line) == -1)
				{
					return line;
				}

				line += ",ccr";
			}

			// move to sr
			else if (((unsigned char)code[offset] & 0x06) >> 1 == 0x03)
			{
				line += " ";

				if (parseSourceOperand(code, offset, 0x46, 2, line) == -1)
				{
					return line;
				}

				line += ",sr";
			}
		}

		// negx
		else if ((unsigned char)code[offset] == 0x40)
		{
			line = "negx";

			// get size
			if ((size = parseSize(code, offset, 0x40, line)) == -1)
			{
				return line;
			}

			if (parseDestOperand(code, offset, 0x40, size, line) == -1)
			{
				return line;
			}
		}

		// clr
		else if ((unsigned char)code[offset] == 0x42)
		{
			line = "clr";

			if ((size = parseSize(code, offset, 0x42, line)) == -1)
			{
				return line;
			}

			if (parseDestOperand(code, offset, 0x42, size, line) == -1)
			{
				return line;
			}
		}

		// neg
		else if ((unsigned char)code[offset] == 0x44)
		{
			line = "neg";

			if ((size = parseSize(code, offset, 0x44, line)) == -1)
			{
				return line;
			}

			if (parseDestOperand(code, offset, 0x44, size, line) == -1)
			{
				return line;
			}
		}

		// not
		else if ((unsigned char)code[offset] == 0x46)
		{
			line = "not";

			if ((size = parseSize(code, offset, 0x46, line)) == -1)
			{
				return line;
			}

			if (parseDestOperand(code, offset, 0x46, size, line) == -1)
			{
				return line;
			}
		}

		// ext
		else if ((unsigned char)code[offset] == 0x48 && (((unsigned char)code[offset + 1] & 0x38) >> 3) == 0x00 && (((unsigned char)code[offset + 1] & 0x80) >> 7) == 0x01)
		{
			line = "ext";

			// Get Size
			if ((((unsigned char)code[offset + 1] & 0x40) >> 6) == 0x00)
			{
				size = 2;
				line += ".w ";
			}
			else if ((((unsigned char)code[offset + 1] & 0x40) >> 6) == 0x01)
			{
				size = 4;
				line += ".l ";
			}

			line += "d";
			line += hexArray[((unsigned char)code[offset + 1] & REG_1) >> 0];
		}

		// nbcd
		else if ((unsigned char)code[offset] == 0x48 && (((unsigned char)code[offset] & 0x01) << 2) + (((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x00)
		{
			line = "nbcd.b ";

			if (parseDestOperand(code, offset, 0x48, 1, line) == -1)
			{
				return line;
			}
		}

		// swap
		else if ((unsigned char)code[offset] == 0x48 && (((unsigned char)code[offset] & 0x01) << 2) + (((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x01 && (((unsigned char)code[offset + 1] & 0x38) >> 3) == 0x00)
		{
			line = "swap.w d";

			line += hexArray[(unsigned char)code[offset + 1] & 0x07];
		}

		// pea
		else if ((unsigned char)code[offset] == 0x48 && (((unsigned char)code[offset] & 0x01) << 2) + (((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x01)
		{
			line = "pea.l ";

			if (parseSourceOperand(code, offset, 0x48, 4, line) == -1)
			{
				return line;
			}
		}

		// illegal
		else if ((unsigned char)code[offset] == 0x4A && (unsigned char)code[offset + 1] == 0xFC)
		{
			line = "illegal";
		}

		// tas
		else if ((unsigned char)code[offset] == 0x4A && ((unsigned char)code[offset + 1] & 0xC0) >> 6 == 0x03)
		{
			line = "tas.b ";

			if (parseDestOperand(code, offset, 0x4A, 1, line) == -1)
			{
				return line;
			}
		}

		// tst
		else if ((unsigned char)code[offset] == 0x4A)
		{
			line = "tst";

			if ((size = parseSize(code, offset, 0x4A, line)) == -1)
			{
				return line;
			}

			if (parseSourceOperand(code, offset, 0x4A, size, line) == -1)
			{
				return line;
			}
		}

		// trap
		else if ((unsigned char)code[offset] == 0x4E && ((unsigned char)code[offset + 1] & 0xF0) >> 4 == 0x04)
		{
			line = "trap #";

			if (((unsigned char)code[offset + 1] & 0x0F) < 10)
			{
				line += hexArray[(unsigned char)code[offset + 1] & 0x0F];
			}
			else
			{
				line += "1";
				line += hexArray[((unsigned char)code[offset + 1] & 0x0F) - 10];
			}
		}

		// link, unlk
		else if ((unsigned char)code[offset] == 0x4E && ((unsigned char)code[offset + 1] & 0xF0) >> 4 == 0x05)
		{
			// link
			if (((unsigned char)code[offset + 1] & 0x08) >> 3 == 0x00)
			{
				line = "link a";

				line += hexArray[(unsigned char)code[offset + 1] & 0x07];
				line += ",#$";

				data = 2;

				for (int i = 0; i < 2; i++)
				{
					line += parseByte((unsigned char)code[offset + 2 + i]);
				}
			}

			// unlk
			else if (((unsigned char)code[offset + 1] & 0x08) >> 3 == 0x01)
			{
				line = "unlk a";

				line += hexArray[(unsigned char)code[offset + 1] & 0x07];
			}
		}

		// move usp
		else if ((unsigned char)code[offset] == 0x4E && ((unsigned char)code[offset + 1] & 0xF0) >> 4 == 0x06)
		{
			line = "move.l ";

			// to usp
			if (((unsigned char)code[offset + 1] & 0x08) >> 3 == 0x00)
			{
				line += "a";

				line += hexArray[(unsigned char)code[offset + 1] & 0x07];

				line += ",usp";
			}

			// from usp
			else if (((unsigned char)code[offset + 1] & 0x08) >> 3 == 0x01)
			{
				line += "usp,a";

				line += hexArray[(unsigned char)code[offset + 1] & 0x07];
			}
		}

		// reset
		else if ((unsigned char)code[offset] == 0x4E && (unsigned char)code[offset + 1] == 0x70)
		{
			line = "reset";
		}

		// nop
		else if ((unsigned char)code[offset] == 0x4E && (unsigned char)code[offset + 1] == 0x71)
		{
			line = "nop";
		}

		// stop
		else if ((unsigned char)code[offset] == 0x4E && (unsigned char)code[offset + 1] == 0x72)
		{
			line = "stop #$";

			data = 2;

			for (int i = 0; i < 2; i++)
			{
				line += parseByte((unsigned char)code[offset + 2 + i]);
			}
		}

		// rte
		else if ((unsigned char)code[offset] == 0x4E && (unsigned char)code[offset + 1] == 0x73)
		{
			line = "rte";
		}

		// rts
		else if ((unsigned char)code[offset] == 0x4E && (unsigned char)code[offset + 1] == 0x75)
		{
			line = "rts";
		}

		// trapv
		else if ((unsigned char)code[offset] == 0x4E && (unsigned char)code[offset + 1] == 0x76)
		{
			line = "trapv";
		}

		// rtr
		else if ((unsigned char)code[offset] == 0x4E && (unsigned char)code[offset + 1] == 0x77)
		{
			line = "rtr";
		}

		// jsr, jmp
		else if ((unsigned char)code[offset] == 0x4E)
		{
			// jsr
			if (((unsigned char)code[offset + 1] & 0x40) >> 6 == 0x00)
			{
				line = "jsr ";

				if (parseSourceOperand(code, offset, 0x4E, 0, line) == -1)
				{
					return line;
				}
			}

			// jmp
			else if (((unsigned char)code[offset + 1] & 0x40) >> 6 == 0x01)
			{
				line = "jmp ";

				if (parseSourceOperand(code, offset, 0x4E, 0, line) == -1)
				{
					return line;
				}
			}
		}

		// movem
		else if ((unsigned char)code[offset] < 0x4E && ((unsigned char)code[offset] & 0x01) == 0x00)
		{
			line = "movem";

			// get size
			if ((((unsigned char)code[offset + 1] & 0x40) >> 6) == 0x00)
			{
				size = 2;
				line += ".w ";
			}
			else if ((((unsigned char)code[offset + 1] & 0x40) >> 6) == 0x01)
			{
				size = 4;
				line += ".l ";
			}

			// get direction
			// register(s) to memory
			if ((((unsigned char)code[offset] & 0x04) >> 2) == 0x00)
			{
				// source operand(s)
				bool predec = ((((unsigned char)code[offset + 1] & 0x38) >> 3) == 0x04);
				bool first = true;
				for (int i = 7; i >= 0; i--)
				{
					if ((((unsigned char)code[offset + 2] & (int)pow(2.0f,i)) >> i) == 0x01)
					{
						if (first)
						{
							first = false;
						}
						else
						{
							line += "/";
						}

						if (predec)
						{
							line += "a";
							line += hexArray[i];
						}
						else
						{
							line += "d";
							line += hexArray[7 - i];
						}
					}
				}
				for (int i = 7; i >= 0; i--)
				{
					if ((((unsigned char)code[offset + 3] & (int)pow(2.0f, i)) >> i) == 0x01)
					{
						if (first)
						{
							first = false;
						}
						else
						{
							line += "/";
						}

						if (predec)
						{
							line += "d";
							line += hexArray[i];
						}
						else
						{
							line += "a";
							line += hexArray[7 - i];
						}
					}
				}

				data = 2;

				line += ",";

				// destination operand
				if (parseDestOperand(code, offset, 0x4E, size, line) == -1)
				{
					return line;
				}
			}

			// memory to register(s)
			else if ((((unsigned char)code[offset] & 0x04) >> 2) == 0x01)
			{
				data = 2;

				// source operand
				if (parseSourceOperand(code, offset, 0x4E, size, line) == -1)
				{
					return line;
				}

				line += ",";

				// destination operand(s)
				bool first = true;
				for (int i = 7; i >= 0; i--)
				{
					if ((((unsigned char)code[offset + 2] & (int)pow(2.0f, i)) >> i) == 0x01)
					{
						if (first)
						{
							first = false;
						}
						else
						{
							line += "/";
						}

						line += "d";
						line += hexArray[7 - i];
					}
				}
				for (int i = 7; i >= 0; i--)
				{
					if ((((unsigned char)code[offset + 3] & (int)pow(2.0f, i)) >> i) == 0x01)
					{
						if (first)
						{
							first = false;
						}
						else
						{
							line += "/";
						}

						line += "a";
						line += hexArray[7 - i];
					}
				}
			}
		}

		// lea, chk
		else if ((unsigned char)code[offset] < 0x50)
		{
			// lea
			if ((((unsigned char)code[offset + 1] & 0x40) >> 6) == 0x01)
			{
				line = "lea.l ";

				if (parseSourceOperand(code, offset, 0x4E, 4, line) == -1)
				{
					return line;
				}

				line += ",";

				line += "a";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
			}

			// chk
			else if ((((unsigned char)code[offset + 1] & 0x40) >> 6) == 0x00)
			{
				line = "chk.w ";

				if (parseSourceOperand(code, offset, 0x4E, 2, line) == -1)
				{
					return line;
				}

				line += ",";

				line += "d";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
			}
		}
	}

	// addq, subq, scc, dbcc
	else if (((unsigned char)code[offset] & 0xF0) == 0x50)
	{
		// addq, subq
		if ((((unsigned char)code[offset + 1] & 0xC0) >> 6) != 0x03)
		{
			// addq
			if (((unsigned char)code[offset] & 0x01) == 0x00)
			{
				line = "addq";
			}

			// subq
			else if (((unsigned char)code[offset] & 0x01) == 0x01)
			{
				line = "subq";
			}

			if ((size = parseSize(code, offset, 0x50, line)) == -1)
			{
				return line;
			}

			line += "#$";
			line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
			line += ",";

			if (parseDestOperand(code, offset, 0x50, size, line) == -1)
			{
				return line;
			}
		}

		// scc, dbcc
		else if ((((unsigned char)code[offset + 1] & 0xC0) >> 6) == 0x03)
		{
			// scc
			if ((((unsigned char)code[offset + 1] & 0x38) >> 3) != 0x01)
			{
				line = "s";
				line += condArray[(unsigned char)code[offset] & 0x0F];

				line += ".b ";

				if (parseDestOperand(code, offset, 0x50, 1, line) == -1)
				{
					return line;
				}
			}

			// dbcc
			else if ((((unsigned char)code[offset + 1] & 0x38) >> 3) == 0x01)
			{
				line = "db";
				line += condArray[(unsigned char)code[offset] & 0x0F];

				line += " d";
				line += hexArray[(unsigned char)code[offset + 1] & 0x07];
				line += ",$";

				int displacement = (signed int)(((unsigned char)code[offset + 2] * 256) + (unsigned char)code[offset + 3]);
				displacement += (offset + 2);
				char highByte = displacement / 256;
				char lowByte = displacement - (highByte * 256);

				line += parseByte(highByte);
				line += parseByte(lowByte);

				data = 2;
			}
		}
	}

	// bra, bsr, bcc
	else if (((unsigned char)code[offset] & 0xF0) == 0x60)
	{
		// bra
		if (((unsigned char)code[offset] & 0x0F) == 0x00)
		{
			line = "bra";
		}

		// bsr
		else if (((unsigned char)code[offset] & 0x0F) == 0x01)
		{
			line = "bsr";
		}

		// bcc
		else
		{
			line = "b";
			line += condArray[(unsigned char)code[offset] & 0x0F];
		}

		// get size
		if ((unsigned char)code[offset + 1] == 0x00)
		{
			size = 2;
			line += ".w $";

			int displacement = (signed int)(((unsigned char)code[offset + 2] * 256) + (unsigned char)code[offset + 3]);
			displacement += (offset + 2);
			char highByte = displacement / 256;
			char lowByte = displacement - (highByte * 256);

			line += parseByte(highByte);
			line += parseByte(lowByte);

			data = 2;
		}
		else
		{
			size = 1;
			line += ".b $";

			int displacement = (signed char)code[offset + 1];
			displacement += (offset + data + 2);
			char highByte = displacement / 256;
			char lowByte = displacement - (highByte * 256);

			line += parseByte(highByte);
			line += parseByte(lowByte);

			data = 0;
		}		
	}

	// moveq
	else if (((unsigned char)code[offset] & 0xF0) == 0x70)
	{
		line = "moveq.l #$";

		line += parseByte((unsigned char)code[offset + 1]);

		line += ",d";

		line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
	}

	// divu, divs, sbcd, or
	else if (((unsigned char)code[offset] & 0xF0) == 0x80)
	{
		if (((unsigned char)code[offset + 1] & 0xC0) == 0xC0)
		{
			// divu
			if (((unsigned char)code[offset] & 0x01) == 0x00)
			{
				line = "divu.w ";
			}

			// divs
			else if (((unsigned char)code[offset] & 0x01) == 0x01)
			{
				line = "divs.w ";
			}

			if (parseSourceOperand(code, offset, 0x80, 2, line) == -1)
			{
				return line;
			}

			line += ",d";

			line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
		}


		// sbcd
		else if ((((unsigned char)code[offset] & 0x01) == 0x01) && (((unsigned char)code[offset + 1] & 0x38) >> 3) <= 0x01)
		{
			line = "sbcd.b ";

			// Which mode?
			if (((unsigned char)code[offset + 1] & 0x08) == 0x00)
			{
				line += "d";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];

				line += ",d";
				line += hexArray[((unsigned char)code[offset + 1] & 0x07)];
			}
			else
			{
				line += "-(a";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];

				line += "),-(a";
				line += hexArray[((unsigned char)code[offset + 1] & 0x07)];
				line += ")";
			}
		}

		// or
		else
		{
			line = "or";

			if ((size = parseSize(code, offset, 0x80, line)) == -1)
			{
				return line;
			}

			if (((unsigned char)code[offset] & 0x01) == 0x01)
			{
				line += "d";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
				line += ",";

				if (parseDestOperand(code, offset, 0x80, size, line) == -1)
				{
					return line;
				}
			}

			else if (((unsigned char)code[offset] & 0x01) == 0x00)
			{
				if (parseSourceOperand(code, offset, 0x80, size, line) == -1)
				{
					return line;
				}

				line += ",d";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
			}
		}
	}

	// sub, subx, suba
	else if (((unsigned char)code[offset] & 0xF0) == 0x90)
	{
		line = "sub";

		// suba
		if (((unsigned char)code[offset + 1] & 0xC0) == 0xC0)
		{
			line += "a";

			if (((unsigned char)code[offset] & 0x01) == 0x00)
			{
				size = 2;
				line += ".w ";
			}
			else
			{
				size = 4;
				line += ".l ";
			}

			if (parseSourceOperand(code, offset, 0x90, size, line) == -1)
			{
				return line;
			}

			line += ",a";
			line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
		}

		// subx
		else if (((unsigned char)code[offset] & 0x01) == 0x01 && ((unsigned char)code[offset + 1] & 0x38) <= 0x08)
		{
			line += "x";

			if ((size = parseSize(code, offset, 0x90, line)) == -1)
			{
				return line;
			}

			if (((unsigned char)code[offset + 1] & 0x08) == 0x00)
			{
				line += "d";
				line += hexArray[((unsigned char)code[offset + 1] & 0x07)];

				line += ",d";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
			}
			else if (((unsigned char)code[offset + 1] & 0x08) == 0x08)
			{
				line += "-(a";
				line += hexArray[((unsigned char)code[offset + 1] & 0x07)];

				line += "),-(a";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
				line += ")";
			}
		}

		// sub
		else
		{
			if ((size = parseSize(code, offset, 0x90, line)) == -1)
			{
				return line;
			}

			if (((unsigned char)code[offset] & 0x01) == 0x01)
			{
				line += "d";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
				line += ",";

				if (parseDestOperand(code, offset, 0x90, size, line) == -1)
				{
					return line;
				}
			}
			else if (((unsigned char)code[offset] & 0x01) == 0x00)
			{
				if (parseSourceOperand(code, offset, 0x90, size, line) == -1)
				{
					return line;
				}

				line += ",d";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
			}
		}
	}

	// eor, cmpm, cmp, cmpa
	else if (((unsigned char)code[offset] & 0xF0) == 0xB0)
	{
		// cmpa
		if (((unsigned char)code[offset + 1] & 0xC0) == 0xC0)
		{
			line = "cmpa";

			if (((unsigned char)code[offset] & 0x01) == 0x00)
			{
				size = 2;
				line += ".w ";
			}
			else if (((unsigned char)code[offset] & 0x01) == 0x01)
			{
				size = 4;
				line += ".l ";
			}

			if (parseSourceOperand(code, offset, 0xB0, size, line) == -1)
			{
				return line;
			}

			line += ",a";
			line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
		}

		// eor, cmpm
		else if (((unsigned char)code[offset] & 0x01) == 0x01)
		{
			// eor
			if (((unsigned char)code[offset + 1] & 0x38) != 0x08)
			{
				line = "eor";

				if ((size = parseSize(code, offset, 0xB0, line)) == -1)
				{
					return line;
				}

				line += "d";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];

				line += ",";

				if (parseDestOperand(code, offset, 0xB0, size, line) == -1)
				{
					return line;
				}
			}

			// cmpm
			else
			{
				line = "cmpm";

				if ((size = parseSize(code, offset, 0xB0, line)) == -1)
				{
					return line;
				}

				line += "(a";
				line += hexArray[((unsigned char)code[offset + 1] & 0x07)];

				line += ")+,(a";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
				line += ")+";
			}
		}

		// cmp
		else
		{
			line = "cmp";

			if ((size = parseSize(code, offset, 0xB0, line)) == -1)
			{
				return line;
			}

			if (parseSourceOperand(code, offset, 0xB0, size, line) == -1)
			{
				return line;
			}

			line += ",d";
			line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
		}
	}

	// mulu, muls, abcd, exg, and
	else if (((unsigned char)code[offset] & 0xF0) == 0xC0)
	{
		// mulu, muls
		if (((unsigned char)code[offset + 1] & 0xC0) == 0xC0)
		{
			// mulu
			if (((unsigned char)code[offset] & 0x01) == 0x00)
			{
				line = "mulu.w ";
			}

			// muls
			else if (((unsigned char)code[offset] & 0x01) == 0x01)
			{
				line = "muls.w ";
			}

			if (parseSourceOperand(code, offset, 0xC0, 2, line) == -1)
			{
				return line;
			}

			line += ",d";
			line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
		}

		// abcd, exg
		else if (((unsigned char)code[offset + 1] & 0x30) == 0x00 && ((unsigned char)code[offset] & 0x01) == 0x01)
		{
			// abcd
			if (((unsigned char)code[offset + 1] & 0xC0) == 0x00)
			{
				line = "abcd.b ";

				if (((unsigned char)code[offset + 1] & 0x08) == 0x00)
				{
					line += "d";
					line += hexArray[((unsigned char)code[offset + 1] & 0x07)];

					line += ",d";
					line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
				}

				else if (((unsigned char)code[offset + 1] & 0x08) == 0x08)
				{
					line += "-(a";
					line += hexArray[((unsigned char)code[offset + 1] & 0x07)];

					line += "),-(a";
					line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
					line += ")";
				}
			}

			// exg
			else if (((unsigned char)code[offset + 1] & 0x30) == 0x00)
			{
				line = "exg.l ";

				// data reg and data reg
				if ((((unsigned char)code[offset + 1] & 0xF8) >> 3) == 0x08)
				{
					line += "d";
					line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
					
					line += ",d";
					line += hexArray[((unsigned char)code[offset + 1] & 0x07)];
				}

				// address reg and address reg
				else if ((((unsigned char)code[offset + 1] & 0xF8) >> 3) == 0x09)
				{
					line += "a";
					line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];

					line += ",a";
					line += hexArray[((unsigned char)code[offset + 1] & 0x07)];
				}

				// data reg and address reg
				else if ((((unsigned char)code[offset + 1] & 0xF8) >> 3) == 0x11)
				{
					line += "d";
					line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];

					line += ",a";
					line += hexArray[((unsigned char)code[offset + 1] & 0x07)];
				}
			}
		}

		// and
		else
		{
			line = "and";

			if ((size = parseSize(code, offset, 0xC0, line)) == -1)
			{
				return line;
			}

			if (((unsigned char)code[offset] & 0x01) == 0x00)
			{
				if (parseSourceOperand(code, offset, 0xC0, size, line) == -1)
				{
					return line;
				}

				line += ",d";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
			}
			else
			{
				line += "d";
				line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];

				line += ",";
				if (parseDestOperand(code, offset, 0xC0, size, line) == -1)
				{
					return line;
				}
			}
		}
	}

	// add, addx, adda
	else if (((unsigned char)code[offset] & 0xF0) == 0xD0)
	{
		line = "add";

		// add, addx
		if (((unsigned char)code[offset + 1] & 0xC0) < 0xC0)
		{
			// addx
			if (((unsigned char)code[offset] & 0x01) == 0x01 && ((unsigned char)code[offset + 1] & 0x30) == 0x00)
			{
				line += "x";

				if ((size = parseSize(code, offset, 0xD0, line)) == -1)
				{
					return line;
				}

				if (((unsigned char)code[offset + 1] & 0x08) == 0x00)
				{
					line += "d";
					line += hexArray[((unsigned char)code[offset + 1] & 0x07)];

					line += ",d";
					line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
				}

				else if (((unsigned char)code[offset + 1] & 0x08) == 0x08)
				{
					line += "-(a";
					line += hexArray[((unsigned char)code[offset + 1] & 0x07)];

					line += "),-(a";
					line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
					line += ")";
				}
			}

			// add
			else
			{
				if ((size = parseSize(code, offset, 0xD0, line)) == -1)
				{
					return line;
				}

				if (((unsigned char)code[offset] & 0x01) == 0x00)
				{
					if (parseSourceOperand(code, offset, 0xD0, size, line) == -1)
					{
						return line;
					}

					line += ",d";
					line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
				}

				else if (((unsigned char)code[offset] & 0x01) == 0x01)
				{
					line += "d";
					line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];

					line += ",";

					if (parseDestOperand(code, offset, 0xD0, size, line) == -1)
					{
						return line;
					}
				}
			}
		}

		// adda
		else
		{
			line += "a";

			if (((unsigned char)code[offset] & 0x01) == 0x00)
			{
				size = 2;
				line += ".w ";
			}
			else if (((unsigned char)code[offset] & 0x01) == 0x01)
			{
				size = 4;
				line += ".l ";
			}

			if (parseSourceOperand(code, offset, 0xD0, size, line) == -1)
			{
				return line;
			}

			line += ",a";
			line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];
		}
	}

	// asd, lsd, roxd, rod (memory and register)
	else if (((unsigned char)code[offset] & 0xF0) == 0xE0)
	{
		// memory
		if (((unsigned char)code[offset + 1] & 0xC0) == 0xC0)
		{
			// asd
			if (((unsigned char)code[offset] & 0x06) == 0x00)
			{
				line = "as";
			}
			// lsd
			else if (((unsigned char)code[offset] & 0x06) == 0x02)
			{
				line = "ls";
			}
			// roxd
			else if (((unsigned char)code[offset] & 0x06) == 0x04)
			{
				line = "rox";
			}
			// rod
			else if (((unsigned char)code[offset] & 0x06) == 0x06)
			{
				line = "ro";
			}

			if (((unsigned char)code[offset] & 0x01) == 0x00)
			{
				line += "r";
			}
			else
			{
				line += "l";
			}

			line += ".w ";

			if (parseDestOperand(code, offset, 0xE0, 2, line) == -1)
			{
				return line;
			}
		}

		// register
		else
		{
			// asd
			if (((unsigned char)code[offset + 1] & 0x18) == 0x00)
			{
				line = "as";
			}
			// lsd
			else if (((unsigned char)code[offset + 1] & 0x18) == 0x08)
			{
				line = "ls";
			}
			// roxd
			else if (((unsigned char)code[offset + 1] & 0x18) == 0x10)
			{
				line = "rox";
			}
			// rod
			else if (((unsigned char)code[offset + 1] & 0x18) == 0x18)
			{
				line = "ro";
			}

			if (((unsigned char)code[offset] & 0x01) == 0x00)
			{
				line += "r";
			}
			else
			{
				line += "l";
			}

			if ((size = parseSize(code, offset, 0xE0, line)) == -1)
			{
				return line;
			}

			if (((unsigned char)code[offset + 1] & 0x20) == 0x00)
			{
				line += "#$";
			}
			else if (((unsigned char)code[offset + 1] & 0x20) == 0x20)
			{
				line += "d";
			}

			line += hexArray[((unsigned char)code[offset] & 0x0E) >> 1];

			line += ",d";
			line += hexArray[((unsigned char)code[offset + 1] & 0x07)];
		}
	}

	else
	{
		line = "dc.w $";
		for (int i = 0; i < 2; i++)
		{
			line += parseByte(code[offset + i]);
		}

		line += "\t; Invalid Opcode";
	}
#ifndef SSE_BUILD
	line += "\t; offset $";

	char temp = offset / 0x1000000;
	line += parseByte(temp);

	temp = offset / 0x10000;
	temp = temp & 0x00FF;
	line += parseByte(temp);

	temp = offset / 0x100;
	temp = temp & 0x0000FF;
	line += parseByte(temp);

	temp = offset;
	temp = temp & 0x000000FF;
	line += parseByte(temp);

	line += "\n";
#endif
	if (data == 1)
	{
		data = 2;
	}

	offset += 2 + data;

	return line;
}

#ifdef SSE_BUILD
int Disassembler::parseSize(char* code, int& offset, int opcode, EasyStr& line)
#else
int Disassembler::parseSize(char* code, int& offset, int opcode, std::string& line)
#endif
{
	int size;

	if ((((unsigned char)code[offset + 1] & SIZE_1) >> 6) == 0x00)
	{
		line += ".b ";
		size = 1;
	}
	else if ((((unsigned char)code[offset + 1] & SIZE_1) >> 6) == 0x01)
	{
		line += ".w ";
		size = 2;
	}
	else if ((((unsigned char)code[offset + 1] & SIZE_1) >> 6) == 0x02)
	{
		line += ".l ";
		size = 4;
	}
	else
	{
		line = "dc.w $";
		for (int i = 0; i < 2; i++)
		{
			line += parseByte(code[offset + i]);
		}

		line += "\t; Invalid Opcode";
#ifndef SSE_BUILD
		line += "\t; offset $";
		char temp = offset / 0x1000000;
		line += parseByte(temp);

		temp = offset / 0x10000;
		temp = temp & 0x00FF;
		line += parseByte(temp);

		temp = offset / 0x100;
		temp = temp & 0x0000FF;
		line += parseByte(temp);

		temp = offset;
		temp = temp & 0x000000FF;
		line += parseByte(temp);

		line += "\n";
#endif
		offset += 2;
		size = -1;
	}

	return size;
}

#ifdef SSE_BUILD
int Disassembler::parseSourceOperand(char* code, int& offset, int opcode, int size, EasyStr& line)
#else
int Disassembler::parseSourceOperand(char* code, int& offset, int opcode, int size, std::string& line)
#endif
{
	int mode = -1;
	int reg = -1;

	// btst, bset, bchg, bclr, movep
	if (((unsigned char)code[offset] & 0xF0) == 0x00)
	{
		// btst, bset, bchg, bclr (register)
		if (((unsigned char)code[offset + 1] & 0x38) != 0x08)
		{
			mode = 0;
			reg = ((unsigned char)code[offset] & 0x0E) >> 1;
		}

		// movep
		else
		{
			if (((unsigned char)code[offset + 1] & 0x80) == 0x00)
			{
				mode = 5;
				reg = ((unsigned char)code[offset + 1] & 0x07);
			}
			else
			{
				mode = 0;
				reg = ((unsigned char)code[offset] & 0x0E) >> 1;
			}
		}
	}

	// movea, move
	else if (((unsigned char)code[offset] & 0xF0) < 0x40)
	{
		mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
		reg = ((unsigned char)code[offset + 1] & 0x07);
	}

	// move to ccr, move to sr, pea, tst, jmp, jsr, movem, lea, chk
	else if (((unsigned char)code[offset] & 0xF0) == 0x40)
	{
		// move to ccr
		if (((unsigned char)code[offset] & 0x0F) == 0x40 && ((unsigned char)code[offset + 1] & 0xC0) == 0xC0)
		{
			mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
			reg = ((unsigned char)code[offset + 1] & 0x07);
		}
		// move to sr
		else if (((unsigned char)code[offset] & 0x0F) == 0x60 && ((unsigned char)code[offset + 1] & 0xC0) == 0xC0)
		{
			mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
			reg = ((unsigned char)code[offset + 1] & 0x07);
		}
		// pea
		else if (((unsigned char)code[offset] & 0x0F) == 0x08 && ((unsigned char)code[offset + 1] & 0xC0) == 0x40)
		{
			mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
			reg = ((unsigned char)code[offset + 1] & 0x07);
		}
		// tst
		else if (((unsigned char)code[offset] & 0x0F) == 0x0A && ((unsigned char)code[offset + 1] & 0xC0) != 0xC0)
		{
			mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
			reg = ((unsigned char)code[offset + 1] & 0x07);
		}
		// jmp, jsr
		else if (((unsigned char)code[offset] & 0x0F) == 0x0E)
		{
			mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
			reg = ((unsigned char)code[offset + 1] & 0x07);
		}
		// movem
		else if (((unsigned char)code[offset] & 0x0F) == 0x0C)
		{
			mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
			reg = ((unsigned char)code[offset + 1] & 0x07);
		}
		// lea, chk
		else
		{
			mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
			reg = ((unsigned char)code[offset + 1] & 0x07);
		}
	}

	// divu, divs, or
	else if (((unsigned char)code[offset] & 0xF0) == 0x80)
	{
		mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
		reg = ((unsigned char)code[offset + 1] & 0x07);
	}

	// sub, suba
	else if (((unsigned char)code[offset] & 0xF0) == 0x90)
	{
		mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
		reg = ((unsigned char)code[offset + 1] & 0x07);
	}

	// cmp, cmpa
	else if (((unsigned char)code[offset] & 0xF0) == 0xB0)
	{
		mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
		reg = ((unsigned char)code[offset + 1] & 0x07);
	}

	// mulu, muls, and
	else if (((unsigned char)code[offset] & 0xF0) == 0xC0)
	{
		mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
		reg = ((unsigned char)code[offset + 1] & 0x07);
	}

	// add, adda
	else if (((unsigned char)code[offset] & 0xF0) == 0xD0)
	{
		mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
		reg = ((unsigned char)code[offset + 1] & 0x07);
	}

	// mode parsing
	if (mode == 0)
	{
		line += "d";
		line += hexArray[reg];

		data += 0;
	}
	else if (mode == 1)
	{
		line += "a";
		line += hexArray[reg];

		data += 0;
	}
	else if (mode == 2)
	{
		line += "(a";
		line += hexArray[reg];
		line += ")";

		data += 0;
	}
	else if (mode == 3)
	{
		line += "(a";
		line += hexArray[reg];
		line += ")+";

		data += 0;
	}
	else if (mode == 4)
	{
		line += "-(a";
		line += hexArray[reg];
		line += ")";

		data += 0;
	}
	else if (mode == 5)
	{
		line += "$";
		line += parseByte((unsigned char)code[offset + data + 2]);
		line += parseByte((unsigned char)code[offset + data + 3]);

		line += "(a";
		line += hexArray[reg];
		line += ")";

		data += 2;
	}
	else if (mode == 6)
	{
		line += "$";
		line += parseByte((unsigned char)code[offset + data + 3]);

		line += "(a";
		line += hexArray[reg];
		line += ",";
		
		if (((unsigned char)code[offset + data + 2] & 0x80) == 0x00)
		{
			line += "d";
		}
		else
		{
			line += "a";
		}

		line += hexArray[((unsigned char)code[offset + data + 2] & 0x70) >> 4];

		if (((unsigned char)code[offset + data + 2] & 0x08) == 0x00)
		{
			line += ".w";
		}
		else
		{
			line += ".l";
		}

		line += ")";

		data += 2;
	}
	else if (mode == 7)
	{
		// Parse Registers
		if (reg == 2)
		{
			line += "$";

			int displacement = (signed int)(((unsigned char)code[offset + data + 2] * 256) + (unsigned char)code[offset + data + 3]);
			displacement += (offset + data + 2);
			char highByte = displacement / 256;
			char lowByte = displacement - (highByte * 256);

			line += parseByte(highByte);
			line += parseByte(lowByte);

			line += "(pc)";

			data += 2;
		}
		else if (reg == 3)
		{
			line += "$";
			
			int displacement = (signed char)code[offset + data + 3];
			displacement += (offset + data + 2);
			char highByte = displacement / 256;
			char lowByte = displacement - (highByte * 256);

			line += parseByte(highByte);
			line += parseByte(lowByte);

			line += "(pc,";
			
			if (((unsigned char)code[offset + data + 2] & 0x80) == 0x00)
			{
				line += "d";
			}
			else
			{
				line += "a";
			}

			line += hexArray[((unsigned char)code[offset + data + 2] & 0x70) >> 4];

			if (((unsigned char)code[offset + data + 2] & 0x08) == 0x00)
			{
				line += ".w";
			}
			else
			{
				line += ".l";
			}

			line += ")";

			data += 2;
		}
		else if (reg == 0)
		{
			line += "$";
			line += parseByte((unsigned char)code[offset + data + 2]);
			line += parseByte((unsigned char)code[offset + data + 3]);
			line += ".w";

			data += 2;
		}
		else if (reg == 1)
		{
			line += "$";
			line += parseByte((unsigned char)code[offset + data + 2]);
			line += parseByte((unsigned char)code[offset + data + 3]);
			line += parseByte((unsigned char)code[offset + data + 4]);
			line += parseByte((unsigned char)code[offset + data + 5]);
			line += ".l";

			data += 4;
		}
		else if (reg == 4)
		{
			line += "#$";
			
			for (int i = 0; i < size; i++)
			{
				if (size == 1)
				{
					i++;
				}
				line += parseByte((unsigned char)code[offset + data + 2 + i]);
			}

			data += size;

			if (size == 1)
			{
				data++;
			}
		}
		else
		{
			line = "dc.w $";
			for (int i = 0; i < 2; i++)
			{
				line += parseByte(code[offset + i]);
			}

			line += "\t; Invalid Opcode";
#ifndef SSE_BUILD
			line += "\t; offset $";
			char temp = offset / 0x1000000;
			line += parseByte(temp);

			temp = offset / 0x10000;
			temp = temp & 0x00FF;
			line += parseByte(temp);

			temp = offset / 0x100;
			temp = temp & 0x0000FF;
			line += parseByte(temp);

			temp = offset;
			temp = temp & 0x000000FF;
			line += parseByte(temp);

			line += "\n";
#endif
			offset += 2;
			return -1;
		}
	}
	else
	{
		line = "dc.w $";
		for (int i = 0; i < 2; i++)
		{
			line += parseByte(code[offset + i]);
		}

		line += "\t; Invalid Opcode";
#ifndef SSE_BUILD
		line += "\t; offset $";
		char temp = offset / 0x1000000;
		line += parseByte(temp);

		temp = offset / 0x10000;
		temp = temp & 0x00FF;
		line += parseByte(temp);

		temp = offset / 0x100;
		temp = temp & 0x0000FF;
		line += parseByte(temp);

		temp = offset;
		temp = temp & 0x000000FF;
		line += parseByte(temp);

		line += "\n";
#endif
		offset += 2;
		return -1;
	}

	return 0;
}

#ifdef SSE_BUILD
int Disassembler::parseDestOperand(char* code, int& offset, int opcode, int size, EasyStr& line)
#else
int Disassembler::parseDestOperand(char* code, int& offset, int opcode, int size, std::string& line)
#endif
{
	int mode = -1;
	int reg = -1;

	if (data == 1)
	{
		data++;
	}

	// everything before and including movep
	if (((unsigned char)code[offset] & 0xF0) == 0x00)
	{
		if (((unsigned char)code[offset + 1] & 0x38) != 0x08)
		{
			mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
			reg = ((unsigned char)code[offset + 1] & 0x07);
		}
		else
		{
			if (((unsigned char)code[offset + 1] & 0x80) == 0x00)
			{
				mode = 0;
				reg = ((unsigned char)code[offset] & 0x0E) >> 1;
			}
			else
			{
				mode = 5;
				reg = ((unsigned char)code[offset + 1] & 0x07);
			}
		}
	}

	// movea, move
	else if (((unsigned char)code[offset] & 0xF0) < 0x40)
	{
		if (((unsigned char)code[offset] & 0x01) == 0x00 && ((unsigned char)code[offset + 1] & 0xC0) == 0x40)
		{
			mode = 1;
			reg = ((unsigned char)code[offset] & 0x0E) >> 1;
		}
		else
		{
			mode = (((unsigned char)code[offset] & 0x01) << 2) + (((unsigned char)code[offset + 1] & 0xC0) >> 6);
			reg = ((unsigned char)code[offset] & 0x0E) >> 1;
		}
	}

	// move from sr, negx, clr, neg, not, ext, nbcd, swap, tas
	else if (((unsigned char)code[offset] & 0xF0) == 0x40)
	{
		// move from sr
		if (((unsigned char)code[offset] & 0x0F) == 0x00 && ((unsigned char)code[offset + 1] & 0xC0) == 0xC0)
		{
			mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
			reg = ((unsigned char)code[offset + 1] & 0x07);
		}

		// negx, clr, neg, not
		else if (((unsigned char)code[offset] & 0x0F) < 0x08 && ((unsigned char)code[offset + 1] & 0xC0) != 0xC0)
		{
			mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
			reg = ((unsigned char)code[offset + 1] & 0x07);
		}

		// ext, nbcd, swap, movem
		else if (((unsigned char)code[offset] & 0x0F) == 0x08 && ((unsigned char)code[offset] & 0x0F))
		{
			mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
			reg = ((unsigned char)code[offset + 1] & 0x07);
		}

		// tas
		else if (((unsigned char)code[offset] & 0x0F) == 0x0A)
		{
			mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
			reg = ((unsigned char)code[offset + 1] & 0x07);
		}
	}

	// addq, subq, scc
	else if (((unsigned char)code[offset] & 0xF0) == 0x50)
	{
		mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
		reg = ((unsigned char)code[offset + 1] & 0x07);
	}

	// or
	else if (((unsigned char)code[offset] & 0xF0) == 0x80)
	{
		mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
		reg = ((unsigned char)code[offset + 1] & 0x07);
	}

	// sub
	else if (((unsigned char)code[offset] & 0xF0) == 0x90)
	{
		mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
		reg = ((unsigned char)code[offset + 1] & 0x07);
	}

	// eor
	else if (((unsigned char)code[offset] & 0xF0) == 0xB0)
	{
		mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
		reg = ((unsigned char)code[offset + 1] & 0x07);
	}

	// and
	else if (((unsigned char)code[offset] & 0xF0) == 0xC0)
	{
		mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
		reg = ((unsigned char)code[offset + 1] & 0x07);
	}

	// add
	else if (((unsigned char)code[offset] & 0xF0) == 0xD0)
	{
		mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
		reg = ((unsigned char)code[offset + 1] & 0x07);
	}

	// asd, lsd, roxd, rod (memory)
	else if (((unsigned char)code[offset] & 0xF0) == 0xE0)
	{
		mode = ((unsigned char)code[offset + 1] & 0x38) >> 3;
		reg = ((unsigned char)code[offset + 1] & 0x07);
	}

	// mode parsing
	if (mode == 0)
	{
		line += "d";
		line += hexArray[reg];

		data += 0;
	}
	else if (mode == 1)
	{
		line += "a";
		line += hexArray[reg];

		data += 0;
	}
	else if (mode == 2)
	{
		line += "(a";
		line += hexArray[reg];
		line += ")";

		data += 0;
	}
	else if (mode == 3)
	{
		line += "(a";
		line += hexArray[reg];
		line += ")+";

		data += 0;
	}
	else if (mode == 4)
	{
		line += "-(a";
		line += hexArray[reg];
		line += ")";

		data += 0;
	}
	else if (mode == 5)
	{
		line += "$";
		line += parseByte((unsigned char)code[offset + data + 2]);
		line += parseByte((unsigned char)code[offset + data + 3]);

		line += "(a";
		line += hexArray[reg];
		line += ")";

		data += 2;
	}
	else if (mode == 6)
	{
		line += "$";
		line += parseByte((unsigned char)code[offset + data + 3]);

		line += "(a";
		line += hexArray[reg];
		line += ",";

		if (((unsigned char)code[offset + data + 2] & 0x80) == 0x00)
		{
			line += "d";
		}
		else
		{
			line += "a";
		}

		line += hexArray[((unsigned char)code[offset + data + 2] & 0x70) >> 4];

		if (((unsigned char)code[offset + data + 2] & 0x08) == 0x00)
		{
			line += ".w";
		}
		else
		{
			line += ".l";
		}

		line += ")";

		data += 2;
	}
	else if (mode == 7)
	{
		// Parse Registers
		if (reg == 2)
		{
			line += "$";

			int displacement = (signed int)(((unsigned char)code[offset + data + 2] * 256) + (unsigned char)code[offset + data + 3]);
			displacement += (offset + data + 2);
			char highByte = displacement / 256;
			char lowByte = displacement - (highByte * 256);

			line += parseByte(highByte);
			line += parseByte(lowByte);

			line += "(pc)";

			data += 2;
		}
		else if (reg == 3)
		{
			line += "$";

			int displacement = (signed char)code[offset + data + 3];
			displacement += (offset + data + 2);
			char highByte = displacement / 256;
			char lowByte = displacement - (highByte * 256);

			line += parseByte(highByte);
			line += parseByte(lowByte);

			line += "(pc,";

			if (((unsigned char)code[offset + data + 2] & 0x80) == 0x00)
			{
				line += "d";
			}
			else
			{
				line += "a";
			}

			line += hexArray[((unsigned char)code[offset + data + 2] & 0x70) >> 4];

			if (((unsigned char)code[offset + data + 2] & 0x08) == 0x00)
			{
				line += ".w";
			}
			else
			{
				line += ".l";
			}

			line += ")";

			data += 2;
		}
		else if (reg == 0)
		{
			line += "$";
			line += parseByte((unsigned char)code[offset + data + 2]);
			line += parseByte((unsigned char)code[offset + data + 3]);
			line += ".w";

			data += 2;
		}
		else if (reg == 1)
		{
			line += "$";
			line += parseByte((unsigned char)code[offset + data + 2]);
			line += parseByte((unsigned char)code[offset + data + 3]);
			line += parseByte((unsigned char)code[offset + data + 4]);
			line += parseByte((unsigned char)code[offset + data + 5]);
			line += ".l";

			data += 4;
		}
		else if (reg == 4)
		{
			line += "#$";

			for (int i = 0; i < size; i++)
			{
				if (size == 1)
				{
					i++;
				}
				line += parseByte((unsigned char)code[offset + data + 2 + i]);
			}

			data += size;
			if (size == 1)
			{
				data++;
			}
		}
		else
		{
			line = "dc.w $";
			for (int i = 0; i < 2; i++)
			{
				line += parseByte(code[offset + i]);
			}

			line += "\t; Invalid Opcode";
#ifndef SSE_BUILD
			line += "\t; offset $";
			char temp = offset / 0x1000000;
			line += parseByte(temp);

			temp = offset / 0x10000;
			temp = temp & 0x00FF;
			line += parseByte(temp);

			temp = offset / 0x100;
			temp = temp & 0x0000FF;
			line += parseByte(temp);

			temp = offset;
			temp = temp & 0x000000FF;
			line += parseByte(temp);

			line += "\n";
#endif
			offset += 2;
			return -1;
		}
	}
	else
	{
		line = "dc.w $";
		for (int i = 0; i < 2; i++)
		{
			line += parseByte(code[offset + i]);
		}

		line += "\t; Invalid Opcode";
#ifndef SSE_BUILD
		line += "\t; offset $";
		char temp = offset / 0x1000000;
		line += parseByte(temp);

		temp = offset / 0x10000;
		temp = temp & 0x00FF;
		line += parseByte(temp);

		temp = offset / 0x100;
		temp = temp & 0x0000FF;
		line += parseByte(temp);

		temp = offset;
		temp = temp & 0x000000FF;
		line += parseByte(temp);

		line += "\n";
#endif
		offset += 2;
		return -1;
	}

	return 0;
}

#ifdef SSE_BUILD
EasyStr Disassembler::parseByte(char byte)
#else
std::string Disassembler::parseByte(char byte)
#endif
{
#ifdef SSE_BUILD
	EasyStr asciiByte = "";
#else
	std::string asciiByte = "";
#endif
	asciiByte += hexArray[(byte & 0xF0) >> 4];
	asciiByte += hexArray[(byte & 0x0F) >> 0];

	return asciiByte;
}

#endif//#ifdef SSE_BETA
