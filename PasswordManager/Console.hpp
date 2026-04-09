#pragma once

#include <windows.h>
#include <conio.h>
#include <vector>
#include <cwchar>
#include <array>
#include <chrono>
#include <thread>

#include "GuardAllocator.hpp"
#include "Presumer.hpp"


constexpr size_t BAR_SIZE = 32;
constexpr size_t RUNIC_ALPH = 88;
const wchar_t LOGO[] = L"⛨ PASS";


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

using namespace std::chrono_literals;

class Console
{
private:

	HANDLE hConsole, hInput;
	std::unique_ptr<Presumer> presumer;
	std::wstring user_name;

	void read_textbox(std::vector<wchar_t>& buffer)
	{
		INPUT_RECORD record;
		DWORD count = 0, written = 0;

		while (true)
		{
			ReadConsoleInput(hInput, &record, 1, &count);

			if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
			{
				WORD vk = record.Event.KeyEvent.wVirtualKeyCode;
				wchar_t ch = record.Event.KeyEvent.uChar.UnicodeChar;

				if (vk == VK_RETURN)
				{
					break;
				}

				if (vk == VK_BACK)
				{
					if (buffer.size() > 0)
					{
						WriteConsoleW(hConsole, L"\b \b", 3, NULL, NULL);
						buffer.pop_back();
					}
					auto_hint(buffer, 0, 2, 5);
				}
				else if (vk == VK_TAB && buffer.size() > 0)
				{
					size_t old = buffer.size();
					presumer->assume(buffer);
					if (old != buffer.size())
					{
						if (!WriteConsoleW(
							hConsole,
							&buffer.data()[old],
							static_cast<DWORD>(buffer.size() - old),
							&written,
							NULL))
						{
							throw std::runtime_error("read_textbox: !WriteConsoleW(...)");
						}
					}
				}
				else if (ch >= 32 && buffer.size() < USER_INFO_LIMIT - 1)
				{
					buffer.push_back(ch);
					WriteConsoleW(hConsole, &ch, 1, NULL, NULL);
					auto_hint(buffer, 0, 2, 5);
				}
	
			}
		}
	
		if (buffer.size() > 0) buffer.push_back(L'\0');
	}


	void read_password(SafeVector<wchar_t>& buffer) const
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
				}
				else if (vk == VK_BACK && pos > 0)
				{
					pos--;
					{
						Bottleneck lock(buffer);
						buffer[pos] = 0;
						buffer.pop_back();
					}
					WriteConsoleW(hConsole, L"\b \b", 3, NULL, NULL);
				}
				else if (ch >= WHITESPACE && pos < PASSWORD_MAX)
				{
					{
						Bottleneck lock(buffer);
						buffer.push_back(ch);
					}
					pos++;
					WriteConsoleW(hConsole, &runic[rand() % RUNIC_ALPH], 1, NULL, NULL);
				}
			}
		}

		{
			Bottleneck lock(buffer);
			buffer.resize(pos);
		}

		SetConsoleMode(hInput, mode);
	}

	void enable_cursor(HANDLE handler)
	{
		CONSOLE_CURSOR_INFO cci;
		GetConsoleCursorInfo(handler, &cci);
		cci.bVisible = true;
		SetConsoleCursorInfo(handler, &cci);
	}

	void disable_cursor(HANDLE handler)
	{
		CONSOLE_CURSOR_INFO cci;
		GetConsoleCursorInfo(handler, &cci);
		cci.bVisible = false;
		SetConsoleCursorInfo(handler, &cci);
	}

	void listen_menu(uint32_t& option, std::vector<HANDLE>& buffers) const 
	{
		for (auto hCon : buffers)
		{ 
			FlushConsoleInputBuffer(hCon);
		}

		INPUT_RECORD record;
		DWORD count;

		while (true)
		{
			ReadConsoleInput(hInput, &record, 1, &count);

			if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
			{
				switch (record.Event.KeyEvent.wVirtualKeyCode)
				{
				case VK_LEFT:
					option = option == 0 ? static_cast<uint32_t>(buffers.size() - 1) : option - 1;
					SetConsoleActiveScreenBuffer(buffers[option]);
					break;

				case VK_RIGHT:
					option = option == static_cast<uint32_t>(buffers.size() - 1) ? 0 : option + 1;
					SetConsoleActiveScreenBuffer(buffers[option]);
					break;

				case VK_RETURN:
					return;
				}
			}
		}
	}

	/*
	 ⮞ word
	*/
	void auto_hint(std::vector<wchar_t>& buffer, uint32_t x, uint32_t y, uint32_t auto_hint_size)
	{
		std::vector<std::wstring> result;
		presumer->get_matchings(buffer, result);

		size_t w = USER_INFO_LIMIT + 3;
		size_t h = result.size() < auto_hint_size ? result.size() : auto_hint_size;;
		clean_rect(x, y, static_cast<uint32_t>(w), auto_hint_size);
		std::vector<CHAR_INFO> screen_buffer(w * h);

		
		for (size_t i = 0; i < h; i++)
		{
			for (size_t j = 0; j < w; j++)
			{
				size_t ind = i * w + j;

				if (j == 0)
				{
					screen_buffer[ind].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
					screen_buffer[ind].Char.UnicodeChar = L' ';
				}
				else if (j == 1)
				{
					screen_buffer[ind].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
					screen_buffer[ind].Char.UnicodeChar = L'⮞';
				}
				else if (j == 2)
				{
					screen_buffer[ind].Attributes = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
					screen_buffer[ind].Char.UnicodeChar = L' ';
				}
				else
				{
					for (size_t l = 0; l < result[i].size(); l++)
					{
						screen_buffer[ind + l].Attributes = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
						screen_buffer[ind + l].Char.UnicodeChar = result[i][l];
					}
					break;
				}

			}
		}

		COORD bufferSize = { static_cast<SHORT>(w), static_cast<SHORT>(h) };
		COORD bufferCoord = { 0, 0 };
		SMALL_RECT writeRegion = {
			static_cast<SHORT>(x),
			static_cast<SHORT>(y),
			static_cast<SHORT>(x + w - 1),
			static_cast<SHORT>(y + h - 1)
		};

		WriteConsoleOutputW(hConsole, screen_buffer.data(), bufferSize, bufferCoord, &writeRegion);

	}

	void draw_part(const wchar_t* str, std::vector<CHAR_INFO>& buffer, size_t& ind, WORD attribute)
	{
		for (size_t i = 0; i < std::wcslen(str); i++, ind++)
		{
			buffer[ind].Attributes = attribute;
			buffer[ind].Char.UnicodeChar = str[i];
		}
	}

