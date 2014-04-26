#include "QuadraticSieve.h"
#include <memory>

QuadraticSieve::QuadraticSieve(BigNumber& n){
	N = n;
}

BigNumber QuadraticSieve::modPow(const BigNumber&a, const BigNumber& k, const BigNumber& n){
	int deg = k.BitSize();
	BigNumber b(BigNumber::One());
	if (k == BigNumber::Zero())
		return b;

	string s;
	k.num2hex(s);
	Ipp32u size = 0;
	Ipp32u pl = 3;
	while (s[pl] == '0')
		++pl;
	size = s.length() - pl;
	vector<string> bit(size);
	for (auto& i : bit){
		switch (s[pl]){
		case '0': i = "0000"; break; case '4': i = "0100"; break; case '8': i = "1000"; break; case 'C': i = "1100"; break;
		case '1': i = "0001"; break; case '5': i = "0101"; break; case '9': i = "1001"; break; case 'D': i = "1101"; break;
		case '2': i = "0010"; break; case '6': i = "0110"; break; case 'A': i = "1010"; break; case 'E': i = "1110"; break;
		case '3': i = "0011"; break; case '7': i = "0111"; break; case 'B': i = "1011"; break; case 'F': i = "1111"; break;
		}
		++pl;
	}
	//Now, we have all bits of k
	BigNumber A(a);
	if (bit[size - 1][3] == '1')
		b = a;
	for (int i = 1; i < deg; i++){
		A *= A;
		A %= n;
		if (bit[size - 1 - i / 4][3 - i % 4] == '1'){
			b *= A;
			b %= n;
		}
	}
	return b;
}

BigNumber QuadraticSieve::LegendreSymbol(BigNumber& a, BigNumber& p){

	BigNumber r(modPow(a, (p - BigNumber::One()) / BigNumber::Two(), p));
	if (r > BigNumber::One())
		r -= p;
	return r;
}

std::pair<BigNumber, BigNumber> QuadraticSieve::doFactorization(){
	cout << "bit size of " << N << " is " << N.BitSize() << endl;
	double nLg = N.b_ln();
	double t1 = exp(0.5*sqrt(nLg) * sqrt(log(nLg))); //TODO: experiment with it
	Ipp32u t = ceil(t1);
	t /= 10;  //Optimize this
	fbSize = t;
	cout << "First factor base size chosen as "<<fbSize << endl;

	Ipp32u len;
	if (N.BitSize() >= 200)
		len = t*ceill(sqrtl(sqrtl(t)));
	else if (N.BitSize() >= 50)
		len = t*ceill(sqrtl(t));  //TODO: optimize this bound
	else
		len = t*t;

	//Sieve of Eratosphenes
	std::vector<bool> arr(EratospheneSieve(len + 1));

	Ipp32u counter = 1;
	Ipp32u l = len-1;
	Base.push_back(BigNumber::MinusOne());
	Base.push_back(BigNumber::Two());
	//This statemet are easy for parallel
	for (Ipp32u i = 3; i != l && counter <t-1; ++i){
		if (arr[i]){
			BigNumber buf(i);
			if (LegendreSymbol(N, buf) == BigNumber::One()){      
				Base.push_back(buf);
				if (counter % 100000 == 0)
					cout << counter << endl;
				++counter;
			}
		}
	}

	cout << "Base size: " << Base.size() << endl;
	sieving();

	return divisors;
}

