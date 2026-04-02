#pragma once
#define USER_INFO_LIMIT 32
#define PASSWORD_MAX 128
#define MIN_RECORD 5

#include <string>
#include <vector>
#include <sodium.h>
#include <iostream>
#include <windows.h>

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

	std::unique_ptr<Header> header_ptr;
	std::unique_ptr<Console> console_ptr;

	std::vector<Record> records;
	SafeVector<Password> passwords;
	SafeVector<wchar_t> master_password;
	
	States state;
	const std::string data_file = "data.encoded";

	void encode_file();

	uint32_t decode_file();

	uint32_t confirm_password(const wchar_t*, SafeVector<wchar_t>&, SafeVector<wchar_t>&, bool);

	void keygen(SafeVector<unsigned char>&);

	void state_machine();

	void dump(const std::vector<unsigned char>&) const;

	void download(std::vector<unsigned char>&);

	// states functions
	void launch();

	void sign_in();

	void sign_up();

	void menu();

	void save_password();

	void find_password();

	void copy_password(size_t);

	void delete_password(size_t);

	void exit();

	void register_names();

	void error(const wchar_t*);

	void success(const wchar_t*);

	void progress_bar(const wchar_t* message, std::chrono::milliseconds);

	template <typename TContainer>
	void ask(const wchar_t* message, TContainer& safe_buffer)
	{
		console_ptr->clean_rect(0, 0, 128, 1);
		console_ptr->interact(message, 0, 0, safe_buffer);
	}

	inline bool CopyToClipboard(const wchar_t* text, size_t n, HGLOBAL& hMem)
	{
		size_t data_size = sizeof(wchar_t) * n;

		if (hMem)
		{
			void* pDest = GlobalLock(hMem);
			if (pDest)
			{
				CopyMemory(pDest, text, data_size);
				GlobalUnlock(hMem);
				if (SetClipboardData(CF_UNICODETEXT, hMem) == NULL)
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
		return true;
	}



public:
	PasswordManager();

	~PasswordManager();

	void run();
};