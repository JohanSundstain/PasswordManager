#pragma once
#include <string>
#include <vector>
#include <sodium.h>
#include "GuardAllocator.hpp"

enum class Arrows {UP=72, DOWN=80, LEFT=75, RIGHT=77};
enum class Service {BACKSPACE=8, ENTER=13, ESCAPE=27};

inline bool is_arrow(int32_t key)
{
	switch (key)
	{
		case static_cast<int32_t>(Arrows::UP): return true;
		case static_cast<int32_t>(Arrows::DOWN): return true;
		case static_cast<int32_t>(Arrows::LEFT): return true;
		case static_cast<int32_t>(Arrows::RIGHT): return true;
		default: return false; 
	}
}

inline bool is_service(int32_t key)
{
	switch (key)
	{
		case static_cast<int32_t>(Service::ENTER): return true;
		case static_cast<int32_t>(Service::BACKSPACE): return true;
		case static_cast<int32_t>(Service::ESCAPE): return true;
		default: return false;
	}
}

struct Record
{
	char name[32];
	char login[128];
	char password[128];
}__attribute__((packed));

struct Header
{
	char salt[crypto_pwhash_SALTBYTES];
	char check_word[8] = "decoded";
	uint32_t num_of_records;
}__attribute__((packed));

class PasswordManager
{
private:
	
	std::unique_ptr<Header> header;
	std::vector<Record, GuardAllocator<Record>> records;

	const std::string data_file = "data.encoded";

	void encode_file();

	void decode_file();

public:
	PasswordManager();

	~PasswordManager();

	void get_key();

	void read_file();

	
};