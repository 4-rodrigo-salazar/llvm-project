//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// REQUIRES: host-can-create-symlinks
// UNSUPPORTED: c++03, c++11, c++14
// UNSUPPORTED: no-filesystem
// UNSUPPORTED: availability-filesystem-missing

// Android's fchmodat seems broken on various OS versions -- see D140183. This
// test probably passes on new-enough phones (not the emulator).
// XFAIL: LIBCXX-ANDROID-FIXME && target={{i686|x86_64}}-{{.+}}-android{{.*}}
// XFAIL: LIBCXX-ANDROID-FIXME && android-device-api={{21|22}}

// <filesystem>

// void permissions(const path& p, perms prms,
//                  perm_options opts = perm_options::replace);
// void permissions(const path& p, perms prms, std::error_code& ec) noexcept;
// void permissions(const path& p, perms prms, perm_options opts, std::error_code);

#include <filesystem>

#include "test_macros.h"
#include "filesystem_test_helper.h"
namespace fs = std::filesystem;
using namespace fs;

using PR = fs::perms;

static void test_signatures()
{
    const path p; ((void)p);
    const perms pr{}; ((void)pr);
    const perm_options opts{}; ((void)opts);
    std::error_code ec; ((void)ec);
    ASSERT_NOT_NOEXCEPT(fs::permissions(p, pr));
    ASSERT_NOT_NOEXCEPT(fs::permissions(p, pr, opts));
    ASSERT_NOEXCEPT(fs::permissions(p, pr, ec));
    LIBCPP_ASSERT_NOT_NOEXCEPT(fs::permissions(p, pr, opts, ec));
}

static void test_error_reporting()
{
    auto checkThrow = [](path const& f, fs::perms opts,
                         const std::error_code& ec)
    {
#ifndef TEST_HAS_NO_EXCEPTIONS
        try {
            fs::permissions(f, opts);
            return false;
        } catch (filesystem_error const& err) {
            return err.path1() == f
                && err.path2() == ""
                && err.code() == ec;
        }
#else
        ((void)f); ((void)opts); ((void)ec);
        return true;
#endif
    };

    scoped_test_env env;
    const path dne = env.make_env_path("dne");
    const path dne_sym = env.create_symlink(dne, "dne_sym");
    { // !exists
        std::error_code ec = GetTestEC();
        fs::permissions(dne, fs::perms{}, ec);
        assert(ec);
        assert(ec != GetTestEC());
        assert(checkThrow(dne, fs::perms{}, ec));
    }
    {
        std::error_code ec = GetTestEC();
        fs::permissions(dne_sym, fs::perms{}, ec);
        assert(ec);
        assert(ec != GetTestEC());
        assert(checkThrow(dne_sym, fs::perms{}, ec));
    }
}

static void basic_permissions_test()
{
    scoped_test_env env;
    const path file = env.create_file("file1", 42);
    const path dir = env.create_dir("dir1");
    const path file_for_sym = env.create_file("file2", 42);
    const path sym = env.create_symlink(file_for_sym, "sym");
    const perm_options AP = perm_options::add;
    const perm_options RP = perm_options::remove;
    const perm_options NF = perm_options::nofollow;
    struct TestCase {
      path p;
      perms set_perms;
      perms expected;
      perm_options opts;
      TestCase(path xp, perms xperms, perms xexpect,
               perm_options xopts = perm_options::replace)
          : p(xp), set_perms(xperms), expected(xexpect), opts(xopts) {}
    } cases[] = {
        // test file
        {file, perms::none, perms::none},
        {file, perms::owner_all, perms::owner_all},
        {file, perms::group_all, perms::owner_all | perms::group_all, AP},
        {file, perms::group_all, perms::owner_all, RP},
        // test directory
        {dir, perms::none, perms::none},
        {dir, perms::owner_all, perms::owner_all},
        {dir, perms::group_all, perms::owner_all | perms::group_all, AP},
        {dir, perms::group_all, perms::owner_all, RP},
        // test symlink without symlink_nofollow
        {sym, perms::none, perms::none},
        {sym, perms::owner_all, perms::owner_all},
        {sym, perms::group_all, perms::owner_all | perms::group_all, AP},
        {sym, perms::group_all, perms::owner_all, RP},
        // test non-symlink with symlink_nofollow. The last test on file/dir
        // will have set their permissions to perms::owner_all
        {file, perms::group_all, perms::owner_all | perms::group_all, AP | NF},
        {dir,  perms::group_all, perms::owner_all | perms::group_all, AP | NF}
    };
    for (auto const& TC : cases) {
        assert(status(TC.p).permissions() != TC.expected);
        {
          std::error_code ec = GetTestEC();
          permissions(TC.p, TC.set_perms, TC.opts, ec);
          assert(!ec);
          auto pp = status(TC.p).permissions();
          assert(pp == NormalizeExpectedPerms(TC.expected));
        }
        if (TC.opts == perm_options::replace) {
          std::error_code ec = GetTestEC();
          permissions(TC.p, TC.set_perms, ec);
          assert(!ec);
          auto pp = status(TC.p).permissions();
          assert(pp == NormalizeExpectedPerms(TC.expected));
        }
    }
}

#ifndef _WIN32
// This test isn't currently meaningful on Windows; the Windows file
// permissions visible via std::filesystem doesn't show any difference
// between owner/group/others.
static void test_no_resolve_symlink_on_symlink()
{
    scoped_test_env env;
    const path file = env.create_file("file", 42);
    const path sym = env.create_symlink(file, "sym");
    const auto file_perms = status(file).permissions();

    struct TestCase {
        perms set_perms;
        perms expected; // only expected on platform that support symlink perms.
        perm_options opts = perm_options::replace;
        TestCase(perms xperms, perms xexpect,
               perm_options xopts = perm_options::replace)
          : set_perms(xperms), expected(xexpect), opts(xopts) {}
    } cases[] = {
        {perms::owner_all, perms::owner_all},
        {perms::group_all, perms::owner_all | perms::group_all, perm_options::add},
        {perms::owner_all, perms::group_all, perm_options::remove},
    };
    for (auto const& TC : cases) {
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(_AIX)
        // On OS X symlink permissions are supported. We should get an empty
        // error code and the expected permissions.
        const auto expected_link_perms = TC.expected;
        std::error_code expected_ec;
#else
        // On linux symlink permissions are not supported. The error code should
        // be 'operation_not_supported' and the symlink permissions should be
        // unchanged.
        const auto expected_link_perms = symlink_status(sym).permissions();
        std::error_code expected_ec = std::make_error_code(std::errc::operation_not_supported);
#endif
        std::error_code ec = GetTestEC();
        permissions(sym, TC.set_perms, TC.opts | perm_options::nofollow, ec);
        if (expected_ec)
            assert(ErrorIs(ec, static_cast<std::errc>(expected_ec.value())));
        else
            assert(!ec);
        assert(status(file).permissions() == file_perms);
        assert(symlink_status(sym).permissions() == expected_link_perms);
    }
}
#endif // _WIN32

int main(int, char**) {
    test_signatures();
    test_error_reporting();
    basic_permissions_test();
#ifndef _WIN32
    test_no_resolve_symlink_on_symlink();
#endif
    return 0;
}
