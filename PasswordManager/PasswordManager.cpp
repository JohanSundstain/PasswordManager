#define SODIUM_STATIC

#include <iostream>
#include <string>
#include <sodium.h>
#include <fstream>
#include <stdexcept>
#include <filesystem>
#include <sstream>

#include "PasswordManager.h"
#include "GuardAllocator.hpp"

namespace fs = std::filesystem;

PasswordManager::PasswordManager()
{
	if (sodium_init() < 0)
	{
		throw std::runtime_error("PasswordManager: sodium_init() < 0\n");
	}
	header_ptr = std::make_unique<Header>();
	console_ptr = std::make_unique<Console>();

	records = CreateSafeVector<Record>(MIN_RECORD);
	master_password = CreateSafeVector<wchar_t>(PASSWORD_MAX);

	state = States::LAUNCH;
};

PasswordManager::~PasswordManager()
{
	EmergencyRemoval(master_password, records);
};

uint32_t PasswordManager::confirm_password(
	const wchar_t* message,
	SafeVector<wchar_t>& primary_buffer,
	SafeVector<wchar_t>& confirmation_buffer,
	bool check_length)
{
	console_ptr->clean_rect(0, 0, 128, 5);
	{
		Bottleneck lock(primary_buffer, confirmation_buffer);
		std::fill(primary_buffer.begin(), primary_buffer.end(), 0);
		std::fill(confirmation_buffer.begin(), confirmation_buffer.end(), 0);
		primary_buffer.clear();
		confirmation_buffer.clear();
	}

	console_ptr->interact(message, 0, 0, primary_buffer);

	if (check_length && primary_buffer.size() < 5)
	{
		console_ptr->message_box(L"short password. press any key", 0, 2, Console::ERR, NULL);
		console_ptr->wait_key();
		return Console::CODES::ERR;
	}

	console_ptr->clean_rect(0, 0, 128, 5);
	console_ptr->interact(L"confirm your password", 0, 0, confirmation_buffer);

	int result;
	{
		Bottleneck lock(primary_buffer, confirmation_buffer);
		result = sodium_memcmp(primary_buffer.data(),
			confirmation_buffer.data(),
			primary_buffer.size() * sizeof(wchar_t));
	}

	if (primary_buffer.size() != confirmation_buffer.size() || result != 0)
	{
		console_ptr->message_box(L"password doesn't match. press any key", 0, 2, Console::ERR, NULL);
		console_ptr->wait_key();
		return Console::CODES::ERR;
	}
	else
	{
		return Console::CODES::OK;
	}
}

void PasswordManager::launch()
{
	if (!fs::exists(data_file.c_str()))
	{
		state = States::SIGN_UP;
		header_ptr->num_of_records = 0;
		randombytes_buf(header_ptr->salt, sizeof(header_ptr->salt));
	}
	else
	{
		state = States::SIGN_IN;
	}
}

void PasswordManager::sign_up()
{
	SafeVector<wchar_t> primary_buffer = CreateSafeVector<wchar_t>(PASSWORD_MAX);
	SafeVector<wchar_t> confirm_buffer = CreateSafeVector<wchar_t>(PASSWORD_MAX);

	uint32_t code = confirm_password(L"create a master password", primary_buffer, confirm_buffer, true);
	
	if (code == Console::CODES::OK)
	{
		{
			Bottleneck lock(master_password);
			master_password.clear();
			master_password.resize(confirm_buffer.size());
		}

		{
			Bottleneck lock(master_password, confirm_buffer);
			std::memcpy(master_password.data(),
				confirm_buffer.data(),
				confirm_buffer.size() * sizeof(wchar_t));
		}
		state = States::MENU;
	}

	EmergencyRemoval(primary_buffer, confirm_buffer);
}

