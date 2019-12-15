#pragma once

#include <iostream>
#include <iterator>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <string>

namespace Log
{
	template<class IterT, class StringT>
	void Print(IterT first, IterT last, StringT &&fmtString)
	{
		std::cout << std::forward<StringT>(fmtString) << " = { ";
		std::copy(first, last, std::ostream_iterator<IterT::value_type>(std::cout, " "));
		std::cout << "}\n";
	}

	void Progress(std::unordered_map<std::string, float> &progresses)
	{
		int barWidth = 70;

		for (auto &progress : progresses)
		{
			std::cout << progress.first << "\n[";
			int pos = barWidth * progress.second;
			for (int i = 0; i < barWidth; ++i) {
				if (i < pos) std::cout << "=";
				else if (i == pos) std::cout << ">";
				else std::cout << " ";
			}
			std::cout << "] " << int(progress.second * 100.0) << " %\r";
			std::cout.flush();

			std::cout << std::endl;
		}
	}
}