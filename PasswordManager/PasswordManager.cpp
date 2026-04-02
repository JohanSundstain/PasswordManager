#define SODIUM_STATIC

#include <iostream>
#include <string>
#include <sodium.h>
#include <fstream>
#include <stdexcept>
#include <filesystem>
#include <sstream>
#include <windows.h>
#include <chrono>

#include "PasswordManager.h"
#include "GuardAllocator.hpp"

namespace fs = std::filesystem;

using namespace std::chrono_literals;

PasswordManager::PasswordManager()
{
	if (sodium_init() < 0)
	{
		throw std::runtime_error("PasswordManager: sodium_init() < 0\n");
	}
	SetConsoleCP(65001);
	SetConsoleOutputCP(65001);

	header_ptr = std::make_unique<Header>();
	console_ptr = std::make_unique<Console>();

	records.reserve(MIN_RECORD);
	passwords = CreateSafeVector<Password>(MIN_RECORD);
	master_password = CreateSafeVector<wchar_t>(PASSWORD_MAX);

	state = States::LAUNCH;
};

PasswordManager::~PasswordManager()
{
	EmergencyRemoval(master_password, passwords);
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
	else if(primary_buffer.size() == 0)
	{
		console_ptr->message_box(L"password cannot be empty", 0, 2, Console::ERR, NULL);
		console_ptr->wait_key();
		return Console::CODES::ERR;
	}
	else
	{
		return Console::CODES::OK;
	}
}

void PasswordManager::register_names()
{
	std::array<wchar_t, USER_INFO_LIMIT> tmp_array;
	for (Record& rec : records)
	{
		std::copy(std::begin(rec.name),std::end(rec.name), tmp_array.begin());
		console_ptr->update_presumer(tmp_array);
	}
}

void PasswordManager::error(const wchar_t* message)
{
	console_ptr->clean_rect(0, 2, 128, 3);
	console_ptr->message_box(message, 0, 2, Console::ERR, NULL);
	console_ptr->wait_key();
}

void PasswordManager::success(const wchar_t* message)
{
	console_ptr->clean_rect(0, 2, 128, 3);
	console_ptr->message_box(message, 0, 2, Console::OK, NULL);
	console_ptr->wait_key();
}

void PasswordManager::progress_bar(const wchar_t* message,std::chrono::milliseconds)
{
	console_ptr->clean();
	console_ptr->wait_progress_bar(message, 32, 500ms, 1, 1);	
}

void PasswordManager::launch()
{
	progress_bar(LOGO, 500ms);

	if (!fs::exists(data_file.c_str()))
	{
		state = States::SIGN_UP;
		header_ptr->num_of_records = 0;
		randombytes_buf(header_ptr->salt, sizeof(header_ptr->salt));
		records.resize(MIN_RECORD);
		{
			Bottleneck lock(passwords);
			passwords.resize(MIN_RECORD);
		}
	}
	else
	{
		state = States::SIGN_IN;
	}
}

void PasswordManager::sign_up()
{
	console_ptr->clean();
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
		progress_bar(L"loading", 500ms);
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
	
	ask(L"enter a master password", master_password);
	uint32_t result = decode_file();

	progress_bar(L"cheaking", 500ms);

	if (result == Console::CODES::OK)
	{
		state = States::MENU;
		register_names();
		return;
	}
	else
	{
		error(L"wrong password. press any key");
	}

	return;
}

