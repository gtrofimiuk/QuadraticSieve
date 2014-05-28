#include "QuadraticSieve.h"
#include "wiedemann.h"
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
	BigNumber F(a);
	if (bit[size - 1][3] == '1')
		b = a;
	for (int i = 1; i < deg; i++){
		F *= F;
		F %= n;
		if (bit[size - 1 - i / 4][3 - i % 4] == '1'){
			b *= F;
			b %= n;
		}
	}
	return b;
}

BigNumber QuadraticSieve::LegendreSymbol(const BigNumber& a, const BigNumber& p){

	BigNumber r(modPow(a, (p - BigNumber::One()) / BigNumber::Two(), p));
	if (r > BigNumber::One())
		r -= p;
	return r;
}

std::pair<BigNumber, BigNumber> QuadraticSieve::doFactorization(){

	std::pair<BigNumber, BigNumber> divisors;
	cout << "bit size of " << N << " is " << N.BitSize() << endl;
	double nLg = N.b_ln();
	double t1 = exp(0.5*sqrt(nLg) * sqrt(log(nLg))); //TODO: experiment with it
	Ipp32u t = ceil(t1);
	t /= 8;  //Optimize this

	fbSize = t;

	cout << "First factor base size chosen as "<<fbSize << endl;

	Ipp32u len;
	if (N.BitSize() >= 190)
		len = t*ceill(sqrtl(sqrtl(t)));
	else if (N.BitSize() >= 50)
		len = t*ceill(sqrtl(t));  //TODO: optimize this bound
	else
		len = t*t;
	//Sieve of Eratosphenes
	std::vector<bool> arr(EratospheneSieve(len + 1));

	Ipp32u counter = 1;
	Ipp32u l = len-1;
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
	fbSize = Base.size();
	vector<pair<BigNumber, vector<Ipp32u>>> smooth(sieving());

	cout << "Sieving stage completed" << endl;
	SparseMatrix SM(getSparseMatrix(smooth), fbSize + 1);
	Wiedemann calc(SM);

	vector<bool> w(calc.getSolution());
	while (SM.isZero(w))
		w = calc.getSolution();

	bool isCompleted = false;
	while (!isCompleted){
		BigNumber x(1);
		for (uInt i = 0; i < fbSize + 1; ++i){
			if (w[i]){
				x *= smooth[i].first;
				//x %= N;
			}
		}
		x %= N;
		cout <<"x "<< x << endl;
		BigNumber y(1);
		vector<Ipp32u> L(fbSize,0);
		for (Ipp32u j = 0; j < fbSize; ++j){
			for (Ipp32u i = 0; i < fbSize + 1; ++i){
				if (w[i])
					L[j] += smooth[i].second[j];
			}
			L[j] /= 2;	
		}
		for (Ipp32u j = 0; j < fbSize; ++j){
			y *= Base[j].b_power(L[j]);
		}
		y %= N;
		cout << "y = " << y << endl;
		BigNumber d(N);
		IppStatus f1 = ippsGcd_BN(BN(x-y), BN(N), BN(d));
		cout << "f1 " << f1 << endl;
		if (d != N && d != BigNumber::One() && d != BigNumber::Zero()){
			cout << "Completed !!! " << endl;
			isCompleted = true;
			divisors.first = d;
			divisors.second = N / d;
		}
		else{
			cout << "d " << d << endl;
			w = calc.getSolution();
			while (SM.isZero(w))
				w = calc.getSolution();
		}
	}
	return divisors;
}

