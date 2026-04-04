#pragma once
#define USER_INFO_LIMIT 256
#define PASSWORD_MAX 256
#define MIN_RECORD 5

#include <string>
#include <vector>
#include <windows.h>
#include <sodium.h>

#include "Console.hpp"
#include "GuardAllocator.hpp"

enum class States {
	LAUNCH,
	SIGN_IN,
	SIGN_UP,
	EXIT, 
	END, 
	SAVE_PASSWORD,
	FIND_PASSWORD, 
	CIPHER,
	MENU,
	DELETE_PASSWORD
};

struct Record
{
	wchar_t name[USER_INFO_LIMIT];
}__attribute__((packed));

struct Password
{
	wchar_t login[USER_INFO_LIMIT];
	wchar_t password[PASSWORD_MAX];
}__attribute__((packed));

struct Header
{
	unsigned char salt[crypto_pwhash_SALTBYTES];
	unsigned char nonce[crypto_secretbox_NONCEBYTES];
	uint32_t num_of_records;

}__attribute__((packed));


class PasswordManager
{
private:
	static PasswordManager* instance;

	std::unique_ptr<Header> header_ptr;
	std::unique_ptr<Console> console_ptr;

	std::vector<Record> records;
	SafeVector<Password> passwords;
	SafeVector<wchar_t> master_password;
	
	States state;
	const std::wstring data_file = L"data.encoded";

	void dump(const std::vector<unsigned char>&) const;

	void dump(const std::vector<unsigned char>&, const wchar_t*) const;

	void download(std::vector<unsigned char>&);

	void download(std::vector<unsigned char>&, const wchar_t*);

	void encode_file(const wchar_t*);

	uint32_t decode_file(const wchar_t*);

	uint32_t confirm_password(const wchar_t*, SafeVector<wchar_t>&, SafeVector<wchar_t>&, bool);

	void keygen(SafeVector<unsigned char>&, const unsigned char*);

	void state_machine();


	// states functions

	void launch();

	void sign_in();

	void sign_up();

	void menu();

	void save_password();

	void find_password();

	void cipher();

	void copy_password(size_t);

	void delete_password(size_t);

	void emergency_exit();

	void exit_regular();

	void register_names();

	void error(const wchar_t*);

	void success(const wchar_t*);

	void show_pass(const wchar_t*);

	void progress_bar(const wchar_t* message, std::chrono::milliseconds);

	template <typename TContainer>
	void ask(const wchar_t* message, TContainer& safe_buffer)
	{
		console_ptr->clean_rect(0, 0, 128, 1);
		console_ptr->interact(message, 0, 0, safe_buffer);
	}

	inline void clean_path(std::vector<wchar_t>& buffer)
	{
		buffer.erase(std::remove(buffer.begin(), buffer.end(), L'\"'), buffer.end());
		buffer.erase(std::remove(buffer.begin(), buffer.end(), L'\''), buffer.end());

		auto first = std::find_if(
			buffer.begin(), 
			buffer.end(),
			[](wchar_t ch) 
			{
				return ch != L' ' && ch != L'\t' && ch != L'\0';
			});

		buffer.erase(buffer.begin(), first);

		auto last = std::find_if(
			buffer.rbegin(),
			buffer.rend(),
			[](wchar_t ch)
			{
				return ch != L' ' && ch != L'\t' && ch != L'\0';
			});

		if (last != buffer.rend())
		{
			buffer.erase(last.base(), buffer.end());
			buffer.push_back(L'\0');
		}
	}

	inline bool CopyToClipboard(const wchar_t* text)
	{
		size_t len = std::wcslen(text) + 1;
		size_t size = len * sizeof(wchar_t);

		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
		if (!hMem) return false;

		void* pDest = GlobalLock(hMem);
		if (!pDest)
		{
			GlobalFree(hMem);
			return false;
		}

		std::memcpy(pDest, text, size);
		GlobalUnlock(hMem);

		if (!OpenClipboard(nullptr))
		{
			GlobalFree(hMem);
			return false;
		}

		EmptyClipboard();

		if (!SetClipboardData(CF_UNICODETEXT, hMem))
		{
			CloseClipboard();
			GlobalFree(hMem);
			return false;
		}

		CloseClipboard();
		return true;
	}


public:
	PasswordManager();

	~PasswordManager();

	void run();

	static BOOL WINAPI ctrl_handler(DWORD fdw_ctrl_type)
	{

		if (fdw_ctrl_type == CTRL_C_EVENT || fdw_ctrl_type == CTRL_BREAK_EVENT)
		{
			return TRUE;
		}
		else if (fdw_ctrl_type == CTRL_CLOSE_EVENT)
		{
			instance->emergency_exit();
			return TRUE;
		}
	
		return FALSE;
	}

};