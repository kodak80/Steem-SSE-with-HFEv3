#pragma once
#ifdef SSE_BUILD
#include <easystr.h>
#else
#include <string>
#endif

class Disassembler
{
public:
	
	static char hexArray[];
#ifdef SSE_BUILD
  static char* Disassembler::condArray[];
	static EasyStr parseInstruction(char* code, int& offset);
	static int parseSize(char* code, int& offset, int opcode, EasyStr& line);
	static int parseSourceOperand(char* code, int& offset, int opcode, int size, EasyStr& line);
	static int parseDestOperand(char* code, int& offset, int opcode, int size, EasyStr& line);

	static EasyStr parseByte(char byte);
#else
	static std::string condArray[];
	static std::string parseInstruction(char* code, int& offset);
	static int parseSize(char* code, int& offset, int opcode, std::string& line);
	static int parseSourceOperand(char* code, int& offset, int opcode, int size, std::string& line);
	static int parseDestOperand(char* code, int& offset, int opcode, int size, std::string& line);

	static std::string parseByte(char byte);
#endif
	
};

