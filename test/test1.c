int foo(int a, int b, int c, int d, int e, 
		int f, int g, int h, int i, int j, 
		int k, int l, int m, int n, int o, 
		int p, int q, int r, int s, int t, 
		int u, int v, int w, int x, int y, 
		int z) {
	int t1 = a + b;
	int m1 = a + c;
	int l1 = b + d;
	int g1 = a + e;
	int r1 = c + f;
	int z1 = a + f;
	int k1 = t1 + m1 + l1 + g1 + r1 + z1 + a + d ;
	return k;
}

int bar(int a, int b, int c, int d, int e, 
		int f, int g, int h, int i, int j, 
		int k, int l, int m, int n, int o, 
		int p, int q, int r, int s, int t, 
		int u, int v, int w, int x, int y, 
		int z) {
	int o1 = foo(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z);
	int p1 = o1 + g + i;
	int s1 = p + k + o1;
	return s;
}

int main() {
	bar(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26);
}