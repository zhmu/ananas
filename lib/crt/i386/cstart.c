#define NULL 0L

extern int main(int argc, char** argv, char** envp);

int
cstart()
{
	return(main(0, NULL, NULL));
}
