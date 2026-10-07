int main(){
	int n;
	int *p;
	n =5;
	printf("%d\n",n);
	p =&n;
	printf("%d\n",*p);
	*p=1;
	printf("%d\n",n);
	printf("%d\n",*p);
	printf("%x %x\n",p,&n);
	printf("%x %x\n",p+1,&n+2 );
	return 0;
}