vector<pair<BigNumber, vector<bool>>> QuadraticSieve::sieving(){

	Ipp32u decimal_size = (ceil((float)N.BitSize() / log2(10)));
	Ipp32s M;
	cout << "decimal length of number " << decimal_size << endl;
	//I'll optimize it later
	if (decimal_size <= 42)
		M = 50000;
	else if (decimal_size <= 50)
		M = 100000;
	else if (decimal_size <= 55)
		M = 250000;
	else if (decimal_size <= 60)
		M = 350000;
	else if (decimal_size <= 67)
		M = 500000;
	else
		M = 1000000;
	cout << " M " << M<<endl;
	//select k such as kN = 1 mod 8
	BigNumber k(BigNumber::One());
	BigNumber eight(8);
	bool flag = false;
	while (!flag){
		if (k*N % eight == BigNumber::One()){
			flag = true;
		}
		else
			k += BigNumber::One();
	}
	//define pseudo random generator
	int ctxSize;
	int maxBitSize = (((k.BitSize() + N.BitSize()) / 2) - log2(M))/2;
	ippsPrimeGetSize(maxBitSize, &ctxSize);
	IppsPrimeState* pPrimeG = (IppsPrimeState*)(new Ipp8u[ctxSize]);
	ippsPrimeInit(maxBitSize, pPrimeG);

	ippsPRNGGetSize(&ctxSize);
	IppsPRNGState* pRand = (IppsPRNGState*)(new Ipp8u[ctxSize]);
	ippsPRNGInit(160, pRand);
	srand(time(NULL));
	ippsPRNGSetSeed(BN(BigNumber(rand())), pRand);

	//from this moment i can parallel my programm later

	//fill array of log(p[i])
	vector<float> prime_log(Base.size() - 1);

	auto& b_end = Base.end();
	for (auto& it = Base.begin() + 1; it != b_end; ++it){
		Ipp32u ind = it - Base.begin()-1;
		vector<Ipp32u> v;
		it->num2vec(v);
		prime_log[ind] = log(v[0]);
	}
	vector<float> sieve(2 * M + 1,0);
	//result matrix
	vector<pair<BigNumber, vector<bool>>> result(Base.size() + 1);
	for (auto& i : result){
		i.second = std::vector<bool>(Base.size() + 1);
	}

	Ipp32u counter = 0;
	Ipp32u bound = Base.size()+1;
	BigNumber three(3);
	BigNumber four(4);
	BigNumber kN(k*N);
	BigNumber A, h0, h1, h12, tmp, h2, B,D,tmpInvA,tmpL,r1,r2;
	while (counter < bound){
		cout << "switch polynom" << endl;
		//generate coefficients
		ippsPrimeGen_BN(D, maxBitSize, nTrials, pPrimeG, ippsPRNGen, pRand);
		while (!(D.isPrime(nTrials) && (D % 4 == three))){
			ippsPrimeGen_BN(D, maxBitSize, nTrials, pPrimeG, ippsPRNGen, pRand);
		}
		cout << "D " << D << endl;
		A = D*D;
		h0 = modPow(kN, (D - three) / four, D);
		h1 = kN*h0;
		h12 = h1*h1;
		tmp = (BigNumber::Two() * h1).InverseMul(D);
		h2 = (tmp * ((kN - h12) / D)) % D;
		B = (h1 + h2*D) % A;
		cout << "B " << B << endl;
		//Q(x) = ((2 * A *x + B)/2 * D)^2 mod kN
		int prime_deg = 1; //Play with it
		float epsilon = 2;
		for (int l = 1; l <= prime_deg; l++){
			for (auto& it = Base.begin() + 1; it != b_end-1; ++it){
				tmpL = (it->b_power(l));
				tmp = kN.b_sqrt();
				tmpInvA = (BigNumber::Two()*A).InverseMul(tmpL);
				r1 = (B*BigNumber::MinusOne() + tmp)*(tmpInvA) % tmpL;
				r2 = (B*BigNumber::MinusOne() - tmp)*(tmpInvA) % tmpL;
				Ipp32u ind = it - Base.begin() - 1;
				vector<Ipp32u> v, v1, v2;
				r1.num2vec(v);
				tmpL.num2vec(v1);
				r2.num2vec(v2);
				Ipp32s stepR1 = v[0];
				Ipp32u primePower = v1[0];
				Ipp32s stepR2 = v2[0];
				//sieveing by first root
				while (stepR1 <= M){
					sieve[stepR1 + M] += l*prime_log[ind];
					stepR1 += primePower;
				}
				stepR1 = v[0] + M - primePower;
				while (stepR1 >= 0){
					sieve[stepR1] += l*prime_log[ind];
					stepR1 -= primePower;
				}
				//sieving by second root
				while (stepR2 <= M){
					sieve[stepR2 + M] += l*prime_log[ind];
					stepR2 += primePower;
				}
				stepR2 = v2[0] + M - primePower;
				while (stepR2 >= 0){
					sieve[stepR2] += l*prime_log[ind];
					stepR2 -= primePower;
				}
			} //end for one prime from factor base
		}//end for all degrees of primes from FB
		float aim = N.b_ln() / 2 + log(M);
		BigNumber R, x;
		auto& s_end = sieve.end();
		for (auto& it = sieve.begin(); it != s_end; ++it){
			if (abs(*it - aim) < epsilon){
				x = BigNumber((Ipp32u(it - sieve.begin()) - M));
				if (x < BigNumber::Zero())
					result[counter].second[0] = true;
				for (auto& jt = Base.begin()+1; jt != Base.end(); ++jt){
					Ipp32u ind = jt - Base.begin();
					Ipp32u deg = 0;
					while (x % *jt == BigNumber::Zero()){

					}
				}
			}
		}
	}

	return result;
}

//Algorithm return x -> x^2 = a (mod p) or return false
BigNumber QuadraticSieve::Tonelli_Shanks(BigNumber& a, BigNumber& p){
	Ipp32u e = 0;
	BigNumber q = p - BigNumber::One();
	while (q.IsEven()){
		++e;
		q /= 2;
	}
	//1.Find generator
	BigNumber n;
	int size;
	int numSize = 10;  //What size "n" should be?

	ippsPRNGGetSize(&size);
	IppsPRNGState* pPrng = (IppsPRNGState*)(new Ipp8u[size]);
	ippsPRNGInit(160, pPrng);

	ippsPRNGSetSeed(BN(BigNumber(rand())), pPrng);

	ippsPRNGen_BN(BN(n), numSize, pPrng);
	while (LegendreSymbol(n, p) != BigNumber::MinusOne())
		ippsPRNGen_BN(BN(n), numSize, pPrng);

	BigNumber z(modPow(n, q, p));
	//2.Initialize
	BigNumber two(2);
	BigNumber t;
	BigNumber y(z);
	BigNumber r(e);
	BigNumber x(modPow(a, (q - BigNumber::One()) / BigNumber::Two(), p));
	BigNumber b(a*(x*x %p) % p);
	x = a*x %p;
	while (true){
		if (b % p == BigNumber::One())
			return x;
		//3.Find exponent
		BigNumber m(1);
		while (modPow(b, two.b_power(m), p) != BigNumber::One())
			m += 1;
		if (m == r)
			return BigNumber::Zero();
		//4.Reduce exponent
		t = modPow(y, two.b_power(r - m - BigNumber::One()), p);
		y = t*t;
		y %= p;
		r = m;
		x *= t;
		x %= p;
		b *= y;
		b %= p;
	}

}
