#include <assert.h>
#include <string.h>

#include "server/wildmat.h"

#define EXPECT_TRUE(expr)  assert((expr) && #expr)
#define EXPECT_FALSE(expr) assert(!(expr) && #expr)

static void test_basic(void) {
    EXPECT_TRUE(wildmat("anything", "*"));
    EXPECT_TRUE(wildmat("", "*"));

    EXPECT_TRUE(wildmat("foo", "foo"));
    EXPECT_FALSE(wildmat("foobar", "foo"));
    EXPECT_FALSE(wildmat("foo", "foobar"));

    EXPECT_TRUE(wildmat("foo", "f?o"));
    EXPECT_FALSE(wildmat("fo", "f?o"));
}

static void test_classes_and_ranges(void) {
    EXPECT_TRUE(wildmat("cat", "[a-c]at"));
    EXPECT_TRUE(wildmat("bat", "[a-c]at"));
    EXPECT_FALSE(wildmat("dat", "[a-c]at"));

    EXPECT_TRUE(wildmat("z", "[^a-y]"));
    EXPECT_FALSE(wildmat("b", "[^a-y]"));
}

static void test_escape(void) {
    EXPECT_TRUE(wildmat("f*o", "f\\*o"));
    EXPECT_FALSE(wildmat("foo", "f\\*o"));
}

static void test_stars(void) {
    EXPECT_TRUE(wildmat("abcdxxxxx", "*a*b*c*d*"));
    EXPECT_FALSE(wildmat("abcxxxxx", "*a*b*c*d*"));
    EXPECT_FALSE(wildmat("abcxxxxx", "*a*b*c*d"));
}

int main(void) {
    test_basic();
    test_classes_and_ranges();
    test_escape();
    test_stars();
    return 0;
}

