#include <stdio.h>

#include <include/core.h>
#include <include/init.h>

#include "test_case.h"

struct test_case
{
	char *name;
	char *desc;
	int (*func)(int argc, char **argv);
};

struct test_case cases[] = {
	{"list", "", test_list},
	{"configs", "", test_configs},
	{"workqueue", "", test_workqueue},
	{"timer", "", test_timer},
};


int main(int argc, char **argv)
{
	int i;
	int ret;
	int result = 0;
	struct test_case *tcase;

    common_init();

	for(i=0; i<ARRAY_SIZE(cases); i++) {
		tcase = cases + i;
		if(tcase->func != NULL) {
			printf("\n\n==========================================================\n");
			printf("test case [%d]: %s\n", i, tcase->name);
			//printf("%s\n", tcase->desc);

			ret = tcase->func(argc - 1, ++argv);
			if(ret) {
				result++;
			}
		}
	}

	printf("\n\n=======================end================================\n");
	printf("test finish. failed count:%d\n", result);
    return 0;
}
