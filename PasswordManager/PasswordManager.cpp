#define SODIUM_STATIC

#include <iostream>
#include <string>
#include <sodium.h>
#include <fstream>
#include <stdexcept>

#include "PasswordManager.h"
#include "GuardAllocator.hpp"

PasswordManager::PasswordManager()
{
	header = std::make_unique<Header>();
	
};

PasswordManager::~PasswordManager()
{

};


void PasswordManager::decode_file()
{
	std::ifstream in(data_file.c_str(), std::ios::binary);
};


void PasswordManager::read_file()
{
	std::ifstream in(data_file.c_str(), std::ios::binary);

	if (!in.is_open())
	{
		header->num_of_records = 0;
		randombytes_buf(header->salt, sizeof(header->salt));

		std::ofstream out(data_file.c_str(), std::ios::binary);

		out.write((const char*)header.get(), sizeof(Header));

		if (!out.good())
		{
			throw std::runtime_error("read_file: could not write a file");
		}

		out.close();

		in.open(data_file.c_str(), std::ios::binary);
	}

	in.read(reinterpret_cast<char*>(header.get()), sizeof(Header));

	if (sodium_memcmp(header->check_word, "decoded", 7) == 0)
	{
		std::cout << "Success" << std::endl;

		if (header->num_of_records > 0)
		{
			records.resize(header->num_of_records);

			in.read(reinterpret_cast<char*>(records.data()), sizeof(Record) * header->num_of_records);
		}
	}

	in.close();

}
