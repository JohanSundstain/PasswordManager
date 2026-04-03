#pragma once 
#include <vector>
#include <array>
#include <algorithm>
#include <stack>

#include "PasswordManager.h"

class Presumer
{
private:
	struct node
	{
		std::vector<node*> childs;
		wchar_t ch;
		bool is_key;

		node(wchar_t ch, bool is_key) : ch(ch), is_key(is_key) {};
		~node() { for (node* n : childs)delete n; }
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

	void get_matchings(std::vector<wchar_t>& key, std::vector<std::wstring>& result)
	{
		if (root_uptr == nullptr)
		{
			return;
		}
		if (key.size() == 0)
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

		std::wstring basic_string(key.begin(), key.end());
		std::stack<std::pair<node*, std::wstring>> stack;
		if (current_ptr->is_key)
		{
			result.push_back(basic_string);
		}
		for (node* child : current_ptr->childs)
		{
			stack.push(std::make_pair(child, basic_string));
		}

		while (!stack.empty())
		{
			auto& [top, str] = stack.top();
			stack.pop();
			str += top->ch;
			if (top->is_key)
			{
				result.push_back(str);
			}
			for (node* child : top->childs)
			{
				stack.push(std::make_pair(child, str));
			}

		}
	}

	void add(std::array<wchar_t, USER_INFO_LIMIT>& key)
	{
		if (root_uptr == nullptr)
		{
			root_uptr = std::make_unique<node>(0x08, false);
		}
		size_t size = std::wcslen(key.data());

		current_ptr = root_uptr.get();
		bool is_key = false;
		
		for (size_t i = 0; i < size; i++)
		{
			wchar_t c = key[i];
			is_key = (i == size - 1);

			auto it = std::find_if(
				current_ptr->childs.begin(), 
				current_ptr->childs.end(),
				[c](node* n)
				{
					return n->ch == c;
				});

			if (it == current_ptr->childs.end())
			{
				node* new_node = new node(c, is_key);
				current_ptr->childs.push_back(new_node);
				current_ptr = new_node;
			}
			else
			{
				current_ptr = *it;
				current_ptr->is_key = is_key;
			}
		}
	}
};
