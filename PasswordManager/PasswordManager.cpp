#define SODIUM_STATIC

#include <iostream>
#include <string>
#include <sodium.h>
#include <fstream>
#include <stdexcept>
#include <filesystem>

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
	master_password.reserve(128);
	master_password.get_allocator().lock(master_password.data(), master_password.size());
	secret_key.resize(crypto_secretbox_KEYBYTES);
	secret_key.get_allocator().lock(secret_key.data(), secret_key.size());
	records.reserve(5);
	records.get_allocator().lock(records.data(), records.size());

	is_first_time = false;
	state = States::LAUNCH;
};

PasswordManager::~PasswordManager(){};

void PasswordManager::launch()
{
	if (!fs::exists(data_file.c_str()))
	{
		is_first_time = true;
		header_ptr->num_of_records = 0;
		randombytes_buf(header_ptr->salt, sizeof(header_ptr->salt));
	}
	state = States::ENTER_PASSWORD;
}

void PasswordManager::enter_password()
{
	if (is_first_time)
	{
		console_ptr->interact("Create a master password between 5 and 128 characters", 0, 0, master_password);

		if (master_password.size() < 5)
		{
			console_ptr->message_box("Short password", 0, 3, 403, NULL);

			master_password.get_allocator().unlock(master_password.data(), master_password.size());
			std::fill(master_password.begin(), master_password.end(), 0);
			master_password.clear();
			master_password.get_allocator().lock(master_password.data(), master_password.size());
		}
		else
		{
			console_ptr->message_box("Password is created", 0, 3, 200, NULL);
			console_ptr->wait_key();
			console_ptr->clean();
			state = States::MENU;
			is_first_time = false;
		}
	}
	else
	{
		console_ptr->interact("Enter a master password", 0, 0, master_password);
		uint32_t code = decode_file();
		if (code == 200)
		{
			console_ptr->message_box("Success", 0, 3, 200, NULL);
			state = States::MENU;
			console_ptr->wait_key();
			console_ptr->clean();
		}
		else if (code == 403)
		{
			console_ptr->message_box("Wrong password", 0, 3, 403, NULL);
			console_ptr->wait_key();
		}
	}
}

void PasswordManager::menu()
{
	std::vector<std::string> options = {"SAVE PASSWORD", "FIND PASSWORD" };
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
		state = States::DELETE_PASSWORD;
	}

}

void PasswordManager::state_machine()
{
	switch (state)
	{
	case States::LAUNCH:		 launch(); break;
	case States::ENTER_PASSWORD: enter_password(); break;
	case States::MENU:			 menu(); break;
	default: break;
	}
}

void PasswordManager::run()
{
	while (state != States::EXIT)
	{
		state_machine();
	}
}

void PasswordManager::keygen()
{
	master_password.get_allocator().unlock(master_password.data(), master_password.size());
	secret_key.get_allocator().unlock(secret_key.data(), secret_key.size());
	
	if (crypto_pwhash(
		secret_key.data(),
		secret_key.size(),
		reinterpret_cast<const char*>(master_password.data()),
		master_password.size() * sizeof(wchar_t),
		header_ptr->salt,
		crypto_pwhash_OPSLIMIT_INTERACTIVE,
		crypto_pwhash_MEMLIMIT_INTERACTIVE,
		crypto_pwhash_ALG_DEFAULT) != 0)
	{
			SecureZeroMemory(master_password.data(), master_password.size());
			SecureZeroMemory(records.data(), records.size());
			SecureZeroMemory(secret_key.data(), secret_key.size());
			throw std::runtime_error("Out of memory during key derivation");
	}
	
	master_password.get_allocator().lock(master_password.data(), master_password.size());
	secret_key.get_allocator().lock(secret_key.data(), secret_key.size());
}

void PasswordManager::encode_file()
{
	std::vector<unsigned char> ciphertext(crypto_secretbox_MACBYTES + (records.size() * sizeof(Record)));
	randombytes_buf(header_ptr->nonce, sizeof(header_ptr->nonce));

	records.get_allocator().unlock(records.data(), records.size());
	secret_key.get_allocator().unlock(secret_key.data(), secret_key.size());
	if (crypto_secretbox_easy(
		ciphertext.data(),    
		reinterpret_cast<const unsigned char*>(records.data()),    
		records.size() * sizeof(Record),     
		header_ptr->nonce,
		secret_key.data()) != 0) 
	{
		SecureZeroMemory(master_password.data(), master_password.size());
		SecureZeroMemory(records.data(), records.size());
		SecureZeroMemory(secret_key.data(), secret_key.size());
		throw std::runtime_error("Error during encoding");
	}
	records.get_allocator().lock(records.data(), records.size());
	secret_key.get_allocator().lock(secret_key.data(), secret_key.size());

	dump(ciphertext);
}

uint32_t PasswordManager::decode_file()
{
	std::vector<unsigned char> ciphertext;

	download(ciphertext);

	records.get_allocator().unlock(records.data(), records.size());
	secret_key.get_allocator().unlock(secret_key.data(), secret_key.size());
	records.resize(header_ptr->num_of_records);

	int result = crypto_secretbox_open_easy(
		reinterpret_cast<unsigned char*>(records.data()),  
		ciphertext.data(),
		ciphertext.size(),  
		header_ptr->nonce,
		secret_key.data());
	records.get_allocator().lock(records.data(), records.size());
	secret_key.get_allocator().lock(secret_key.data(), secret_key.size());

	if (result == 0) 
	{
		return 200;
	}
	else 
	{
		return 403;
	}
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

	std::streamsize s = in.tellg();
	size_t size = static_cast<size_t>(in.tellg()) - sizeof(Header);
	ciphertext.resize(size);
	in.seekg(0, std::ios::beg);

	in.read(reinterpret_cast<char*>(header_ptr.get()), sizeof(Header));
	in.read(reinterpret_cast<char*>(ciphertext.data()), size);

	in.close();
}