#define DOCTEST_CONFIG_IMPLEMENT
#include <tests/doctest/doctest.hpp>

extern const bool UCI = false;



int main(int argc, char** argv) {
    doctest::Context ctx;

    ctx.setOption("abort-after", 1);
    ctx.setOption("no-exitcode", false);
    ctx.setOption("success", true);

    ctx.applyCommandLine(argc, argv);

    const int res = ctx.run();  // run test cases unless with --no-run

    if (ctx.shouldExit())  // query flags (and --exit) rely on this
        return res;        // propagate the result of the tests

    return res;
}
