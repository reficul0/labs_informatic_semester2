#pragma once

namespace IO
{
	template<typename ValueType, typename IsValueValidFunction, typename MessageT>
	ValueType GetValueFromUser(MessageT&& message, IsValueValidFunction isValueValid)
	{
		using namespace std;

		ValueType value = 0;
		bool isValueInvalid = true;

		while (isValueInvalid)
		{
			cout << std::forward<MessageT>(message);
			cin >> value;

			bool isStreamInFailureState = cin.fail();
			isValueInvalid = isStreamInFailureState || !isValueValid(value);

			if (isValueInvalid)
			{
				if (isStreamInFailureState)
					cin.clear();// возвращаем объект потока в обычный режим
				cout << "Value is invalid. Try enter again. " << endl;
			}
			cin.ignore(numeric_limits<streamsize>::max(), '\n');
		}

		system("cls");

		return value;
	}
}