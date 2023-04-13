/* A very simple program to call a custom system call added to the Linux kernel*/

#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

int main()
{
	long int amma;
	long int *amma_p;

	amma_p = &amma;
	amma = syscall(545, amma_p); // Call the system call 545
	printf("System call sys_mm_userp returned %ld\n", amma);
	return 0;
}
