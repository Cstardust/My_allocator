#include<iostream>
#include<vector>
#include"myallocator.h"

int main()
{
	std::vector<int,my_allocator<int>> vec;
	for (int i = 0; i < 100; ++i)
	{
		vec.push_back(i);
	}

	for (auto x : vec) {
		std::cout << x << std::endl;
	}
}