vector<pair<BigNumber, vector<Ipp32u>>> QuadraticSieve::sieving(){
	float epsilon = 6;

	Ipp32u decimal_size = (ceil((float)N.BitSize() / log2(10)));
	Ipp32s M;
	cout << "decimal length of number " << decimal_size << endl;
	//I'll optimize it

	BigNumber k = Base[fbSize - 1];
	vector<Ipp32u> v;
	k.num2vec(v);
	M = v[0];
	M *= 3;
	M /= 2;
	if (N.BitSize() > 210)
		M /= 4;
	cout << " M " << M << endl;

	//define pseudo random generators
	int ctxSize;
	int maxBitSize = ((N.BitSize()) / 2) - log2(M)+1;
	maxBitSize /= 2;
	ippsPrimeGetSize(maxBitSize, &ctxSize);
	IppsPrimeState* pPrimeG = (IppsPrimeState*)(new Ipp8u[ctxSize]);
	ippsPrimeInit(maxBitSize, pPrimeG);

	ippsPRNGGetSize(&ctxSize);
	IppsPRNGState* pRand = (IppsPRNGState*)(new Ipp8u[ctxSize]);
	ippsPRNGInit(160, pRand);
	srand(time(NULL));
	ippsPRNGSetSeed(BN(BigNumber(rand())), pRand);

	int size_TS;

	ippsPRNGGetSize(&size_TS);
	pPrng_TS = (IppsPRNGState*)(new Ipp8u[size_TS]);
	ippsPRNGInit(160, pPrng_TS);

	ippsPRNGSetSeed(BN(BigNumber(rand())), pPrng_TS);

	//fill array of log(p[i])
	vector<float> prime_log(Base.size()-1);

	auto b_end = Base.end();
	for (auto it = Base.begin() + 1; it != b_end; ++it){
		Ipp32u ind = it - Base.begin() -1;
		vector<Ipp32u> v;
		it->num2vec(v);
		prime_log[ind] = log(v[0]);
	}
	//result matrix
	vector<pair<BigNumber, vector<Ipp32u>>> result(fbSize + 1);
	for (auto& i : result){
		i.second = std::vector<Ipp32u>(fbSize+1);
	}

	BigNumber  D, h2, B2, primeMod, r1, r2, A2, q,InvA, bt,dt;

	//parallel control
	Ipp32u lenM = 2 * M + 1;
	numThreads = 4;
	omp_set_num_threads(numThreads);
	float** sieve = new float*[numThreads];
	for (int i = 0; i < numThreads; ++i){
		sieve[i] = new float[lenM];
	}
	for (int i = 0; i < numThreads; ++i){
		for (Ipp32u j = 0; j < lenM; ++j){
			sieve[i][j] = 0;
		}
	}
	Ipp32u* thread_begin_index = new Ipp32u[numThreads];
	Ipp32u* thread_bound = new Ipp32u[numThreads];
	thread_begin_index[0] = 0;
	Ipp32u quo = (fbSize + 1) / numThreads;
	for (int i = 1; i < numThreads; ++i){
		thread_begin_index[i] = thread_begin_index[i - 1] + quo;
	}
	for (int i = 0; i < numThreads - 1; ++i){
		thread_bound[i] = thread_begin_index[i + 1] - thread_begin_index[i];
	}
	thread_bound[numThreads - 1] = (fbSize + 1) - thread_begin_index[numThreads - 1];
	Ipp32u* thread_counter = new Ipp32u[numThreads];
	for (int i = 0; i < numThreads; ++i){
		thread_counter[i] = 0;
	}
	
	BigNumber A, B, C, m_TS, b_TS, ts_1,ts_2,ts_3,ts_4,ts_5,ts_6,_ts_7,ts_8, N_TS;
	Ipp32u ts_9;

#pragma omp parallel for private(A,B,C,D, h2, B2, primeMod, r1, r2, A2, q,InvA, bt,dt, m_TS, b_TS,ts_1,ts_2,ts_3,ts_4,ts_5,ts_6,_ts_7,ts_8,ts_9, N_TS)
	for (int thread_id = 0; thread_id < numThreads; thread_id++){
		N_TS = N;
		while (thread_counter[thread_id] < thread_bound[thread_id]){
			//generate coefficients
			ippsPrimeGen_BN(q, maxBitSize, nTrials, pPrimeG, ippsPRNGen, pRand);
			while (!(q.isPrime(nTrials) && LegendreSymbol(N_TS, q) == BigNumber::One())){
				ippsPrimeGen_BN(q, maxBitSize, nTrials, pPrimeG, ippsPRNGen, pRand);
			}
			cout << "thread_id " << thread_id << " counter = " << thread_counter[thread_id] << endl;
			//cout << "Switch polynom" << endl;
			A = q*q;

			//now use Hensel�s Lemma
			BigNumber B1;

			B1 = Tonelli_Shanks(N_TS, q, m_TS, b_TS, ts_1, ts_2, ts_3, ts_4, ts_5, ts_6, _ts_7, ts_8, ts_9);
			BigNumber invF(q);

			ippsModInv_BN(BN(BigNumber::Two()*B1 %q), BN(q), BN(invF));

			BigNumber t = ((N - B1*B1) / q)*invF % q;
			B = (B1 + q*t) % A;
			C = (B*B - N) / A;
			BigNumber Q1(N);
			ippsModInv_BN(BN(q), BN(N), BN(Q1));
			//Q(x) = Ax^2 + 2*B*x +C
			Ipp32u nb = Base.size();
			for (Ipp32u i = 1; i < nb; ++i){
				//primeMod = BigNumber(*it);
				primeMod = Base[i];
				A2 = BigNumber::Two()*A;
				InvA = primeMod;
				ippsModInv_BN(BN(A2 % primeMod), BN(primeMod), BN(InvA));
				D = Tonelli_Shanks(N_TS, primeMod, m_TS, b_TS, ts_1, ts_2, ts_3, ts_4, ts_5, ts_6, _ts_7, ts_8, ts_9);
				bt = B;
				bt *= BigNumber::MinusOne(); bt *= BigNumber::Two();
				dt = D; dt *= BigNumber::Two();
				r1 = bt; r2 = bt;
				r1 += dt; r2 -= dt;
				r1 *= InvA; r2 *= InvA;
				r1 %= primeMod;
				r2 %= primeMod;

				Ipp32u ind = i - 1;
				vector<Ipp32u> v, v1, v2;
				r1.num2vec(v);
				primeMod.num2vec(v1);
				r2.num2vec(v2);
				Ipp32s stepR1 = v[0];
				Ipp32s primePower = v1[0];
				Ipp32s stepR2 = v2[0];
				if (primePower <= M){
					//sieving by first root
					while (stepR1 <= M){
						sieve[thread_id][stepR1 + M] -= prime_log[ind];
						stepR1 += primePower;
					}
					stepR1 = v[0] + M - primePower;
					while (stepR1 >= 0){
						sieve[thread_id][stepR1] -= prime_log[ind];
						stepR1 -= primePower;
					}
					//sieving by second root
					while (stepR2 <= M){
						sieve[thread_id][stepR2 + M] -= prime_log[ind];
						stepR2 += primePower;
					}
					stepR2 = v2[0] + M - primePower;
					while (stepR2 >= 0){
						sieve[thread_id][stepR2] -= prime_log[ind];
						stepR2 -= primePower;
					}
				}
			} //end for one prime from factor base

			//tbb::parallel_for(tbb::blocked_range<Ipp32s>(0, 2 * M + 1),
			//	[=](const tbb::blocked_range<Ipp32s>& r) -> void{
			//	for (Ipp32s i = r.begin(); i != r.end(); ++i){
			//		sieve[0][i] += Q(BigNumber(i - M)).b_ln();
			//	}
			//});

			for (Ipp32s i = 0; i < 2 * M + 1; ++i){
				sieve[thread_id][i] += Q(BigNumber(i - M),A,B,C).b_ln();
			}

			BigNumber R, Qx;
			for (Ipp32s i = 0; i < 2 * M + 1 & thread_counter[thread_id] < thread_bound[thread_id]; ++i){
				if (abs(sieve[thread_id][i]) <= epsilon){
					Ipp32s xg = i - M;
					BigNumber x(xg);
					Qx = Q(x,A,B,C);
					Ipp32u res_index = thread_begin_index[thread_id] + thread_counter[thread_id];
					if (Qx < BigNumber::Zero()){
						Qx *= BigNumber::MinusOne();
					}
					//Trial division stage
					if (Qx > BigNumber::Zero()){
						for (auto jt = Base.begin(); jt != Base.end(); ++jt){
							Ipp32u ind = jt - Base.begin();
							Ipp32u deg = 0;
							BigNumber ttt(*jt);
							//Silverman point us to one improvement : R = x mod pi must be equals one of two roots
							//I'll do it later
							while (Qx % ttt == BigNumber::Zero()){
								++deg;
								Qx /= ttt;
							}
							result[res_index].second[ind] = deg;
						}
						if (Qx == BigNumber::One()){
							result[res_index].first = (A*x + B) * Q1; //???
							++thread_counter[thread_id];
						}
						else{
							for (auto& kk : result[res_index].second)  //I want to zero all degrees
								kk = 0;
						}
					}
				}
			}

			for (int i = 0; i < 2 * M + 1; ++i)
				sieve[thread_id][i] = 0;
		}
	}
	delete pPrng_TS;
	return std::move(result);
}

