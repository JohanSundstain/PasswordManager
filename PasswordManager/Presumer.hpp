#pragma once 
#include <vector>
#include <iostream>
#include <array>
#include <algorithm>

#include "PasswordManager.h"
// TODO: Добавить удаление

class Presumer
{
private:
	struct node
	{
		std::vector<node*> childs;
		wchar_t ch;

		node(wchar_t ch) : ch(ch) {};
		~node() { for (node* n : childs)delete n; }
		node(const node&) = delete;
		node& operator=(const node&) = delete;
	};

	std::unique_ptr<node> root_uptr;
	node* current_ptr;
public:

	Presumer()
	{
		std::array<int, 20> s;
		root_uptr = nullptr;
		current_ptr = nullptr;
	}

	void assume(std::vector<wchar_t>& key)
	{
		if (root_uptr == nullptr)
		{
			return;
		}
		current_ptr = root_uptr.get();

		// Going down as we can
		for (size_t i = 0; i < key.size(); i++)
		{
			wchar_t c = key[i];
			auto it = std::find_if(
				current_ptr->childs.begin(), 
				current_ptr->childs.end(),
				[c](node* n)
				{
					return n->ch == c;
				});

			// if find an end, return back 
			if (it == current_ptr->childs.end()) 
			{
				return; 
			}
			else
			{
				current_ptr = *it;
			}
		}

		while (current_ptr->childs.size() == 1)
		{
			current_ptr = *current_ptr->childs.begin();
			key.push_back(current_ptr->ch);
		}
	}

	void add(std::array<wchar_t, USER_INFO_LIMIT>& key)
	{
		if (root_uptr == nullptr)
		{
			root_uptr = std::make_unique<node>(0x08);
		}
		current_ptr = root_uptr.get();
		
		for (size_t i = 0; i < key.size(); i++)
		{
			wchar_t c = key[i];

			if (c == L'\0')
			{
				break;
			}

			auto it = std::find_if(
				current_ptr->childs.begin(), 
				current_ptr->childs.end(),
				[c](node* n)
				{
					return n->ch == c;
				});

			if (it == current_ptr->childs.end())
			{
				node* new_node = new node(c);
				current_ptr->childs.push_back(new_node);
				current_ptr = new_node;
			}
			else
			{
				current_ptr = *it;
			}
		}
	}
};