void PasswordManager::exit()
{
	state = States::END;
	encode_file();
	EmergencyRemoval(master_password, passwords);
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

void PasswordManager::copy_password(size_t index)
{
	console_ptr->clean();
	SafeVector<wchar_t> confirm_buffer = CreateSafeVector<wchar_t>(PASSWORD_MAX);

	ask(L"master password", confirm_buffer);

	int result;
	{
		Bottleneck lock(master_password, confirm_buffer);
		result = sodium_memcmp(master_password.data(),
			confirm_buffer.data(),
			confirm_buffer.size() * sizeof(wchar_t));
	}

	if (result != 0 || confirm_buffer.size() == 0 || master_password.size() != confirm_buffer.size())
	{
		error(L"wrong password");
		state = States::MENU;
		return;
	}

	HGLOBAL hLogin = GlobalAlloc(GMEM_MOVEABLE, USER_INFO_LIMIT * sizeof(wchar_t));
	bool is_copied = false;
	if (OpenClipboard(NULL))
	{
		EmptyClipboard();
		{
			Bottleneck lock(passwords);
			is_copied = CopyToClipboard(reinterpret_cast<const wchar_t*>(passwords[index].login), USER_INFO_LIMIT, hLogin);
		}
		if (!is_copied)
		{
			GlobalFree(hLogin);
			error(L"cannot copy login to clipboard");
			state = States::MENU;
			return;
		}
		CloseClipboard();
	}

	progress_bar(L"copying", 2000ms);

	is_copied = false;
	HGLOBAL hPass = GlobalAlloc(GMEM_MOVEABLE, PASSWORD_MAX * sizeof(wchar_t));
	if (OpenClipboard(NULL))
	{
		EmptyClipboard();
		{
			Bottleneck lock(passwords);
			is_copied = CopyToClipboard(reinterpret_cast<const wchar_t*>(passwords[index].password), PASSWORD_MAX, hPass);
		}
		if (!is_copied)
		{
			GlobalFree(hPass);
			error(L"cannot copy password to clipboard");
			state = States::MENU;
			return;
		}
		CloseClipboard();
	}

	if (is_copied)
	{
		state = States::MENU;
		return;
	}
	
	state = States::MENU;
	
}

void PasswordManager::find_password()
{
	console_ptr->clean();
	std::vector<wchar_t> alias;
	alias.reserve(USER_INFO_LIMIT);

	ask(L"enter password's alias", alias);

	wchar_t* ptr = alias.data();

	auto it = std::find_if(
		records.begin(), 
		records.end(),
		[ptr](Record& record)
		{
			return std::wcsncmp(ptr, record.name, USER_INFO_LIMIT) == 0;
		});

	if (it != records.end())
	{
		size_t index = std::distance(records.begin(), it);
		success(L"password found");

		std::vector<std::wstring> options = {
		L"  COPY  ",
		L" DELETE ",
		L"  MENU  "};

		uint32_t option = console_ptr->menu(options);

		if (option == 0)
		{
			copy_password(index);
		}
		if (option == 1)
		{
			delete_password(index);
		}
		if (option == 2)
		{
			state = States::MENU;
		}

	}
	else
	{
		error(L"password was not found");
		state = States::MENU;
	}
}

void PasswordManager::delete_password(size_t index)
{
	console_ptr->clean();
	SafeVector<wchar_t> confirm_buffer = CreateSafeVector<wchar_t>(PASSWORD_MAX);

	ask(L"master password", confirm_buffer);

	int result;
	{
		Bottleneck lock(master_password, confirm_buffer);
		result = sodium_memcmp(master_password.data(),
			confirm_buffer.data(),
			confirm_buffer.size() * sizeof(wchar_t));
	}

	if (result != 0 || confirm_buffer.size() == 0 || master_password.size() != confirm_buffer.size())
	{
		error(L"wrong password");
		state = States::MENU;
		return;
	}

	{
		Bottleneck lock(passwords);
		passwords.erase(passwords.begin() + index);
	}
	records.erase(records.begin()+index);

	progress_bar(L"deleting", 500ms);

	state = States::MENU;

	
}

void PasswordManager::save_password()
{
	console_ptr->clean();
	std::vector<wchar_t> alias;
	std::vector<wchar_t> login;

	alias.reserve(USER_INFO_LIMIT);
	login.reserve(USER_INFO_LIMIT);

	ask(L"alias", alias);
	if (alias.size() == 0) 
	{ 
		error(L"the alias cannot be empty"); 
		state = States::MENU; 
		return;
	}

	ask(L"login", login);
	if (login.size() == 0) 
	{ 
		error(L"the login cann't be empty"); 
		return; 
	}

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
			 error(ss.str().c_str());
		 }
	}

	if (code == Console::CODES::OK)
	{
		uint32_t& nor = header_ptr->num_of_records;

		if (nor == passwords.size())
		{
			{
				Bottleneck lock(passwords);
				passwords.resize(nor * 2);
			}
			records.resize(nor * 2);
		}
		
		{
			Bottleneck lock(passwords, confirm_buffer);
			std::memcpy(passwords[nor].login, login.data(), USER_INFO_LIMIT * sizeof(wchar_t));
			std::memcpy(passwords[nor].password, confirm_buffer.data(), PASSWORD_MAX * sizeof(wchar_t));
		}
		std::memcpy(records[nor].name, alias.data(), USER_INFO_LIMIT * sizeof(wchar_t));
		
		std::array<wchar_t, USER_INFO_LIMIT> tmp_array;
		std::copy(alias.begin(), alias.end(), tmp_array.begin());
		console_ptr->update_presumer(tmp_array);

		EmergencyRemoval(confirm_buffer, primary_buffer);
		success(L"success. press any key");
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
	BOOL isDebuggerPresent = FALSE;
	CheckRemoteDebuggerPresent(GetCurrentProcess(), &isDebuggerPresent);
	while (state != States::END)
	{
		if (isDebuggerPresent)
		{
			return;
		}
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
		EmergencyRemoval(master_password, secret_key, passwords);
		throw std::runtime_error("Out of memory during key derivation");
	}
}

void PasswordManager::encode_file()
{
	SafeVector<unsigned char> secret_key = CreateSafeVector<unsigned char>(crypto_secretbox_KEYBYTES);
	std::vector<unsigned char> ciphertext(crypto_secretbox_MACBYTES + (header_ptr->num_of_records * sizeof(Password)));

	randombytes_buf(header_ptr->nonce, 	crypto_secretbox_NONCEBYTES);

	keygen(secret_key);

	int result = 0;
	{
		Bottleneck lock(passwords, secret_key);
		result = crypto_secretbox_easy(
			ciphertext.data(),
			reinterpret_cast<const unsigned char*>(passwords.data()),
			header_ptr->num_of_records * sizeof(Password),
			header_ptr->nonce,
			secret_key.data());
	}

	if (result != 0) 
	{
		EmergencyRemoval(master_password, secret_key, passwords);
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
		Bottleneck lock(passwords, secret_key);
		passwords.resize(header_ptr->num_of_records);
		result = crypto_secretbox_open_easy(
			reinterpret_cast<unsigned char*>(passwords.data()),
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
	out.write(reinterpret_cast<const char*>(records.data()), header_ptr->num_of_records * sizeof(Record));
	out.write(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
	out.close();
}

void PasswordManager::download(std::vector<unsigned char>& ciphertext)
{
	std::ifstream in(data_file.c_str(), std::ios::binary | std::ios::ate);

	size_t size = static_cast<size_t>(in.tellg());
	in.seekg(0, std::ios::beg);

	in.read(reinterpret_cast<char*>(header_ptr.get()), sizeof(Header));

	records.resize(header_ptr->num_of_records);
	in.read(reinterpret_cast<char*>(records.data()), sizeof(Record) * header_ptr->num_of_records);

	size -= (sizeof(Header) + (sizeof(Record) * header_ptr->num_of_records));
	ciphertext.resize(size);
	in.read(reinterpret_cast<char*>(ciphertext.data()), size);
	in.close();
}