BigNumber QuadraticSieve::Q(const BigNumber& x, BigNumber& A, BigNumber& B, BigNumber& C){
	BigNumber r(A);
	BigNumber c(B);
	c *= BigNumber::Two(); c *= x;
	r *= x; r *= x; r += c; r += C;
	return std::move(r);
}

//Algorithm return x -> x^2 = a (mod p) or return false
BigNumber QuadraticSieve::Tonelli_Shanks(const BigNumber& a,const BigNumber& p, BigNumber&m, BigNumber&b,
	BigNumber& q_TS, BigNumber& n_TS, BigNumber& z_TS, BigNumber&  two_TS, BigNumber& t_TS, BigNumber& y_TS, BigNumber& r_TS, BigNumber& x_TS, Ipp32u e_TS){
	e_TS = 0;
	two_TS = BigNumber(2);
	q_TS = p - BigNumber::One();
	while (q_TS.IsEven()){
		++e_TS;
		q_TS /= BigNumber::Two();
	}
	//1.Find generator

	ippsPRNGen_BN(BN(n_TS), numSize_TS, pPrng_TS);
	while (LegendreSymbol(n_TS, p) != BigNumber::MinusOne())
		ippsPRNGen_BN(BN(n_TS), numSize_TS, pPrng_TS);
	z_TS = modPow(n_TS, q_TS, p);
	//2.Initialize
	y_TS = z_TS;
	r_TS = e_TS;
	x_TS = modPow(a, (q_TS - BigNumber::One()) / two_TS, p);
	b = x_TS; b *= x_TS; b %= p; b *= a; b %= p;
	x_TS = a*x_TS %p;
	while (true){
		if (b % p == BigNumber::One()){
			return std::move(x_TS);
		}
		//3.Find exponent
		m = BigNumber::One();
		while (modPow(b, two_TS.b_power(m), p) != BigNumber::One())
			m += BigNumber::One();
		if (m == r_TS){
			return BigNumber::Zero();
		}
		//4.Reduce exponent
		t_TS = modPow(y_TS, two_TS.b_power(r_TS - m - BigNumber::One()), p);
		y_TS = t_TS;
		y_TS *= t_TS;
		y_TS %= p;
		r_TS = m;
		x_TS *= t_TS;
		x_TS %= p;
		b *= y_TS;
		b %= p;
	}
}

vector<unsigned int> QuadraticSieve::getSparseMatrix(vPair& v){
	unsigned int len = v.size();
	vector<unsigned int> r;
	for (unsigned int i = 0; i < len; ++i){
		r.push_back(0);
		const auto ind = r.end() - r.begin() - 1;
		for (unsigned int j = 0; j < len; ++j){
			if (v[j].second[i] % 2 != 0){
				r.push_back(j);
				r[ind]++;
			}
		}
	}
	return std::move(r);
}