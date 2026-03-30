#pragma once

#include <windows.h>
#include <conio.h>
#include <vector>
#include <Lmcons.h>
#include <cwchar>
#include <array>
#include <ctime>

#include "GuardAllocator.hpp"

const wchar_t characters[] = L"!\"#$%&\'()*+,-./:;<=>?@[\\]^_{|}~`0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

constexpr size_t RUNIC_ALPH = 88;
constexpr size_t ASCII_ALPH = (sizeof(characters) / sizeof(wchar_t)) - 1;

constexpr std::array<wchar_t, RUNIC_ALPH> init_runic()
{
	std::array<wchar_t, RUNIC_ALPH> data = {};
	for (size_t i = 0; i < RUNIC_ALPH; i++)
	{
		data[i] = static_cast<wchar_t>(i+ 0x16a0);
	}
	return data;
}

constexpr std::array<wchar_t, RUNIC_ALPH> runic = init_runic();

class Console
{
private:

	HANDLE hConsole, hInput;
	std::wstring user_name;

	DWORD read_textbox(std::vector<wchar_t>& buffer)
	{
		DWORD character_read = 0;
		BOOL success = ReadConsoleW(
			hInput,              
			buffer.data(),        
			buffer.capacity(),   
			&character_read,
			NULL);

		if (success && character_read == 0)
		{
			throw std::runtime_error("listen_textbox: success && charactersRead == 0");
		}
		
		buffer[character_read - 1] = L'\0';
		buffer[character_read - 2] = L'\0';

		return character_read;
		
	}

	DWORD read_password(std::vector<wchar_t, GuardAllocator<wchar_t>>& buffer)
	{
		DWORD mode;
		GetConsoleMode(hInput, &mode);
		DWORD new_mode = mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
		SetConsoleMode(hInput, new_mode);

		DWORD pos = 0;
		bool finished = false;

		while (!finished) 
		{
			DWORD count;
			INPUT_RECORD record;
			ReadConsoleInputW(hInput, &record, 1, &count);

			if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
			{
				wchar_t ch = record.Event.KeyEvent.uChar.UnicodeChar;
				WORD vk = record.Event.KeyEvent.wVirtualKeyCode;

				if (vk == VK_RETURN) 
				{
					finished = true;
					WriteConsoleW(hConsole, L"\n", 1, NULL, NULL);
				}
				else if (vk == VK_BACK && pos > 0)
				{
					pos--;
					{
						Bottleneck lock(buffer);
						buffer[pos] = 0;
					}
					WriteConsoleW(hConsole, L"\b \b", 3, NULL, NULL);
				}
				else if (ch >= 32 && pos < buffer.capacity() - 1)
				{ 
					{
						Bottleneck lock(buffer);
						buffer.push_back(ch);
					}
					pos++;
					WriteConsoleW(hConsole, &runic[rand()%RUNIC_ALPH], 1, NULL, NULL);
				}
			}
		}

		{
			Bottleneck lock(buffer);
			buffer.resize(pos);
		}

		SetConsoleMode(hInput, mode);
		return pos;
	}

	void clear_input_buffer() 
	{
		wchar_t c;
		while (_kbhit()) { c = _getwch(); }
	}

	void listen_menu(uint32_t& option, std::vector<HANDLE>& buffers)
	{
		clear_input_buffer();
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

	COORD get_current_cursor_pos()
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi;

		if (GetConsoleScreenBufferInfo(hConsole, &csbi))
		{
			return { csbi.dwCursorPosition.X, csbi.dwCursorPosition.Y };
		}
		else return { 0, 0 };
		
	}

public:
	static enum CODES {OK=200, ERR=403};
	static enum Keybord { ENTER = 13, BACKSPACE = 8, UP = 72, DOWN = 80, LEFT=75, RIGHT=77 };

	Console()
	{
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		hInput = GetStdHandle(STD_INPUT_HANDLE);
		srand(time(NULL));
	}

	void wait_key()
	{
		wchar_t ch = _getwch();
	}

	void clean_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
	{
		std::vector<CHAR_INFO> buffer(w * h);
		for (size_t i = 0; i < h; i++)
		{
			for (size_t j = 0; j < w; j++)
			{
				size_t ind = i * w + j;
				buffer[ind].Char.UnicodeChar = L' ';
			}
		}

		COORD bufferSize = { (WORD)w, (WORD)h };
		COORD bufferCoord = { 0, 0 };
		SMALL_RECT writeRegion = { (WORD)x, (WORD)y, (WORD)(x + w - 1), (WORD)(y + h - 1) };

		WriteConsoleOutputW(hConsole, buffer.data(), bufferSize, bufferCoord, &writeRegion);
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
	template <typename TAlloc>
	DWORD interact(const wchar_t* message, 
		uint32_t x,
		uint32_t y, 
		std::vector<wchar_t, TAlloc>& pass_buffer)
	{
		size_t w = std::wcslen(message) + 6;
		size_t h = 2;

		clean_rect(x, y, 128, h);
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

		if constexpr (std::is_same_v<TAlloc, GuardAllocator<wchar_t>>)
		{
			return read_password(pass_buffer);
		}
		else
		{
			return read_textbox(pass_buffer);
		}
	}

	/*
	┌─────────┐
	│ message │
	└─────────┘
	*/
	void message_box(const wchar_t* message, 
		uint32_t x,
		uint32_t y,
		uint32_t code,
		HANDLE hBuffer)
	{
		if (hBuffer == NULL) hBuffer = hConsole;
	

		size_t w = std::wcslen(message) + 2;
		size_t h = 3;
		clean_rect(x, y, 128, h);
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
			if (code == Console::OK)
			{
				buffer[ind].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY ;
			}
			else if (code == Console::ERR)
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

	uint32_t menu(std::vector<std::wstring> options)
	{
		clean();
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
		SetConsoleActiveScreenBuffer(buffers[option]);
		listen_menu(option, buffers);

		SetConsoleActiveScreenBuffer(hConsole);

		for (size_t i = 0; i < options.size(); i++)
		{
			CloseHandle(buffers[i]);
		}

		return option;
	}

	void label(const wchar_t* name, const wchar_t* text, uint32_t x, uint32_t y)
	{
		size_t name_len = std::wcslen(name);
		size_t text_len = std::wcslen(text);
		size_t w = name_len + text_len + 2;
		size_t h = 1;

		clean_rect(x, y, 128, h);
		std::vector<CHAR_INFO> hBuffer(w);

		for (size_t j = 0; j < name_len; j++)
		{
			hBuffer[j].Char.UnicodeChar = name[j];
		}
		hBuffer[name_len].Char.UnicodeChar = L':';
		hBuffer[name_len + 1].Char.UnicodeChar = L' ';

		for (size_t i = 0; i < text_len; i++)
		{
			hBuffer[name_len+2+i].Char.UnicodeChar = text[i];
		}

		for (size_t i = 0; i < w; i++)
		{
			hBuffer[i].Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
		}

		COORD bufferSize = { (WORD)w, (WORD)h };
		COORD bufferCoord = { 0, 0 };
		SMALL_RECT writeRegion = { (WORD)x, (WORD)y, (WORD)(x + w - 1), (WORD)(y + h - 1) };

		WriteConsoleOutputW(hConsole, hBuffer.data(), bufferSize, bufferCoord, &writeRegion);
	}
};