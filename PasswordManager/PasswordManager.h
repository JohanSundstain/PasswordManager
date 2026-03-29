#pragma once
#include <string>
#include <vector>
#include <sodium.h>
#include "GuardAllocator.hpp"
#include "Console.hpp"


enum class States {
	LAUNCH,
	ENTER_PASSWORD, 
	EXIT, SAVE_PASSWORD,
	FIND_PASSWORD, 
	MENU,
	DELETE_PASSWORD
};

struct Record
{
	wchar_t name[32];
	wchar_t login[128];
	wchar_t password[128];
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
	bool is_first_time;
	std::unique_ptr<Header> header_ptr;
	std::unique_ptr<Console> console_ptr;
	std::vector<Record, GuardAllocator<Record>> records;
	std::vector<unsigned char, GuardAllocator<unsigned char>> secret_key;
	std::vector<wchar_t, GuardAllocator<wchar_t>> master_password;
	
	States state;

	const std::string data_file = "data.encoded";

	void encode_file();

	uint32_t decode_file();

public:
	PasswordManager();

	~PasswordManager();

	void run();

	void state_machine();

	void keygen();

	void dump(const std::vector<unsigned char>& ciphertext) const;

	void download(std::vector<unsigned char>& ciphertext);

	// states functions
	void launch();

	void enter_password();

	void menu();
	
};