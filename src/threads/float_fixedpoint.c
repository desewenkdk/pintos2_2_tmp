/* 필요한 부동 소수점 연산만 따로 구현해 놓음 */
/* 32bit integer type으로 고정소수점 방식의 float형과 int를 동시에 표현한다. 따라서 return type은 모두 int가 되고, 실질적으로는 int형의 공간을 가지나 연산은 고정소수점 연산방식을 따르게 한다. 고정소수점 형식의 float의 비트 구성은 명세서에 나와있듯, 1bit-sign, 17bit-int, 14bit-fraction이다.*/

/*
priority = PRI_MAX - (recent_cpu / 4) - (nice * 2)

recent_cpu : Estimate of the CPU time the thread has used recently, init:0
-> (2 * load_avg ) / (2 * load_avg + 1 ) * recent_cpu + nice

nice: nice value of the thread, -20~20

load_avg : average of the number of thread in READY state, initial:0
-> (59/60) * load_avg + (1/60) * ready_threads 

ready_threads : number of thread in READY or RUNNING state 
(Except for idle thread)
*/

#define BIT_SHIFT_FOR_F 14

//same as integer arithmatic
int R_mul_I(int R, int I){
	return R * I;
}

int R_div_I(int R, int I){
	return R / I;
}

int R_add_R(int R1, int R2){
	return R1 + R2;
}

int R_sub_R(int R1, int R2){
	return R1 - R2;
}

//have to do shift operation(doing mult or div by 2^14) after calculation
//because we have to consider of "fraction" part.
int R_add_I(int R, int I){
	int c = I << BIT_SHIFT_FOR_F;
	return R + c;
}

int R_sub_I(int R, int I){
	int c = I << BIT_SHIFT_FOR_F;
	return R - I;
}

int I_sub_R(int I, int R){
	int c = I << BIT_SHIFT_FOR_F;
	return I - R;
}

int R_mul_R(int r1, int r2){
	int64_t rt = r1;//prevent overflow...
	rt = rt * r2;
	rt = rt >> BIT_SHIFT_FOR_F;
	return (int)rt;
}

int R_div_R(int r1, int r2){
	int64_t rt = r1; //prevent overflow..
	rt = rt << BIT_SHIFT_FOR_F;
	rt = rt / r2;
	return (int)rt;	
}
