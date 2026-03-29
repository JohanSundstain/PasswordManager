#pragma once

#include <windows.h>
#include <conio.h>
#include <vector>
#include <Lmcons.h>

#include "GuardAllocator.hpp"

enum Colors {BLACK=0, BLUE=1, GREEN=3, RED=4, WHITE_GRAY=7, UP=72, DOWN=80};
enum Keybord {ENTER=13, BACKSPACE=8};

class Console
{
private:

	HANDLE hConsole;
	std::wstring user_name;

	void listen_textbox(std::vector<wchar_t, GuardAllocator<wchar_t>>& buffer)
	{
		wchar_t pushed_key = -1;
		size_t free_place = 0;

		while (pushed_key != ENTER)
		{
			if (_kbhit())
			{
				pushed_key = _getwch();

				if (pushed_key == 224 || pushed_key == 0)
				{
					pushed_key = _getwch();
					continue;
				}

				switch (pushed_key)
				{
				case BACKSPACE: if (free_place > 0) buffer[free_place - 1] = 0, free_place--; break;
				default:
				{
					if (pushed_key >= 32)
					{
						buffer.get_allocator().unlock(buffer.data(), buffer.size());
						buffer.push_back(pushed_key);
						buffer.get_allocator().lock(buffer.data(), buffer.size());
						free_place++;
					}
				}	break;
				}
			}
		}

	}

	void setColor(int textColor, int bgColor = 0)
	{
		SetConsoleTextAttribute(hConsole, (bgColor << 4) | textColor);
	}

	void listen_menu(uint32_t& option, std::vector<HANDLE>& buffers)
	{
		wchar_t pushed_key = -1;

		while (pushed_key != ENTER)
		{
			if (_kbhit())
			{
				pushed_key = _getwch();

				if (pushed_key == 224 || pushed_key == 0)
				{
					pushed_key = _getwch();

					switch (pushed_key)
					{
						case UP:
						{
							option = option == 0 ? buffers.size()-1 : option - 1;
							SetConsoleActiveScreenBuffer(buffers[option]);
							break;
						} 
						case DOWN:
						{
							option = option == buffers.size() - 1 ? 0 : option + 1;
							SetConsoleActiveScreenBuffer(buffers[option]);
							break;
						}
						default: break;
					}

				}
			}
		}
	}

public:

	Console()
	{
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	}

	void wait_key()
	{
		wchar_t ch = _getwch();
	}

