#include <iostream>

using namespace std;

int CountNumbers(const int& first_num, int second_num)
{
	int result = 0;

	for (int i = first_num; i < second_num; i++)
	{
		if (i / 100 == 3 || i % 10 == 3 || (i / 10) % 10 == 3)
		{
			result++;
		}
	}
	return result;
}

int main()
{
	int first_num = 1;
	int second_num = 1000;
	cout << CountNumbers(first_num, second_num) << endl;
}
// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь:

// Закомитьте изменения и отправьте их в свой репозиторий.
