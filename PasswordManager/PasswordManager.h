#pragma once
#include <string>
#include <vector>
#include <sodium.h>
#include <iostream>
#include <windows.h>
#include <iomanip> 

#include "Console.hpp"
#include "GuardAllocator.hpp"

#define NAME_MAX 32
#define LOGIN_MAX 128
#define PASSWORD_MAX 128
#define MIN_RECORD 5

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
	wchar_t name[NAME_MAX];
	wchar_t login[LOGIN_MAX];
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

	SafeVector<Record> records;
	SafeVector<wchar_t> master_password;
	
	States state;
	const std::string data_file = "data.encoded";

	void encode_file();

	uint32_t decode_file();

	uint32_t confirm_password(const wchar_t*, SafeVector<wchar_t>&, SafeVector<wchar_t>&, bool);

	// TODO: DELETE
	template <typename T>
	void memory_view(SafeVector<T>& password)
	{
		{
			COORD pos = { (WORD)0, (WORD)7 };
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleCursorPosition(hConsole, pos);
		
			std::cout  << std::hex;
			Bottleneck lock(password);
			const char* data_ptr = reinterpret_cast<const char*>(password.data());
			for (size_t i = 0; i < password.capacity() * sizeof(T); i++)
			{
				std::cout << std::setfill('0') << std::setw(2) << (uint32_t) data_ptr[i] << " ";
			}
			console_ptr->wait_key();
		}
	}

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

	void exit();

public:
	PasswordManager();

	~PasswordManager();

	void run();
};