public:
	enum CODES {OK=200, ERR=403};
	enum Keybord {
		ENTER = 0x0d,
		BACKSPACE = 0x08,
		TAB = 0x09,
		WHITESPACE = 0x20,
		UP = 72, 
		DOWN = 80,
		LEFT = 75,
		RIGHT = 77 };

	Console()
	{
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		hInput = GetStdHandle(STD_INPUT_HANDLE);
		presumer = std::make_unique<Presumer>();
		disable_cursor(hConsole);
	}

	~Console()
	{
		enable_cursor(hConsole);
	}

	void update_presumer(std::array<wchar_t, USER_INFO_LIMIT>& data)
	{
		presumer->add(data);
	}

	void wait_key()
	{
		wchar_t ch = _getwch();
	}

	void wait_progress_bar(
		const wchar_t* message,
		size_t bar_size,
		std::chrono::milliseconds ms,
		uint32_t x, uint32_t y)
	{
		wchar_t bar_piece = L'━';
		DWORD written = 0;
		COORD start_coord = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
		std::array<WORD, 6> colors =
		{
			FOREGROUND_RED,
			FOREGROUND_RED | FOREGROUND_INTENSITY,
			FOREGROUND_RED | FOREGROUND_GREEN,
			FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
			FOREGROUND_GREEN,
			FOREGROUND_GREEN | FOREGROUND_INTENSITY,
		};

		disable_cursor(hConsole);

		size_t length = std::wcslen(message);
		if (length % 2 != bar_size % 2)
		{
			bar_size += 1;
		}

		size_t text_start = (bar_size - length) / 2;
		size_t step = (bar_size + colors.size() - 1) / colors.size();

		auto tick = ms / bar_size;
		COORD text_place = { start_coord.X + static_cast<SHORT>(text_start) , start_coord.Y};
		SetConsoleCursorPosition(hConsole, text_place);
		WriteConsoleW(hConsole, message, static_cast<DWORD>(length), &written, NULL);
		SetConsoleCursorPosition(hConsole, start_coord);

		for (size_t i = 0; i < bar_size; i++)
		{
			SetConsoleCursorPosition(hConsole, start_coord);
			FillConsoleOutputAttribute(hConsole, colors[i / step], static_cast<DWORD>(bar_size), start_coord, &written);

			if (i < text_start - 1 || i > text_start + length)
			{
				COORD current_coord = { start_coord.X + static_cast<SHORT>(i), start_coord.Y };
				SetConsoleCursorPosition(hConsole, current_coord);
				WriteConsoleW(hConsole, &bar_piece, 1, &written, NULL);
			}

			std::this_thread::sleep_for(tick);
		}

		enable_cursor(hConsole);
		SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	}

	void clean_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) const 
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

		COORD bufferSize = { (SHORT)w, (SHORT)h };
		COORD bufferCoord = { 0, 0 };
		SMALL_RECT writeRegion = { (SHORT)x, (SHORT)y, (SHORT)(x + w - 1), (SHORT)(y + h - 1) };

		WriteConsoleOutputW(hConsole, buffer.data(), bufferSize, bufferCoord, &writeRegion);
	}

	void clean() const 
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
	┌──[⛨PASS]~(message):
	└─$
	*/
	template <typename TAlloc>
	void interact(const wchar_t* message,
		uint32_t x,
		uint32_t y,
		std::vector<wchar_t, TAlloc>& pass_buffer)
	{
		enable_cursor(hConsole);

		const wchar_t left[] = L"┌──[";
		const wchar_t middle[] = L"]~(";
		const wchar_t right[] = L"):";
		const wchar_t bottom[] = L"└─";
		const wchar_t sign[] = L"$";


		size_t w =
			std::wcslen(message)
			+ std::wcslen(left)
			+ std::wcslen(LOGO)
			+ std::wcslen(middle)
			+ std::wcslen(right);
		size_t h = 2;

		std::vector<CHAR_INFO> buffer(w * h);
		clean_rect(x, y, PASSWORD_MAX, static_cast<uint32_t>(h));

		size_t ind = 0;
		draw_part(left, buffer, ind, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		draw_part(LOGO, buffer, ind, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		draw_part(middle, buffer, ind, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		draw_part(message, buffer, ind, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY);
		draw_part(right, buffer, ind, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		draw_part(bottom, buffer, ind, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		draw_part(sign, buffer, ind, FOREGROUND_BLUE | FOREGROUND_INTENSITY);

		COORD bufferSize = { (SHORT)w, (SHORT)h };
		COORD bufferCoord = { 0, 0 };
		SMALL_RECT writeRegion = { (SHORT)x, (SHORT)y, (SHORT)(x + w - 1), (SHORT)(y + h - 1) };

		WriteConsoleOutputW(hConsole, buffer.data(), bufferSize, bufferCoord, &writeRegion);

		COORD pos = { (SHORT)x+3, (SHORT)y+1 };
		SetConsoleCursorPosition(hConsole, pos);

		if constexpr (std::is_same_v<TAlloc, GuardAllocator<wchar_t>>)
		{
			read_password(pass_buffer);
		}
		else
		{
			read_textbox(pass_buffer);
		}

		disable_cursor(hConsole);
	}

	/*
	┌─────────┐
	│ message │
	└─────────┘
	*/
	void message_box(const wchar_t* message, 
		uint32_t x, uint32_t y,
		uint32_t code,
		HANDLE hBuffer)
	{
		if (hBuffer == NULL) hBuffer = hConsole;
	
		disable_cursor(hBuffer);
		size_t w = std::wcslen(message) + 2;
		size_t h = 3;
		clean_rect(x, y, 128, static_cast<uint32_t>(h));
		std::vector<CHAR_INFO> buffer(w*h);

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
				buffer[ind].Attributes = BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_RED | FOREGROUND_INTENSITY ;
			}
		}

		COORD bufferSize = { (SHORT)w, (SHORT)h };
		COORD bufferCoord = { 0, 0 };
		SMALL_RECT writeRegion = { (SHORT)x, (SHORT)y, (SHORT)(x + w - 1), (SHORT)(y + h - 1) };

		WriteConsoleOutputW(hBuffer, buffer.data(), bufferSize, bufferCoord, &writeRegion);
		enable_cursor(hBuffer);

	}


	/*
	* ┌─[⛨PASS]~(option1)─(option2)─(...):
	  └─$(current option1)
	   ┌─[⛨PASS]~(option1)─(option2)─(...):[⛨PA
	   └───────────────────────────────
	*/
	uint32_t menu(std::vector<std::wstring>& options)
	{
		clean();
		std::vector<HANDLE> buffers;

		const wchar_t left[] = L"┌──[";
		const wchar_t left_op[] = L"]~(";
		const wchar_t mid_op[] = L")─(";
		const wchar_t right_op[] = L"):";
		const wchar_t bottom[] = L"└─";
		const wchar_t sign[] = L"$";

		size_t w =
			std::wcslen(left)
			+ std::wcslen(LOGO)
			+ std::wcslen(left_op)
			+ (std::wcslen(mid_op) * options.size() - 1); // 1 - 2 - 3  - черточек на 1 меньше чем содержимого

		for (auto str : options)
		{
			w += str.size();
		}
		size_t h = 2;

		for (size_t op = 0; op < options.size(); op++)
		{
			std::vector<CHAR_INFO> screen_buffer(w * h);

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
			
			size_t ind = 0;
			draw_part(left, screen_buffer, ind, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
			draw_part(LOGO, screen_buffer, ind, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
			draw_part(left_op, screen_buffer, ind, FOREGROUND_GREEN | FOREGROUND_INTENSITY);

			for (size_t j = 0; j < options.size(); j++)
			{

				if (j == op)
				{
					draw_part(options[j].c_str(), screen_buffer, ind, FOREGROUND_RED | FOREGROUND_INTENSITY);
				}
				else
				{
					draw_part(options[j].c_str(), screen_buffer, ind, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);

				}


				if (j == options.size() - 1)
				{
					draw_part(right_op, screen_buffer, ind, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
				}
				else
				{
					draw_part(mid_op, screen_buffer, ind, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
				}
				
			}

			draw_part(bottom, screen_buffer, ind, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
			draw_part(sign, screen_buffer, ind, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
			draw_part(options[op].c_str(), screen_buffer, ind, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);

			SetConsoleActiveScreenBuffer(hBuffer);
			CONSOLE_CURSOR_INFO cci;
			GetConsoleCursorInfo(hBuffer, &cci);
			cci.bVisible = FALSE;
			SetConsoleCursorInfo(hBuffer, &cci);

			COORD bufferSize = { (SHORT)w, (SHORT)h };
			COORD bufferCoord = { 0, 0 };
			SMALL_RECT writeRegion = { (SHORT)0, (SHORT)0, (SHORT)(w - 1), (SHORT)(h - 1) };
			WriteConsoleOutputW(hBuffer, screen_buffer.data(), bufferSize, bufferCoord, &writeRegion);
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


	/*
	* ┌[⛨PASS]~login: login     
 	* └[⛨PASS]~password: copied✔
	*/
	void text(const wchar_t* message, uint32_t x, uint32_t y)
	{
		const wchar_t left_up[] = L"┌[";
		const wchar_t middle[] = L"]~login: ";
		const wchar_t left_down[] = L"└[";
		const wchar_t right_down[] = L"]~password: ";
		const wchar_t copied[] = L"copied✔";
		disable_cursor(hConsole);

		size_t w_up = 
			  std::wcslen(left_up)
			+ std::wcslen(LOGO)
			+ std::wcslen(middle)
			+ std::wcslen(message);
		size_t w_down =
			std::wcslen(left_down)
			+ std::wcslen(LOGO)
			+ std::wcslen(right_down)
			+ std::wcslen(copied);
			
		size_t h = 1;

		std::vector<CHAR_INFO> buffer_up(w_up);
		std::vector<CHAR_INFO> buffer_down(w_down);

		clean();

		size_t ind_up = 0;
		draw_part(left_up, buffer_up, ind_up, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		draw_part(LOGO, buffer_up, ind_up, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		draw_part(middle, buffer_up, ind_up, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		draw_part(message, buffer_up, ind_up, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY);

		size_t ind_down = 0;
		draw_part(left_down, buffer_down, ind_down, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		draw_part(LOGO, buffer_down, ind_down, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		draw_part(right_down, buffer_down, ind_down, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		draw_part(copied, buffer_down, ind_down, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY);

		COORD bufferSize = { (SHORT)w_up, (SHORT)h };
		COORD bufferCoord = { 0, 0 };
		SMALL_RECT writeRegion = { (SHORT)x, (SHORT)y, (SHORT)(x + w_up - 1), (SHORT)(y + h - 1) };

		WriteConsoleOutputW(hConsole, buffer_up.data(), bufferSize, bufferCoord, &writeRegion);

		bufferSize = { (SHORT)w_down, (SHORT)h };
		writeRegion = { (SHORT)x, (SHORT)(y + 1) , (SHORT)(x + w_down - 1), (SHORT)(y + 1 +  h - 1) };
		WriteConsoleOutputW(hConsole, buffer_down.data(), bufferSize, bufferCoord, &writeRegion);
	}

};