void PasswordManager::sign_in()
{
	console_ptr->clean_rect(0, 0, 128, 5);

	if (master_password.size() > 0)
	{
		Bottleneck lock(master_password);
		std::fill(master_password.begin(), master_password.end(), 0);
		master_password.clear();
	}

	DWORD read = console_ptr->interact(L"enter a master password", 0, 0, master_password);
	
	uint32_t result = decode_file();

	if (result == Console::CODES::OK)
	{
		state = States::MENU;
		return;
	}
	else
	{
		console_ptr->message_box(L"wrong password. press any key", 0, 2, Console::ERR, NULL);
		console_ptr->wait_key();
	}

	return;
}

void PasswordManager::exit()
{
	state = States::END;
	encode_file();
	EmergencyRemoval(master_password, records);
}

void PasswordManager::menu()
{
	std::vector<std::wstring> options = {
		L"SAVE PASSWORD",
		L"FIND PASSWORD", 
		L"    CLOSE    "
	};
	uint32_t option = console_ptr->menu(options);

	if (option == 0)
	{
		state = States::SAVE_PASSWORD;
	}
	else if (option == 1)
	{
		state = States::FIND_PASSWORD;
	}
	else if (option == 2)
	{
		state = States::EXIT;
	}

}


void PasswordManager::find_password()
{

}

void PasswordManager::save_password()
{
	console_ptr->clean();

	std::vector<wchar_t> name(NAME_MAX*2);
	std::vector<wchar_t> login(LOGIN_MAX*2);
	DWORD read = console_ptr->interact(L"alias", 0, 0, name);

	if (read - 2 == 0)
	{
		console_ptr->message_box(L"the alias cannot be empty", 0, 2, Console::CODES::ERR, NULL);
		console_ptr->wait_key();
		return;
	}
	if (read - 2 > NAME_MAX)
	{
		console_ptr->message_box(L"the alias is too long", 0, 2, Console::CODES::ERR, NULL);
		console_ptr->wait_key();
		return;
	}
	
	console_ptr->clean_rect(0, 0, 128, 5);
	console_ptr->label(L"alias", name.data(), 0, 6);
	read = console_ptr->interact(L"login", 0, 0, login);

	if (read-2 == 0)
	{
		console_ptr->message_box(L"the login cann't be empty", 0, 2, Console::CODES::ERR, NULL);
		console_ptr->wait_key();
		return;
	}

	console_ptr->clean_rect(0, 0, 128, 5);
	login.push_back(0);
	console_ptr->label(L"login", login.data(), 0, 7);

	// 3 time for entering password
	uint32_t code;
	SafeVector<wchar_t> primary_buffer = CreateSafeVector<wchar_t>(PASSWORD_MAX);
	SafeVector<wchar_t> confirm_buffer = CreateSafeVector<wchar_t>(PASSWORD_MAX);

	for (int time = 3; time >= 0; time--)
	{
		 code = confirm_password(L"password", primary_buffer, confirm_buffer, false);
		 if (code == Console::CODES::OK) break;
		 else
		 {
			 std::wstringstream ss;
			 ss << time << " times left";
			 console_ptr->clean_rect(0, 2, 123, 3);
			 console_ptr->message_box(ss.str().c_str(), 0, 2, Console::CODES::ERR, NULL);
			 console_ptr->wait_key();
		 }

	}

	if (code == Console::CODES::OK)
	{
		uint32_t& nor = header_ptr->num_of_records;

		if (nor == records.size())
		{
			Bottleneck lock(records);
			records.resize(nor * 2);
		}
		
		{
			Bottleneck lock(records, confirm_buffer);
			std::memcpy(records[nor].name, name.data(), NAME_MAX * sizeof(wchar_t));
			std::memcpy(records[nor].login, login.data(), LOGIN_MAX * sizeof(wchar_t));
			std::memcpy(records[nor].password, confirm_buffer.data(), PASSWORD_MAX * sizeof(wchar_t));
		}
		EmergencyRemoval(confirm_buffer, primary_buffer);
		console_ptr->message_box(L"success. press any key", 0, 2, Console::CODES::OK, NULL);
		console_ptr->wait_key();

		nor++;
	}
	
	state = States::MENU;
}

