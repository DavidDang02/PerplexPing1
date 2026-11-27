// Matthew Erdman
// Tests the password hasher for expected value and edge cases.

#include "doctest.h"
#include "passwordhasher.h"

#include <string>

TEST_CASE("PasswordHasher: salt generation produces 32-character hex and distinct values") {
    const std::string s1 = PasswordHasher::generateSaltHex();
    const std::string s2 = PasswordHasher::generateSaltHex();

    CHECK(s1.size() == 32);
    CHECK(s2.size() == 32);
    CHECK(s1 != s2);
}

TEST_CASE("PasswordHasher: deterministic hash for identical input") {
    const std::string salt = PasswordHasher::generateSaltHex();
    const std::string h1 = PasswordHasher::hashPassword("mySecretPassword", salt, 100);
    const std::string h2 = PasswordHasher::hashPassword("mySecretPassword", salt, 100);

    CHECK(h1.size() == 64);
    CHECK(h1 == h2);
}

TEST_CASE("PasswordHasher: different salt or password produces distinct hash") {
    const std::string salt1 = PasswordHasher::generateSaltHex();
    const std::string salt2 = PasswordHasher::generateSaltHex();

    const std::string h1 = PasswordHasher::hashPassword("passwordA", salt1, 50);
    const std::string h2 = PasswordHasher::hashPassword("passwordB", salt1, 50);
    const std::string h3 = PasswordHasher::hashPassword("passwordA", salt2, 50);

    CHECK(h1 != h2);
    CHECK(h1 != h3);
}

TEST_CASE("PasswordHasher: verification expected values and edge cases") {
    const std::string salt = PasswordHasher::generateSaltHex();
    const std::string hash = PasswordHasher::hashPassword("testPass123", salt, 10);

    // Expected value
    CHECK(PasswordHasher::verifyPassword("testPass123", salt, 10, hash));

    // Wrong password
    CHECK_FALSE(PasswordHasher::verifyPassword("wrongPass", salt, 10, hash));

    // Wrong salt
    const std::string otherSalt = PasswordHasher::generateSaltHex();
    CHECK_FALSE(PasswordHasher::verifyPassword("testPass123", otherSalt, 10, hash));

    // Empty inputs
    CHECK_FALSE(PasswordHasher::verifyPassword("", salt, 10, hash));
    CHECK_FALSE(PasswordHasher::verifyPassword("testPass123", "", 10, hash));
    CHECK_FALSE(PasswordHasher::verifyPassword("testPass123", salt, 10, ""));
    CHECK_FALSE(PasswordHasher::verifyPassword("testPass123", salt, 0, hash));
}