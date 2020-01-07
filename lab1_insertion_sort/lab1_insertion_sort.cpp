// lab1_insertion_sort.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include "pch.h"
#include <iostream>
#include <vector>
#include <time.h>
#include <algorithm>

#include "../include/io_plugin.h"
#include "../include/Sort.h"
#include "../include/Log.h"

int main()
{
	std::vector<int64_t> arr( IO::GetValueFromUser<std::int64_t>("Eneter array size:\n", [](auto &val) { return val > 0; }) );
	
	std::srand(unsigned(time(0)));
	std::generate(arr.begin(), arr.end(), []() { return std::rand() % 1000 - 499; });
		
	Log::Print(arr.cbegin(), arr.cend(), "source array");
	
	algorithm::info<uint64_t> info{ 0,0 };
	algorithm::sort::Insertion(info, arr.begin(), arr.end(), [](auto first, auto second) { return *first > *second; });

	Log::Print(arr.cbegin(), arr.cend(), "sorted array");
	std::cout << "\nComparers count: " << info.comparers 
			  << "\nSwaps count: " << info.swaps 
			  << "\n\n";

	std::cout << "Variant 10 Insertion sort.\n\n";

	system("pause");
}