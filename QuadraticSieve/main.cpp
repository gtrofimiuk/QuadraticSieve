#include <fstream>
#include "bignum.h"
#include <ipp.h>
#include <stdio.h>
#include "Factorization.h"
#include <time.h>

int main(){

	fstream in("input.txt");
	std::string num;
	in >> num;
	Factorization comp = Factorization(num.c_str());
	BigNumber test(num.c_str());
	clock_t start = clock();

	std::map<BigNumber, Ipp32u> factor = comp.getFactor();
	cout << "Factorization completed! " << endl<<"time = "<<clock() - start << endl;
	for (auto& i : factor){
		cout << i.first;
		if (i.second > 1)
			cout << "^" << i.second;
		cout << endl;
	}

	system("pause");
	return 0;
}