void PasswordManager::state_machine()
{
	switch (state)
	{
	case States::LAUNCH:		launch(); break;
	case States::SIGN_IN:		sign_in(); break;
	case States::SIGN_UP:		sign_up(); break;
	case States::SAVE_PASSWORD: save_password(); break;
	case States::FIND_PASSWORD: find_password(); break;
	case States::MENU:			menu(); break;
	case States::EXIT:			exit();  break;
	default: break;
	}
}

void PasswordManager::run()
{
	while (state != States::END)
	{
		state_machine();
	}
	console_ptr->clean();
}

void PasswordManager::keygen(SafeVector<unsigned char>& secret_key)
{
	int result = 0;
	{
		Bottleneck lock(master_password, secret_key);
		result = crypto_pwhash(
			secret_key.data(),
			crypto_secretbox_KEYBYTES,
			reinterpret_cast<const char*>(master_password.data()),
			master_password.size() * sizeof(wchar_t),
			header_ptr->salt,
			crypto_pwhash_OPSLIMIT_INTERACTIVE,
			crypto_pwhash_MEMLIMIT_INTERACTIVE,
			crypto_pwhash_ALG_DEFAULT);
	}

	if (result != 0)
	{
		EmergencyRemoval(master_password, secret_key, records);
		throw std::runtime_error("Out of memory during key derivation");
	}
}

void PasswordManager::encode_file()
{
	SafeVector<unsigned char> secret_key = CreateSafeVector<unsigned char>(crypto_secretbox_KEYBYTES);
	std::vector<unsigned char> 
		ciphertext(crypto_secretbox_MACBYTES + (header_ptr->num_of_records * sizeof(Record)));

	randombytes_buf(header_ptr->nonce, 	crypto_secretbox_NONCEBYTES);

	keygen(secret_key);

	int result = 0;
	{
		Bottleneck lock(records, secret_key);
		result = crypto_secretbox_easy(
			ciphertext.data(),
			reinterpret_cast<const unsigned char*>(records.data()),
			header_ptr->num_of_records * sizeof(Record),
			header_ptr->nonce,
			secret_key.data());
	}

	if (result != 0) 
	{
		EmergencyRemoval(master_password, secret_key, records);
		throw std::runtime_error("Error during encoding");
	}

	dump(ciphertext);
}

uint32_t PasswordManager::decode_file()
{
	std::vector<unsigned char> ciphertext;
	SafeVector<unsigned char> secret_key = CreateSafeVector<unsigned char>(crypto_secretbox_KEYBYTES);

	download(ciphertext);

	keygen(secret_key);

	int result;

	{
		Bottleneck lock(records, secret_key);
		records.resize(header_ptr->num_of_records);
		result = crypto_secretbox_open_easy(
			reinterpret_cast<unsigned char*>(records.data()),
			ciphertext.data(),
			ciphertext.size(),
			header_ptr->nonce,
			secret_key.data());
	}

	if (result == 0) return Console::CODES::OK;
	else return Console::CODES::ERR;
};

void PasswordManager::dump(const std::vector<unsigned char>& ciphertext) const
{
	std::ofstream out(data_file.c_str(), std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char*>(header_ptr.get()), sizeof(Header));
	out.write(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
	out.close();
}

void PasswordManager::download(std::vector<unsigned char>& ciphertext)
{
	std::ifstream in(data_file.c_str(), std::ios::binary | std::ios::ate);

	size_t size = static_cast<size_t>(in.tellg()) - sizeof(Header);
	ciphertext.resize(size);
	in.seekg(0, std::ios::beg);

	in.read(reinterpret_cast<char*>(header_ptr.get()), sizeof(Header));
	in.read(reinterpret_cast<char*>(ciphertext.data()), size);

	in.close();
}