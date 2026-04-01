#pragma once 
#include <vector>
#include <iostream>
#include <string>
#include <algorithm>

// TODO: Добавить удаление

class Presumer
{
private:
	struct node
	{
		std::vector<node*> childs;
		wchar_t ch;

		node(wchar_t ch) : ch(ch) {};
		~node() {
			for (node* n : childs)
			{
				std::wcout << L"deleted " << n->ch << std::endl;
				delete n;
			}
				
		}
		node(const node&) = delete;
		node& operator=(const node&) = delete;
	};

	std::unique_ptr<node> root_uptr;
	node* current_ptr;
public:

	Presumer()
	{
		root_uptr = nullptr;
		current_ptr = nullptr;
	}

	void enter_data(std::vector<std::vector<wchar_t>>& data)
	{
		for (size_t i = 0; i < data.size(); i++)
		{
			add(data[i]);
		}
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

	void add(std::vector<wchar_t>& key)
	{
		if (root_uptr == nullptr)
		{
			root_uptr = std::make_unique<node>(L'\0');
		}
		current_ptr = root_uptr.get();
		
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
