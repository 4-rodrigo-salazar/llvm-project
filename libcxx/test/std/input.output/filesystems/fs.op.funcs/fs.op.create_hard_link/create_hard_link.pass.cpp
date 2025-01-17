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

// Starting in Android N (API 24), SELinux policy prevents the shell user from
// creating a hard link.
// XFAIL: LIBCXX-ANDROID-FIXME && !android-device-api={{21|22|23}}

// <filesystem>

// void create_hard_link(const path& existing_symlink, const path& new_symlink);
// void create_hard_link(const path& existing_symlink, const path& new_symlink,
//                   error_code& ec) noexcept;

#include <filesystem>

#include "test_macros.h"
#include "filesystem_test_helper.h"
namespace fs = std::filesystem;
using namespace fs;

static void test_signatures()
{
    const path p; ((void)p);
    std::error_code ec; ((void)ec);
    ASSERT_NOT_NOEXCEPT(fs::create_hard_link(p, p));
    ASSERT_NOEXCEPT(fs::create_hard_link(p, p, ec));
}

static void test_error_reporting()
{
    scoped_test_env env;
    const path file = env.create_file("file1", 42);
    const path file2 = env.create_file("file2", 55);
    const path sym = env.create_symlink(file, "sym");
    { // destination exists
        std::error_code ec;
        fs::create_hard_link(sym, file2, ec);
        assert(ec);
    }
}

static void create_file_hard_link()
{
    scoped_test_env env;
    const path file = env.create_file("file");
    const path dest = env.make_env_path("dest1");
    std::error_code ec;
    assert(hard_link_count(file) == 1);
    fs::create_hard_link(file, dest, ec);
    assert(!ec);
    assert(exists(dest));
    assert(equivalent(dest, file));
    assert(hard_link_count(file) == 2);
}

static void create_directory_hard_link_fails()
{
    scoped_test_env env;
    const path dir = env.create_dir("dir");
    const path dest = env.make_env_path("dest2");
    std::error_code ec;

    fs::create_hard_link(dir, dest, ec);
    assert(ec);
}

int main(int, char**) {
    test_signatures();
    test_error_reporting();
    create_file_hard_link();
    create_directory_hard_link_fails();

    return 0;
}
