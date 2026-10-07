void echanger(int *adr_a, int *adr_b)
{
int tmp;
tmp = *adr_a;
*adr_a = *adr_b;
*adr_b = tmp;
}

int main()
{
int a=3,b=258;
echanger(&a, &b);
printf("%d\t%d\n",a,b);
return 0;
}