	void clean()
	{
		COORD coord = { 0, 0 };
		DWORD count;
		CONSOLE_SCREEN_BUFFER_INFO csbi;

		if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
			DWORD consoleSize = csbi.dwSize.X * csbi.dwSize.Y;

			FillConsoleOutputCharacter(hConsole, (TCHAR)' ', consoleSize, coord, &count);

			FillConsoleOutputAttribute(hConsole, csbi.wAttributes, consoleSize, coord, &count);

			SetConsoleCursorPosition(hConsole, coord);
		}
	}


	/*
	┌──(message):
	└-$
	*/
	void interact(const char* message, 
		uint32_t x,
		uint32_t y, 
		std::vector<wchar_t, 
		GuardAllocator<wchar_t>>& pass_buffer)
	{
		size_t w = strlen(message) + 6;
		size_t h = 2;
		std::vector<CHAR_INFO> buffer(w * h);

		buffer[0].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		buffer[0].Char.UnicodeChar = L'┌';

		buffer[1].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		buffer[1].Char.UnicodeChar = L'─';

		buffer[2].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		buffer[2].Char.UnicodeChar = L'─';
		
		buffer[3].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		buffer[3].Char.UnicodeChar = L'(';

		for (size_t i = 0; i < w - 6; i++)
		{
			buffer[i + 4].Attributes = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
			buffer[i + 4].Char.UnicodeChar = message[i];
		}

		buffer[w - 2].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		buffer[w - 2].Char.UnicodeChar = L')';

		buffer[w - 1].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		buffer[w - 1].Char.UnicodeChar = L':';

		buffer[w].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		buffer[w].Char.UnicodeChar = L'└';

		buffer[w + 1].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		buffer[w + 1].Char.UnicodeChar = L'─';
	
		buffer[w + 2].Attributes = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
		buffer[w + 2].Char.UnicodeChar = L'$';


		COORD bufferSize = { (WORD)w, (WORD)h };
		COORD bufferCoord = { 0, 0 };
		SMALL_RECT writeRegion = { (WORD)x, (WORD)y, (WORD)(x + w - 1), (WORD)(y + h - 1) };

		WriteConsoleOutputW(hConsole, buffer.data(), bufferSize, bufferCoord, &writeRegion);

		COORD pos = { (WORD)x+3, (WORD)y+1 };
		SetConsoleCursorPosition(hConsole, pos);

		listen_textbox(pass_buffer);
	}

	/*
	┌─────────┐
	│ message │
	└─────────┘
	*/
	void message_box(const char* message, 
		uint32_t x,
		uint32_t y,
		uint32_t code,
		HANDLE hBuffer)
	{
		if (hBuffer == NULL) hBuffer = hConsole;
	

		size_t w = strlen(message) + 2;
		size_t h = 3;
		std::vector<CHAR_INFO> buffer(w*h);

		// UP Border

		for (size_t i = 0; i < h; i++)
		{
			for (size_t j = 0; j < w; j++)
			{
				size_t ind = i * w + j;
				buffer[ind].Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

				if (i == 0 && j == 0)				buffer[ind].Char.UnicodeChar = L'┌';
				else if (i == 0 && j == (w - 1))	buffer[ind].Char.UnicodeChar = L'┐';
				else if (i == (h-1) && j == 0)		buffer[ind].Char.UnicodeChar = L'└';
				else if (i == (h - 1) && j == (w-1))buffer[ind].Char.UnicodeChar = L'┘';

				else if (j == 0 || j == (w-1))      buffer[ind].Char.UnicodeChar = L'│';
				else if (i == 0 || i == (h-1))      buffer[ind].Char.UnicodeChar = L'─';	
				else								buffer[ind].Char.UnicodeChar = L' ';

			}
		}

		for (size_t j = 0, i = 1; j < w - 2; j++)
		{
			size_t ind = i * w + (j + 1);
			buffer[ind].Char.UnicodeChar = message[j];
			if (code == 200)
			{
				buffer[ind].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY ;
			}
			else if (code == 403)
			{
				buffer[ind].Attributes = FOREGROUND_RED | FOREGROUND_INTENSITY;
			}
			else
			{
				buffer[ind].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY | BACKGROUND_BLUE | BACKGROUND_RED;
			}
		}

		COORD bufferSize = { (WORD)w, (WORD)h };
		COORD bufferCoord = { 0, 0 };
		SMALL_RECT writeRegion = { (WORD)x, (WORD)y, (WORD)(x + w - 1), (WORD)(y + h - 1) };

		WriteConsoleOutputW(hBuffer, buffer.data(), bufferSize, bufferCoord, &writeRegion);

	}


	uint32_t menu(std::vector<std::string> options)
	{
		std::vector<HANDLE> buffers;

		for (size_t i = 0; i < options.size(); i++)
		{
			HANDLE hBuffer = CreateConsoleScreenBuffer(
				GENERIC_READ | GENERIC_WRITE,
				0,                          
				NULL,
				CONSOLE_TEXTMODE_BUFFER,
				NULL);
			if (hBuffer == INVALID_HANDLE_VALUE) 
			{
				throw std::runtime_error("menu: hBuffer == INVALID_HANDLE_VALUE");
			}

			buffers.push_back(hBuffer);

			SetConsoleActiveScreenBuffer(buffers[i]);

			for (size_t j = 0; j < options.size(); j++)
			{
				if (j == i)
				{
					message_box(options[j].c_str(), 0, (j * 3), 0, buffers[i]);
				}
				else
				{
					message_box(options[j].c_str(), 0, (j * 3), 200, buffers[i]);
				}
			}
		}

		uint32_t option = 0;

		listen_menu(option, buffers);

		SetConsoleActiveScreenBuffer(hConsole);

		for (size_t i = 0; i < options.size(); i++)
		{
			CloseHandle(buffers[i]);
		}

		return 0;
	}
};