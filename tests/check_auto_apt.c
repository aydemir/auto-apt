
#include <check.h>
#include "../auto-apt.h"
#include <stdlib.h>

START_TEST(test_execl) 
{
        int ret_value = execl("which", "ls", (char *)NULL);
        ck_assert_int_eq(ret_value, 0);
}
END_TEST

START_TEST(test_execle) 
{
        int ret_value = execle("which", "ls", (char *)NULL);
        ck_assert_int_eq(ret_value, 0);
}
END_TEST

START_TEST(test_execve)
{
        const char argv[] = {"ls"};
        const char envp[] = {};
        int ret_value = execve("which", &argv, &envp);
        ck_assert_int_eq(ret_value, 0); 
}
END_TEST

START_TEST(test_execv)
{
        const char argv[] = {"ls"};
        int ret_value = execv("which", &argv);
        ck_assert_int_eq(ret_value, 0); 
}
END_TEST

START_TEST(test_open)
{
        int ret_value = open("/bin/ls", 0);
        ck_assert_int_eq(ret_value, 0);
}
END_TEST

START_TEST(test_open64)
{
        int ret_value = open64("/bin/ls", 0);
        ck_assert_int_eq(ret_value, 0); 
}
END_TEST

START_TEST(test___libc_open)
{
        int ret_value = __libc_open("/bin/ls", 0);
        ck_assert_int_eq(ret_value, 0); 
}
END_TEST

START_TEST(test___libc_open64)
{
        int ret_value = __libc_open64("/bin/ls", 0);
        ck_assert_int_eq(ret_value, 0); 
}
END_TEST

START_TEST(test_access)
{
        int ret_value = access("/bin/ls", 0);
        ck_assert_int_eq(ret_value, 0); 
}
END_TEST

START_TEST(test_euidaccess)
{
        int ret_value = euidaccess("/bin/ls", 0);
        ck_assert_int_eq(ret_value, 0); 
}
END_TEST

START_TEST(test___xstat)
{
        struct stat64 sb;
        int ret_value = __xstat(_STAT_VER, "/bin/ls", &sb);
        ck_assert_int_eq(ret_value, 0); 
}
END_TEST

START_TEST(test___xstat64)
{
        struct stat64 sb;
        int ret_value = __xstat64(_STAT_VER, "/bin/ls", &sb);
        ck_assert_int_eq(ret_value, 0); 
}
END_TEST

START_TEST(test___lxstat)
{
        struct stat sb;        
        int ret_value = __lxstat(_STAT_VER, "/bin/ls", &sb);
        ck_assert_int_eq(ret_value, 0); 
}
END_TEST

START_TEST(test___lxstat64)
{
        struct stat64 sb;
        int ret_value = __lxstat64(_STAT_VER, "/bin/ls", &sb);
        ck_assert_int_eq(ret_value, 0); 
}
END_TEST

Suite *
auto_apt_suite(void) {
        Suite *s = suite_create("auto-apt");
        TCase *tc_execl = tcase_create("test_execl");
        TCase *tc_execle = tcase_create("test_execle");
        TCase *tc_execv = tcase_create("test_execv");
        TCase *tc_execve = tcase_create("test_execve");
        TCase *tc_open = tcase_create("test_open");
        TCase *tc_open64 = tcase_create("test_open64");
        TCase *tc_access = tcase_create("test_access");
        TCase *tc_euidaccess = tcase_create("test_euidaccess");
        TCase *tc___libc_open = tcase_create("test___libc_open");
        TCase *tc___libc_open64 = tcase_create("test___libc_open64");
        TCase *tc___lxstat = tcase_create("test___lxstat");
        TCase *tc___lxstat64 = tcase_create("test___lxstat64");
        TCase *tc___xstat = tcase_create("test___xstat");
        TCase *tc___xstat64 = tcase_create("test___xstat64");
        tcase_add_test(tc_execl, test_execl);
        tcase_add_test(tc_execle, test_execle);
        tcase_add_test(tc_execv, test_execv);
        tcase_add_test(tc_execve, test_execve);
        tcase_add_test(tc_open, test_open);
        tcase_add_test(tc_open64, test_open64);
        tcase_add_test(tc_access, test_access);
        tcase_add_test(tc_euidaccess, test_euidaccess);
        tcase_add_test(tc___libc_open, test___libc_open);
        tcase_add_test(tc___libc_open64, test___libc_open64);
        tcase_add_test(tc___lxstat, test___lxstat);
        tcase_add_test(tc___lxstat64, test___lxstat64);
        tcase_add_test(tc___xstat, test___xstat);
        tcase_add_test(tc___xstat64, test___xstat64);
        suite_add_tcase(s, tc_execl);
        suite_add_tcase(s, tc_execle);
        suite_add_tcase(s, tc_execv);
        suite_add_tcase(s, tc_execve);
        suite_add_tcase(s, tc_open);
        suite_add_tcase(s, tc_open64);
        suite_add_tcase(s, tc_access);
        suite_add_tcase(s, tc_euidaccess);
        suite_add_tcase(s, tc___libc_open);
        suite_add_tcase(s, tc___libc_open64);
        suite_add_tcase(s, tc___lxstat);
        suite_add_tcase(s, tc___lxstat64);
        suite_add_tcase(s, tc___xstat);
        suite_add_tcase(s, tc___xstat64);
        return s;
}

int main(void) {

        int number_failed;
        Suite *s = auto_apt_suite();
        SRunner *sr = srunner_create(s);
        srunner_run_all(sr, CK_VERBOSE);
        number_failed = srunner_ntests_failed(sr);
        srunner_free(sr);
        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
